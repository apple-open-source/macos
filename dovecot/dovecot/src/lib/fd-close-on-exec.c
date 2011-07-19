/* Copyright (c) 2002-2011 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "network.h"
#include "fd-close-on-exec.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/un.h>

void fd_close_on_exec(int fd, bool set)
{
	int flags;

	flags = fcntl(fd, F_GETFD, 0);
	if (flags < 0)
		i_fatal("fcntl(F_GETFD, %d) failed: %m", fd);

	flags = set ? (flags | FD_CLOEXEC) : (flags & ~FD_CLOEXEC);
	if (fcntl(fd, F_SETFD, flags) < 0)
		i_fatal("fcntl(F_SETFD, %d) failed: %m", fd);
}

void fd_debug_verify_leaks(int first_fd, int last_fd)
{
	struct ip_addr addr, raddr;
	unsigned int port, rport;
	struct stat st;
	int old_errno;
	unsigned int leaked = 0;				/* APPLE */

	for (; first_fd <= last_fd; first_fd++) {
		if (fcntl(first_fd, F_GETFD, 0) == -1 && errno == EBADF)
			continue;

		old_errno = errno;

		if (net_getsockname(first_fd, &addr, &port) == 0) {
			if (addr.family == AF_UNIX) {
				struct sockaddr_un sa;

				socklen_t socklen = sizeof(sa);

				if (getsockname(first_fd, (void *)&sa,
						&socklen) < 0)
					sa.sun_path[0] = '\0';

				/* APPLE */
				++leaked;
				i_error("Leaked UNIX socket fd %d: %s",
					first_fd, sa.sun_path);
				continue;
			}

			if (net_getpeername(first_fd, &raddr, &rport) < 0) {
				memset(&raddr, 0, sizeof(raddr));
				rport = 0;
			}
			/* APPLE */
			++leaked;
			i_error("Leaked socket fd %d: %s:%u -> %s:%u",
				first_fd, net_ip2addr(&addr), port,
				net_ip2addr(&raddr), rport);
			continue;
		}

		if (fstat(first_fd, &st) == 0) {
#ifdef __APPLE__
			/* OSX workaround: gettimeofday() calls shm_open()
			   internally and the fd won't get closed on exec.
			   We'll just skip all ino/dev=0 files and hope they
			   weren't anything else. */
			if (st.st_ino == 0 && st.st_dev == 0)
				continue;
#endif
#ifdef HAVE_SYS_SYSMACROS_H
			/* APPLE */
			++leaked;
			i_error("Leaked file fd %d: dev %s.%s inode %s",
				first_fd, dec2str(major(st.st_dev)),
				dec2str(minor(st.st_dev)), dec2str(st.st_ino));
			continue;
#else
			/* APPLE */
			++leaked;
			i_error("Leaked file fd %d: dev %s inode %s",
				first_fd, dec2str(st.st_dev),
				dec2str(st.st_ino));
			continue;
#endif
		}

		/* APPLE */
		++leaked;
		i_error("Leaked unknown fd %d (errno = %s)",
			first_fd, strerror(old_errno));
	}

	/* APPLE */
	if (leaked > 0)
		i_panic("Leaked %d descriptor(s)", leaked);
}
