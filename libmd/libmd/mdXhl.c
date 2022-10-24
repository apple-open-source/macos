/*- mdXhl.c
 * SPDX-License-Identifier: Beerware
 *
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/stat.h>
#ifdef __APPLE__
#include <dispatch/dispatch.h>
#include <os/assumes.h>
#endif
#include <fcntl.h>
#include <unistd.h>

#include <errno.h>
#ifdef __APPLE__
#include <stdatomic.h>
#endif
#include <stdio.h>
#include <stdlib.h>

#include "mdX.h"

#ifdef __APPLE__
#include <limits.h> /* INT_MAX */

/* Kernel flag, but it's still exposed via F_GETFL. */
#ifndef FNOCACHE
#define FNOCACHE 0x40000
#endif

#define DISPATCH_QUEUE_NAME "com.apple.libmd.io.mdX"
/*
 * # Digits we'll tack on after the DISPATCH_QUEUE_NAME.  The counter's really
 * just a debugging aide, so FdChunk() will just fallback to base queue name
 * above if we can't fit it in anymore.
 *
 * The constructed queue name is allocated on the stack.
 */
#define DISPATCH_QUEUE_NDIGITS	10

#define	CHUNK_SIZE	(10 * 1024 * 1024)

static _Atomic unsigned long dispatch_queue_serial = ATOMIC_VAR_INIT(0);
#endif /* __APPLE__ */

char *
MDXEnd(MDX_CTX *ctx, char *buf)
{
#ifdef __APPLE__
	size_t i;
#else
	int i;
#endif
	unsigned char digest[LENGTH];
	static const char hex[]="0123456789abcdef";

	if (!buf)
		buf = malloc(2*LENGTH + 1);
	if (!buf)
		return 0;
	MDXFinal(digest, ctx);
	for (i = 0; i < LENGTH; i++) {
		buf[i+i] = hex[digest[i] >> 4];
		buf[i+i+1] = hex[digest[i] & 0x0f];
	}
	buf[i+i] = '\0';
	return buf;
}

char *
MDXFd(int fd, char *buf)
{
	return MDXFdChunk(fd, buf, 0, 0);
}

char *
MDXFdChunk(int fd, char *buf, off_t ofs, off_t len)
{
#ifdef __APPLE__
	__block MDX_CTX ctx;
	dispatch_queue_t queue;
	dispatch_semaphore_t sema;
	dispatch_io_t io;
	__block int s_error = 0;
	__block bool eof = false;
	off_t chunk_offset;
	bool cached;
	/* +1 to account for a period after the base queue name. */
	char queue_name[sizeof(DISPATCH_QUEUE_NAME) + DISPATCH_QUEUE_NDIGITS + 1];
	unsigned long queue_serial;
#else
	unsigned char buffer[16*1024];
	MDX_CTX ctx;
	struct stat stbuf;
	int readrv, e;
	off_t remain;
#endif

	if (len < 0) {
		errno = EINVAL;
		return NULL;
	}

	MDXInit(&ctx);
	if (ofs != 0) {
		errno = 0;
		if (lseek(fd, ofs, SEEK_SET) != ofs ||
		    (ofs == -1 && errno != 0)) {
#ifdef __APPLE__
			return NULL;
#else
			readrv = -1;
			goto error;
#endif
		}
	}
#ifdef __APPLE__
	cached = (fcntl(fd, F_GETFL) & FNOCACHE) == 0;
	if (cached)
		(void)fcntl(fd, F_NOCACHE, 1);

	/*
	 * Fallback to the base name if we can't assign a serial to it.  The
	 * serial is mostly a debugging aide.
	 */
	queue_serial = atomic_fetch_add(&dispatch_queue_serial, 1);
	if (snprintf(queue_name, sizeof(queue_name),
	    "%s.%d", DISPATCH_QUEUE_NAME, queue_serial) < sizeof(queue_name))
		queue = dispatch_queue_create(queue_name, NULL);
	else
		queue = dispatch_queue_create(DISPATCH_QUEUE_NAME, NULL);
	os_assert(queue);
	sema = dispatch_semaphore_create(0);
	os_assert(sema);

	io = dispatch_io_create(DISPATCH_IO_STREAM, fd, queue, ^(int error) {
		if (error != 0) {
			s_error = error;
		}
		(void)dispatch_semaphore_signal(sema);
	});
	os_assert(io);
	for (chunk_offset = 0; eof == false && s_error == 0 &&
	    (len == 0 || chunk_offset < len); chunk_offset += CHUNK_SIZE) {
		dispatch_io_read(io, chunk_offset,
		    len == 0 ? CHUNK_SIZE : MIN(len, CHUNK_SIZE),
		    queue, ^(bool done, dispatch_data_t data, int error) {
			if (data != NULL) {
				(void)dispatch_data_apply(data, ^(__unused dispatch_data_t region, __unused size_t offset, const void *buffer, size_t size) {
					/* Update() takes an int32. */
					os_assert(size <= INT_MAX);
					MDXUpdate(&ctx, buffer, (CC_LONG)size);
					return (bool)true;
				});
			}

			if (error != 0) {
				s_error = error;
			}

			if (done) {
				eof = (data == dispatch_data_empty);
				dispatch_semaphore_signal(sema);
			}
		});
		dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
	}
	dispatch_release(io); // it will close on its own

	(void)dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);

	dispatch_release(queue);
	dispatch_release(sema);

	if (cached)
		(void)fcntl(fd, F_NOCACHE, 0);

	if (s_error != 0) {
		errno = s_error;
		return NULL;
	}

	return (MDXEnd(&ctx, buf));
#else
	remain = len;
	readrv = 0;
	while (len == 0 || remain > 0) {
		if (len == 0 || remain > sizeof(buffer))
			readrv = read(fd, buffer, sizeof(buffer));
		else
			readrv = read(fd, buffer, remain);
		if (readrv <= 0)
			break;
		MDXUpdate(&ctx, buffer, readrv);
		remain -= readrv;
	}
error:
	if (readrv < 0)
		return NULL;
	return (MDXEnd(&ctx, buf));
#endif
}

char *
MDXFile(const char *filename, char *buf)
{
	return (MDXFileChunk(filename, buf, 0, 0));
}

char *
MDXFileChunk(const char *filename, char *buf, off_t ofs, off_t len)
{
	char *ret;
	int e, fd;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return NULL;
	ret = MDXFdChunk(fd, buf, ofs, len);
	e = errno;
	close (fd);
	errno = e;
	return ret;
}

char *
MDXData (const void *data, unsigned int len, char *buf)
{
	MDX_CTX ctx;

	MDXInit(&ctx);
	MDXUpdate(&ctx,data,len);
	return (MDXEnd(&ctx, buf));
}

#ifndef __APPLE__
/*
 * We won't build with WEAK_REFS anyways, but the below is not applicable for
 * Apple.
 */
#ifdef WEAK_REFS
/* When building libmd, provide weak references. Note: this is not
   activated in the context of compiling these sources for internal
   use in libcrypt.
 */
#undef MDXEnd
__weak_reference(_libmd_MDXEnd, MDXEnd);
#undef MDXFile
__weak_reference(_libmd_MDXFile, MDXFile);
#undef MDXFileChunk
__weak_reference(_libmd_MDXFileChunk, MDXFileChunk);
#undef MDXData
__weak_reference(_libmd_MDXData, MDXData);
#endif
#endif
