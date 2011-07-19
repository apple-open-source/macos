/*
 * Copyright (c) 1995,1999 Theo de Raadt.  All rights reserved.
 * Copyright (c) 2001-2002 Damien Miller.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <X11/Xos.h>
#ifndef X_NO_SYS_UN
# include	<sys/un.h>
#endif
#include <netinet/in.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "dm_auth.h"
#include "dm_error.h"

#ifndef INADDR_LOOPBACK
# define INADDR_LOOPBACK 0x7F000001U
#endif

static ssize_t atomicio(ssize_t (*)(int, void *, size_t), int, void *, size_t);

#ifndef offsetof
# define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

/*
 * Collect 'len' bytes of entropy into 'buf' from PRNGD/EGD daemon
 * listening either on 'tcp_port', or via Unix domain socket at *
 * 'socket_path'.
 * Either a non-zero tcp_port or a non-null socket_path must be
 * supplied.
 * Returns 0 on success, -1 on error
 */
int
get_prngd_bytes(char *buf, int len,
    unsigned short tcp_port, char *socket_path)
{
	int fd, addr_len, rval, errors;
	char msg[2];
	struct sockaddr *addr;
	struct sockaddr_in addr_in;
	struct sockaddr_un addr_un;
	int af;
	void (*old_sigpipe)(int);

	/* Sanity checks */
	if (socket_path == NULL && tcp_port == 0) {
		LogError("get_random_prngd: "
		    "You must specify a port or a socket\n");
		return -1;
	}
	if (socket_path != NULL &&
	    strlen(socket_path) >= sizeof(addr_un.sun_path)) {
		LogError("get_random_prngd: Random pool path is too long\n");
		return -1;
	}
	if (len > 255) {
		LogError("get_random_prngd: "
		    "Too many bytes to read from PRNGD\n");
		return -1;
	}

	memset(&addr_in, '\0', sizeof(addr));

	if (tcp_port != 0) {
		af = addr_in.sin_family = AF_INET;
		addr_in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		addr_in.sin_port = htons(tcp_port);
		addr_len = sizeof(addr_in);
		addr = (struct sockaddr *)&addr_in;
	} else {
		af = addr_un.sun_family = AF_UNIX;
		strncpy(addr_un.sun_path, socket_path,
		    sizeof(addr_un.sun_path));
		addr_len = offsetof(struct sockaddr_un, sun_path) +
		    strlen(socket_path) + 1;
		addr = (struct sockaddr *)&addr_un;
	}

	old_sigpipe = signal(SIGPIPE, SIG_IGN);

	errors = 0;
	rval = -1;
reopen:
	fd = socket(af, SOCK_STREAM, 0);
	if (fd == -1) {
		LogInfo("Couldn't create socket: %s\n", strerror(errno));
		goto done;
	}

	if (connect(fd, (struct sockaddr*)addr, addr_len) == -1) {
		if (af == AF_INET) {
			LogInfo("Couldn't connect to PRNGD port %d: %s\n",
			    tcp_port, strerror(errno));
		} else {
			LogInfo("Couldn't connect to PRNGD socket"
			    " \"%s\": %s\n",
			    addr_un.sun_path, strerror(errno));
		}
		goto done;
	}

	/* Send blocking read request to PRNGD */
	msg[0] = 0x02;
	msg[1] = len;

	if (atomicio(write, fd, msg, sizeof(msg)) != sizeof(msg)) {
		if (errno == EPIPE && errors < 10) {
			close(fd);
			errors++;
			goto reopen;
		}
		LogInfo("Couldn't write to PRNGD socket: %s\n",
		    strerror(errno));
		goto done;
	}

	if (atomicio(read, fd, buf, len) != len) {
		if (errno == EPIPE && errors < 10) {
			close(fd);
			errors++;
			goto reopen;
		}
		LogInfo("Couldn't read from PRNGD socket: %s\n",
		    strerror(errno));
		goto done;
	}

	rval = 0;
done:
	signal(SIGPIPE, old_sigpipe);
	if (fd != -1)
		close(fd);
	return rval;
}

/*
 * ensure all of data on socket comes through. f==read || f==write
 */
static ssize_t
atomicio(ssize_t (*f)(int, void *, size_t), int fd, void *_s, size_t n)
{
	char *s = _s;
	ssize_t res, pos = 0;

	while (n > pos) {
		res = (f) (fd, s + pos, n - pos);
		switch (res) {
		case -1:
#ifdef EWOULDBLOCK
			if (errno == EINTR || errno == EAGAIN
			    || errno == EWOULDBLOCK)
#else
			if (errno == EINTR || errno == EAGAIN)
#endif
				continue;
		case 0:
			return (res);
		default:
			pos += res;
		}
	}
	return (pos);
}
