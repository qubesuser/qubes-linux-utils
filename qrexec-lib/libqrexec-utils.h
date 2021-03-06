/*
 * The Qubes OS Project, http://www.qubes-os.org
 *
 * Copyright (C) 2010  Rafal Wojtczuk  <rafal@invisiblethingslab.com>
 * Copyright (C) 2013  Marek Marczykowski  <marmarek@invisiblethingslab.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <sys/select.h>
#include <libvchan.h>

struct buffer {
	char *data;
	int buflen;
};

typedef void (do_exec_t)(const char *);
void register_exec_func(do_exec_t *func);

void buffer_init(struct buffer *b);
void buffer_free(struct buffer *b);
void buffer_append(struct buffer *b, const char *data, int len);
void buffer_remove(struct buffer *b, int len);
int buffer_len(struct buffer *b);
void *buffer_data(struct buffer *b);


void do_fork_exec(const char *cmdline, int *pid, int *stdin_fd, int *stdout_fd,
		  int *stderr_fd);
void wait_for_vchan_or_argfd(libvchan_t *vchan, int max, fd_set * rdset, fd_set * wrset);
int read_vchan_all(libvchan_t *vchan, void *data, size_t size);
int write_vchan_all(libvchan_t *vchan, const void *data, size_t size);
int read_all(int fd, void *buf, int size);
int write_all(int fd, const void *buf, int size);
void fix_fds(int fdin, int fdout, int fderr);
void set_nonblock(int fd);
void set_block(int fd);

int get_server_socket(const char *);
int do_accept(int s);

void set_nonblock(int fd);
