/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 * All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)activate.c	8.3 (Berkeley) 4/28/95
 */

#ifndef lint
static const char rcsid[] =
	"$Id: activate.c,v 1.3 2002/04/26 20:42:28 lutherj Exp $";
#endif /* not lint */

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/vnode.h>
#include <sys/syslog.h>
#include <sys/time.h>
#include <pthread.h>


#include "../webdav_fs.kextproj/webdav_fs.kmodproj/vnops.h"
#include "webdavd.h"

/*****************************************************************************/

static int get_request(so, vnopp, pcr, key, klen)
	int so;
	int *vnopp;
	struct webdav_cred *pcr;
	void *key;
	int klen;
{
	struct iovec iov[3];
	struct msghdr msg;
	int n;
	
	iov[0].iov_base = (caddr_t)vnopp;
	iov[0].iov_len = sizeof(int);
	iov[1].iov_base = (caddr_t)pcr;
	iov[1].iov_len = sizeof(*pcr);
	iov[2].iov_base = key;
	iov[2].iov_len = klen;
	
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = iov;
	msg.msg_iovlen = 3;
	
	n = recvmsg(so, &msg, 0);
	if (n < 0)
	{
		return (errno);
	}
	if (n <= sizeof(*pcr))
	{
		return (EINVAL);
	}
	
	n -= (sizeof(*pcr) + sizeof(int));
	((char *)key)[n] = '\0';
	
	return (0);
}

/*****************************************************************************/

static void send_reply(so, fd, data, error)
	int so;
	int fd;
	int data;
	int error;
{
	int n;
	struct iovec iov[1];
	struct msghdr msg;
	struct
	{
		struct cmsghdr cmsg;
		int fd;
	} ctl;
	
	/*
	 * Line up error code.  Don't worry about byte ordering
	 * because we must be sending to the local machine.
	 */
	iov[0].iov_base = (caddr_t) & error;
	iov[0].iov_len = sizeof(error);
	
	/*
	 * Build a msghdr
	 */
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	
	/*
	 * If there is a file descriptor to send then
	 * construct a suitable rights control message.
	 */
	if (fd >= 0)
	{
		ctl.fd = fd;
		ctl.cmsg.cmsg_len = sizeof(ctl);
		ctl.cmsg.cmsg_level = SOL_SOCKET;
		ctl.cmsg.cmsg_type = SCM_RIGHTS;
		msg.msg_control = (caddr_t) & ctl;
		msg.msg_controllen = ctl.cmsg.cmsg_len;
	}
	
	/*
	 * Send to kernel...
	 */
	n = sendmsg(so, &msg, 0);
	if (n < 0)
	{
		syslog(LOG_ERR, "send fd: %s", strerror(errno));
	}

#ifdef DEBUG0
	fprintf(stderr, "sendreply: sent %d bytes\n", n);
#endif

	/*
	 * Ok, now we have sent the file descriptor, send the
	 * file handle now if there wasn't an error
	 */
	if (!error)
	{
		iov[0].iov_base = (caddr_t) &data;
		iov[0].iov_len = sizeof(data);
		msg.msg_iov = iov;
		msg.msg_iovlen = 1;
		msg.msg_control = (caddr_t) 0;
		msg.msg_controllen = 0;

		n = sendmsg(so, &msg, 0);
		if (n < 0)
		{
			syslog(LOG_ERR, "send fh: %s", strerror(errno));
		}

#ifdef DEBUG0
		fprintf(stderr, "sendreply: sent %d bytes; fh was %d\n", n, data);
#endif
	}/* end of if !error */

#ifdef notdef
	if (shutdown(so, 2) < 0)
	{
		syslog(LOG_ERR, "shutdown: %s", strerror(errno));
	}
#endif

	/*
	 * Throw away the open file descriptor
	 */
	if (fd >= 0)
	{
	    if (close(fd) < 0)
		{
	        syslog(LOG_ERR, "send_reply close: %s", strerror(errno));
		}
	}
}

/*****************************************************************************/

static void send_data(so, data, size, error)
	int so;
	void *data;
	int size;
	int error;
{
	int n;
	struct iovec iov[2];
	struct msghdr msg;
	int send_error = error;
	
	/*
	 * Line up error code.  Don't worry about byte ordering
	 * because we must be sending to the local machine.
	 */
	iov[0].iov_base = (caddr_t) & send_error;
	iov[0].iov_len = sizeof(send_error);
	if (size != 0)
	{
		iov[1].iov_base = (caddr_t)data;
		iov[1].iov_len = size;
	}
	
	/*
	 * Build a msghdr
	 */
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = iov;
	
	if (size != 0)
	{
		msg.msg_iovlen = 2;
	}
	else
	{
		msg.msg_iovlen = 1;
	}
	
	msg.msg_control = (caddr_t)0;
	msg.msg_controllen = 0;

	n = sendmsg(so, &msg, 0);
	if (n < 0)
	{
		syslog(LOG_ERR, "send data: %s", strerror(errno));
	}
	
#ifdef DEBUG0
	fprintf(stderr, "senddata: sent %d bytes\n", n);
#endif

#ifdef notdef
	if (shutdown(so, 2) < 0)
	{
		syslog(LOG_ERR, "shutdown: %s", strerror(errno));
	}
#endif
}

