/* Sio.c */

#include "Sys.h"

#include <errno.h>
#include <signal.h>
#include <setjmp.h>

#include "Sio.h"

#ifndef EPIPE
#	define EPIPE 0
#endif
#ifndef ETIMEDOUT
#	define ETIMEDOUT 0
#endif

jmp_buf gNetTimeoutJmp;
jmp_buf gPipeJmp;

static void
SIOHandler(int sigNum)
{
	if (sigNum == SIGPIPE)
		longjmp(gPipeJmp, 1);
	longjmp(gNetTimeoutJmp, 1);
}	/* SIOHandler */




/* Read up to "size" bytes on sfd before "tlen" seconds.
 *
 * If "retry" is on, after a successful read of less than "size"
 * bytes, it will attempt to read more, upto "size."
 *
 * If the timer elapses and one or more bytes were read, it returns
 * that number, otherwise a timeout error is returned.
 *
 * Although "retry" would seem to indicate you may want to always
 * read "size" bytes or else it is an error, even with that on you
 * may get back a value < size.  Set "retry" to 0 when you want to
 * return as soon as there is a chunk of data whose size is <= "size".
 */

int
Sread(int sfd, char *buf0, size_t size, int tlen, int retry)
{
	int nread;
	volatile int nleft;
	int tleft;
	vsio_sigproc_t sigalrm, sigpipe;
	time_t done, now;
	char *volatile buf;

	buf = buf0;
	if (setjmp(gNetTimeoutJmp) != 0) {
		alarm(0);
		(void) signal(SIGALRM, (sio_sigproc_t) sigalrm);
		(void) signal(SIGPIPE, (sio_sigproc_t) sigpipe);
		nread = size - nleft;
		if (nread > 0)
			return (nread);
		errno = ETIMEDOUT;
		return (kTimeoutErr);
	}

	if (setjmp(gPipeJmp) != 0) {
		alarm(0);
		(void) signal(SIGALRM, (sio_sigproc_t) sigalrm);
		(void) signal(SIGPIPE, (sio_sigproc_t) sigpipe);
		nread = size - nleft;
		if (nread > 0)
			return (nread);
		errno = EPIPE;
		return (kBrokenPipeErr);
	}

	sigalrm = (vsio_sigproc_t) signal(SIGALRM, SIOHandler);
	sigpipe = (vsio_sigproc_t) signal(SIGPIPE, SIOHandler);
	errno = 0;

	nleft = (int) size;
	time(&now);
	done = now + tlen;
	while (1) {
		tleft = (int) (done - now);
		if (tleft < 1) {
			nread = size - nleft;
			if (nread == 0) {
				nread = kTimeoutErr;
				errno = ETIMEDOUT;
			}
			goto done;
		}
		(void) alarm((unsigned int) tleft);
		nread = read(sfd, buf, nleft);
		(void) alarm(0);
		if (nread <= 0) {
			if (nread == 0) {
				/* EOF */
				nread = size - nleft;
				goto done;
			} else if (errno != EINTR) {
				nread = size - nleft;
				if (nread == 0)
					nread = -1;
				goto done;
			} else {
				errno = 0;
				nread = 0;
				/* Try again. */
			}
		}
		nleft -= nread;
		if ((nleft <= 0) || (retry == 0))
			break;
		buf += nread;
		time(&now);
	}
	nread = size - nleft;

done:
	(void) signal(SIGALRM, (sio_sigproc_t) sigalrm);
	(void) signal(SIGPIPE, (sio_sigproc_t) sigpipe);

	return (nread);
}	/* Sread */




int
Swrite(int sfd, char *buf0, size_t size, int tlen)
{
	volatile int nleft;
	int nwrote, tleft;
	vsio_sigproc_t sigalrm, sigpipe;
	time_t done, now;
	char *volatile buf;

	buf = buf0;
	if (setjmp(gNetTimeoutJmp) != 0) {
		alarm(0);
		(void) signal(SIGALRM, (sio_sigproc_t) sigalrm);
		(void) signal(SIGPIPE, (sio_sigproc_t) sigpipe);
		nwrote = size - nleft;
		if (nwrote > 0)
			return (nwrote);
		errno = ETIMEDOUT;
		return (kTimeoutErr);
	}

	if (setjmp(gPipeJmp) != 0) {
		alarm(0);
		(void) signal(SIGALRM, (sio_sigproc_t) sigalrm);
		(void) signal(SIGPIPE, (sio_sigproc_t) sigpipe);
		nwrote = size - nleft;
		if (nwrote > 0)
			return (nwrote);
		errno = EPIPE;
		return (kBrokenPipeErr);
	}

	sigalrm = (vsio_sigproc_t) signal(SIGALRM, SIOHandler);
	sigpipe = (vsio_sigproc_t) signal(SIGPIPE, SIOHandler);

	nleft = (int) size;
	time(&now);
	done = now + tlen;
	while (1) {
		tleft = (int) (done - now);
		if (tleft < 1) {
			nwrote = size - nleft;
			if (nwrote == 0) {
				nwrote = kTimeoutErr;
				errno = ETIMEDOUT;
			}
			goto done;
		}
		(void) alarm((unsigned int) tleft);
		nwrote = write(sfd, buf, nleft);
		(void) alarm(0);
		if (nwrote < 0) {
			if (errno != EINTR) {
				nwrote = size - nleft;
				if (nwrote == 0)
					nwrote = -1;
				goto done;
			} else {
				errno = 0;
				nwrote = 0;
				/* Try again. */
			}
		}
		nleft -= nwrote;
		if (nleft <= 0)
			break;
		buf += nwrote;
		time(&now);
	}
	nwrote = size - nleft;

done:
	(void) signal(SIGALRM, (sio_sigproc_t) sigalrm);
	(void) signal(SIGPIPE, (sio_sigproc_t) sigpipe);

	return (nwrote);
}	/* Swrite */
