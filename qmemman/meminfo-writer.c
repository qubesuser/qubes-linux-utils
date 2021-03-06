#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef USE_XENSTORE_H /* Xen >= 4.2 */
#include <xenstore.h>
#else
#include <xs.h>
#endif
#include <syslog.h>
#include <string.h>
#include <signal.h>

long prev_used_mem;
int used_mem_change_threshold;
int delay;
int usr1_received;

const char *parse(const char *buf)
{
	const char *ptr = buf;
	static char outbuf[4096];
	int val;
	int len;
	int ret;
	int MemTotal = 0, MemFree = 0, Buffers = 0, Cached = 0, SwapTotal =
	    0, SwapFree = 0;
	unsigned long long key;
	long used_mem, used_mem_diff;
	int nitems = 0;

	while (nitems != (1<<6)-1 || !*ptr) {
		ret = sscanf(ptr, "%*s %d kB\n%n", &val, &len);
		if (ret < 1 || len < (int)sizeof (unsigned long long)) {
			ptr += len;
			continue;
		}
		key = *(unsigned long long *) ptr;
		if (key == *(unsigned long long *) "MemTotal:") {
			MemTotal = val;
			nitems |= 1;
		} else if (key == *(unsigned long long *) "MemFree:") {
			MemFree = val;
			nitems |= 2;
		} else if (key == *(unsigned long long *) "Buffers:") {
			Buffers = val;
			nitems |= 4;
		} else if (key == *(unsigned long long *) "Cached:  ") {
			Cached = val;
			nitems |= 8;
		} else if (key == *(unsigned long long *) "SwapTotal:") {
			SwapTotal = val;
			nitems |= 16;
		} else if (key == *(unsigned long long *) "SwapFree:") {
			SwapFree = val;
			nitems |= 32;
		}

		ptr += len;
	}

	used_mem =
	    MemTotal - Buffers - Cached - MemFree + SwapTotal - SwapFree;
	if (used_mem < 0)
		return NULL;

	used_mem_diff = used_mem - prev_used_mem;
	if (used_mem_diff < 0)
		used_mem_diff = -used_mem_diff;
	if (used_mem_diff > used_mem_change_threshold
		|| prev_used_mem == 0
	    || (used_mem > prev_used_mem && used_mem / 10 > (MemTotal+12) / 13
		&& used_mem_diff > used_mem_change_threshold/2)) {
		prev_used_mem = used_mem;
		sprintf(outbuf,
			"MemTotal: %d kB\nMemFree: %d kB\nBuffers: %d kB\nCached: %d kB\n"
			"SwapTotal: %d kB\nSwapFree: %d kB\n", MemTotal,
			MemFree, Buffers, Cached, SwapTotal, SwapFree);
		return outbuf;
	}
	return NULL;
}

void usage(void)
{
	fprintf(stderr,
		"usage: meminfo_writer threshold_in_kb delay_in_us [pidfile]\n");
	fprintf(stderr, "  When pidfile set, meminfo-writer will:\n");
    fprintf(stderr, "   - fork into background\n");
	fprintf(stderr, "   - wait for SIGUSR1 (in background) before starting main work\n");
	exit(1);
}

void send_to_qmemman(struct xs_handle *xs, const char *data)
{
	if (!xs_write(xs, XBT_NULL, "memory/meminfo", data, strlen(data))) {
		syslog(LOG_DAEMON | LOG_ERR, "error writing xenstore ?");
		exit(1);
	}
}

void usr1_handler(int sig __attribute__((__unused__))) {
	usr1_received = 1;
}

int main(int argc, char **argv)
{
	char buf[4096];
	int n;
	const char *meminfo_data;
	int fd;
	struct xs_handle *xs;

	if (argc != 3 && argc != 4)
		usage();
	used_mem_change_threshold = atoi(argv[1]);
	delay = atoi(argv[2]);
	if (used_mem_change_threshold <= 0 || delay <= 0)
		usage();

	if (argc == 4) {
		pid_t pid;
		sigset_t mask, oldmask;

		switch (pid = fork()) {
			case -1:
				perror("fork");
				exit(1);
			case 0:
				sigemptyset (&mask); 
				sigaddset (&mask, SIGUSR1);
				/* Wait for a signal to arrive. */
				sigprocmask (SIG_BLOCK, &mask, &oldmask);
				usr1_received = 0;
				signal(SIGUSR1, usr1_handler);
				while (!usr1_received)
					  sigsuspend (&oldmask);
				sigprocmask (SIG_UNBLOCK, &mask, NULL);
				break;
			default:
				fd = open(argv[3], O_CREAT | O_TRUNC | O_WRONLY, 0666);
				if (fd < 0) {
					perror("open pidfile");
					kill(pid,9);
					exit(1);
				}
				n = sprintf(buf, "%d\n", pid);
				if (write(fd, buf, n) != n) {
					perror("write pid");
					kill(pid,9);
					exit(1);
				}
				close(fd);
				exit(0);
		}
	}

	fd = open("/proc/meminfo", O_RDONLY);
	if (fd < 0) {
		perror("open meminfo");
		exit(1);
	}
	xs = xs_domain_open();
	if (!xs) {
		perror("xs_domain_open");
		exit(1);
	}
	if (argc == 3) {
		/* if not waiting for signal, fork after first info written to xenstore */
		n = pread(fd, buf, sizeof(buf)-1, 0);
		if (n < 0) {
			perror("pread");
			exit(1);
		}
		buf[n] = 0;
		meminfo_data = parse(buf);
		if (meminfo_data)
			send_to_qmemman(xs, meminfo_data);
		n = fork();
		if (n < 0) {
			perror("fork");
			exit(1);
		}
		if (n > 0)
			exit(0);
		usleep(delay);
	}

	for (;;) {
		n = pread(fd, buf, sizeof(buf)-1, 0);
		if (n < 0) {
			perror("pread");
			exit(1);
		}
		buf[n] = 0;
		meminfo_data = parse(buf);
		if (meminfo_data)
			send_to_qmemman(xs, meminfo_data);
		usleep(delay);
	}
}