/*****************************************************************************/

void activate(int so, int proxy_ok, int *socketptr)
{
	struct webdav_cred pcred;
	char key[(MAXPATHLEN + 1) * 2 + sizeof(webdav_rename_header_t)];
	int error;
	int fd = -1;
	int data = 0;
	char *bytes;
	off_t num_bytes;
	int vnop;
	struct vattr statbuf;
	struct statfs statfsbuf;
	
	/*
	 * Read the key from the socket
	 */
	error = get_request(so, &vnop, &pcred, (void *)key, sizeof(key));
	if (error)
	{
		syslog(LOG_ERR, "activate: recvmsg: %s", strerror(error));
		send_reply(so, fd, data, error);
		goto drop;
	}
#if (defined(DEBUG) || defined(WEBDAV_TRACE))
	if (vnop!=WEBDAV_STAT)
	{
#ifdef DEBUG	
	fprintf(stderr, 
#else
	syslog(LOG_ERR,
#endif
		"%s(%d) %s[%d]\n",
		(vnop==WEBDAV_FILE_OPEN) ? "FILE_OPEN" :
			(vnop==WEBDAV_DIR_OPEN) ? "DIR_OPEN" :
			(vnop==WEBDAV_DIR_REFRESH) ? "DIR_REFRESH" :
			(vnop==WEBDAV_CLOSE) ? "CLOSE" :
			(vnop==WEBDAV_FILE_FSYNC) ? "FSYNC" :
			(vnop==WEBDAV_LOOKUP) ? "LOOKUP" :
			(vnop==WEBDAV_STAT) ? "STAT" :
			(vnop==WEBDAV_FILE_CREATE) ? "FILE_CREATE" :
			(vnop==WEBDAV_DIR_CREATE) ? "DIR_CREATE" :
			(vnop==WEBDAV_FILE_DELETE) ? "FILE_DEL" :
			(vnop==WEBDAV_DIR_DELETE) ? "DIR_DEL" :
			(vnop==WEBDAV_STATFS) ? "STATFS" :
			(vnop==WEBDAV_RENAME) ? "RENAME" :
			(vnop==WEBDAV_BYTE_READ) ? "BYTE_READ" : "???",
		vnop, 
		(vnop==WEBDAV_RENAME) ? (char *)(key+sizeof(webdav_rename_header_t)) : key, 
		((vnop==WEBDAV_CLOSE) || (vnop==WEBDAV_FILE_FSYNC)) ? (*(int *) key) : -1);
	}
#endif

	/*
	 * Call the function to handle the request
	 */
	switch (vnop)
	{
		case WEBDAV_FILE_OPEN:
			error = webdav_open(proxy_ok, &pcred, (char *)key, socketptr, so, &fd,
				WEBDAV_FILE_TYPE, &((webdav_filehandle_t)data));
			if (error)
			{
				fd = -1;
			}
			else if (fd < 0)
			{
				error = -1;
			}
			send_reply(so, fd, data, error);
			break;

		case WEBDAV_DIR_OPEN:
			error = webdav_open(proxy_ok, &pcred, (char *)key, socketptr, so, &fd,
				WEBDAV_DIR_TYPE, &((webdav_filehandle_t)data));
			if (error)
			{
				fd = -1;
			}
			else if (fd < 0)
			{
				error = -1;
			}
			send_reply(so, fd, data, error);
			break;

		case WEBDAV_DIR_REFRESH:
			error = webdav_refreshdir(proxy_ok, &pcred, *((webdav_filehandle_t *)key), socketptr);
			send_data(so, (void *)0, 0, error);
			break;

		case WEBDAV_LOOKUP:
			error = webdav_lookupinfo(proxy_ok, &pcred, (char *)key, socketptr,
				&((webdav_filetype_t)data));
			send_data(so, (void *) & data, sizeof(data), error);
			break;

		case WEBDAV_FILE_FSYNC:
			error = webdav_fsync(proxy_ok, &pcred, *((webdav_filehandle_t *)key), socketptr);
			send_data(so, (void *)0, 0, error);
			break;

		case WEBDAV_CLOSE:
			error = webdav_close(proxy_ok, *((webdav_filehandle_t *)key), socketptr);
			send_data(so, (void *)0, 0, error);
			break;

		case WEBDAV_STAT:
			error = webdav_stat(proxy_ok, &pcred, (char *)key, socketptr, so, &statbuf);
			send_data(so, (void *) & statbuf, sizeof(statbuf), error);
			break;

		case WEBDAV_FILE_CREATE:
			error = webdav_create(proxy_ok, &pcred, (char *)key, socketptr, WEBDAV_FILE_TYPE);
			send_data(so, (void *)0, 0, error);
			break;

		case WEBDAV_DIR_CREATE:
			error = webdav_create(proxy_ok, &pcred, (char *)key, socketptr, WEBDAV_DIR_TYPE);
			send_data(so, (void *)0, 0, error);
			break;

		case WEBDAV_FILE_DELETE:
			error = webdav_delete(proxy_ok, &pcred, (char *)key, socketptr, WEBDAV_FILE_TYPE);
			send_data(so, (void *)0, 0, error);
			break;

		case WEBDAV_DIR_DELETE:
			error = webdav_delete(proxy_ok, &pcred, (char *)key, socketptr, WEBDAV_DIR_TYPE);
			send_data(so, (void *)0, 0, error);
			break;

		case WEBDAV_RENAME:
			error = webdav_rename(proxy_ok, &pcred, (char *)key, socketptr);
			send_data(so, (void *)0, 0, error);
			break;

		case WEBDAV_STATFS:
			error = webdav_statfs(proxy_ok, &pcred, (char *)key, socketptr, so, &statfsbuf);
			send_data(so, (void *) & statfsbuf, sizeof(statfsbuf), error);
			break;

		case WEBDAV_BYTE_READ:
			error = webdav_read_bytes(proxy_ok, &pcred, (char *)key, socketptr, &bytes, &num_bytes);
			send_data(so, (void *)bytes, (int)num_bytes, error);
			if (bytes)
			{
				free(bytes);
			}
			break;

		default:
			error = EOPNOTSUPP;
			break;
	}

#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
	if (vnop!=WEBDAV_STAT)
	{
#ifdef WEBDAV_ERROR
		if (error)
#endif
		{
#ifdef DEBUG	
			fprintf(stderr, 
#else
			syslog(LOG_ERR,
#endif
				"error %d, %s(%d) %s[%d]\n", error,
				(vnop==WEBDAV_FILE_OPEN) ? "FILE_OPEN" :
					(vnop==WEBDAV_DIR_OPEN) ? "DIR_OPEN" :
					(vnop==WEBDAV_DIR_REFRESH) ? "DIR_REFRESH" :
					(vnop==WEBDAV_CLOSE) ? "CLOSE" :
					(vnop==WEBDAV_FILE_FSYNC) ? "FSYNC" :
					(vnop==WEBDAV_LOOKUP) ? "LOOKUP" :
					(vnop==WEBDAV_STAT) ? "STAT" :
					(vnop==WEBDAV_FILE_CREATE) ? "FILE_CREATE" :
					(vnop==WEBDAV_DIR_CREATE) ? "DIR_CREATE" :
					(vnop==WEBDAV_FILE_DELETE) ? "FILE_DEL" :
					(vnop==WEBDAV_STATFS) ? "STATFS" :
					(vnop==WEBDAV_DIR_DELETE) ? "DIR_DEL" :
					(vnop==WEBDAV_RENAME) ? "RENAME" :
					(vnop==WEBDAV_BYTE_READ) ? "BYTE_READ" : "???",
				vnop,
				(vnop==WEBDAV_RENAME) ? (char *)(key+sizeof(webdav_rename_header_t)) : key, 
				((vnop==WEBDAV_CLOSE) || (vnop==WEBDAV_FILE_FSYNC)) ? (*(int *)key) :
					((vnop==WEBDAV_FILE_OPEN) || (vnop==WEBDAV_DIR_OPEN)) ? data: -1);
		}
	}
#endif
drop:
	close(so);
}


/*****************************************************************************/

void webdav_pulse_thread(arg)
	void *arg;
{
	struct timeval tv;
	struct timezone tz;
	int i, error, proxy_ok;
	int mysocket;
	
	mysocket = socket(PF_INET, SOCK_STREAM, 0);
	if (mysocket < 0)
	{
		errx(EIO, "webdav_pulse_thread socket");
	}
	
	proxy_ok = *((int *)arg);
	
	while (TRUE)
	{
		error = pthread_mutex_lock(&garray_lock);
		if (error)
		{
			goto done;
		}
		gettimeofday(&tv, &tz);

#ifdef DEBUG
        printf("Pulse thread running at %s\n",ctime((long *) &tv.tv_sec));
#endif

		for (i = 0; i < WEBDAV_MAX_OPEN_FILES; ++i)
		{
			if (gfile_array[i].fd &&
				gfile_array[i].lockdata.locktoken &&
				gfile_array[i].lockdata.refresh)
			{
				error = webdav_lock(proxy_ok, &(gfile_array[i]), &mysocket);
			}
			
			/*
			 * if there is an error, just move on to the
			 * next one and try to refresh it.
			 */
	
			if (gfile_array[i].fd && gfile_array[i].cachetime)
			{
	
				if (gfile_array[i].deleted)
				{
					CLEAR_GFILE_ENTRY(i);
				}
				else
				{
					if (tv.tv_sec > (gfile_array[i].cachetime + WEBDAV_CACHE_TIMEOUT))
					{
						/* time to clear out this file */
						if (!webdav_set_file_handle(gfile_array[i].uri,
							strlen(gfile_array[i].uri), -1))
						{
							CLEAR_GFILE_ENTRY(i);
						}
						/* else if we can't clear out the file handle,
						  don't delete the cache, just move on */
	
					}
				}
			}
		} /* end for loop */

        error = pthread_mutex_unlock(&garray_lock);

done:
        (void) sleep(gtimeout_val/2);
    }
}

/*****************************************************************************/
