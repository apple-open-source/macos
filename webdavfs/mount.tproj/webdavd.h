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
 *	  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *	  must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *	  may be used to endorse or promote products derived from this software
 *	  without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.	IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)webdavd.h	8.1 (Berkeley) 6/5/93
 *
 * $Id: webdavd.h,v 1.9 2002/10/22 22:19:38 lutherj Exp $
 */

#include <sys/cdefs.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include "../webdav_fs.kextproj/webdav_fs.kmodproj/vnops.h"
#include "../webdav_fs.kextproj/webdav_fs.kmodproj/webdav.h"
#include "webdav_mount.h"
#include "webdav_memcache.h"
#include "webdav_authcache.h"
#include "webdav_inode.h"
#include <pthread.h>

/*
 * Meta-chars in an RE.	 Paths in the config file containing
 * any of these characters will be matched using regexec, other
 * paths will be prefix-matched.
 */
#define RE_CHARS ".|()[]*+?\\^$"

/* webdav process structures */

struct webdav_put_struct
{
	int fd;
	char *locktoken;
};

struct webdav_lock_struct
{
	/* If you modify this structure, keep it word aligned. */
	int refresh;
	char *locktoken;
};

struct file_array_element
{
	/* Note that the download status field is first in the hopes that
	 * that will ensure that it is word aligned.  This field will
	 * be used for multithread synchronization so it needs to be
	 * word aligned to ensure that the loads and stores are atomic
	 * on multiprocessor machines.	If you modify the structure,
	 * keep it word aligned as this structure appears inside of
	 * an array.
	 */

	int download_status;
	int fd;
	int uid;
	int deleted;								/* flag to indicate whether this file was deleted */
	char *uri;
	int32_t modtime;
	int32_t cachetime;
	int32_t lastvalidtime;
	webdav_filetype_t file_type;
	struct webdav_lock_struct lockdata;
};

#define CLEAR_GFILE_ENTRY(i) \
	{ \
		if (gfile_array[i].fd > 0) \
		{ \
			(void)close(gfile_array[i].fd); \
		} \
		gfile_array[i].fd = 0; \
		gfile_array[i].uid = 0; \
		gfile_array[i].deleted = 0; \
		gfile_array[i].download_status = 0; \
		gfile_array[i].file_type = 0; \
		if (gfile_array[i].uri) \
		{ \
			(void)free(gfile_array[i].uri); \
		} \
		gfile_array[i].uri = NULL; \
		gfile_array[i].cachetime = 0; \
		gfile_array[i].lastvalidtime = 0; \
		gfile_array[i].modtime = 0; \
	}

/* Note that gfile_array[index].fd needs to be valid when DEL_EXPIRED_CACHE()
   is called. */
#define DEL_EXPIRED_CACHE(index, current_time, timeout) \
	{ \
		if (gfile_array[index].cachetime) \
		{ \
			if (gfile_array[index].deleted) \
			{ \
				CLEAR_GFILE_ENTRY(index); \
			} \
			else \
			{ \
				if (current_time > (gfile_array[index].cachetime + timeout)) \
				{ \
					/* time to clear out this file */ \
					int error = webdav_set_file_handle(gfile_array[index].uri, \
						strlen(gfile_array[index].uri), -1); \
					if (!error || (error == ENOENT)) \
					{ \
						CLEAR_GFILE_ENTRY(index); \
						/* else if we can't clear out the file handle, \
							don't delete the cache, just move on */ \
					} \
				} \
			} \
		} \
	}

struct webdav_lookup_info
{
	enum filetype
	{
		dir, file
	} filetype;
};

struct webdav_stat_struct
{
	const char *orig_uri;
	struct vattr *statbuf;
};

struct webdav_auth_struct
{
	char *webdav_auth_info;
	char *webdav_proxy_auth_info;
};

struct webdav_read_byte_info
{
	off_t byte_start;
	off_t num_bytes;
	char *uri;
	char *byte_addr;
	off_t num_read_bytes;
};

/*
 * webdav functions
 */
extern int webdav_open __P((int proxy_ok, struct webdav_cred *pcr, char *key,
							int * a_socket,
							int so, int *fdp, webdav_filetype_t file_type,
							webdav_filehandle_t * a_file_handle));

extern int webdav_refreshdir __P((int proxy_ok, struct webdav_cred *pcr,
								 webdav_filehandle_t file_handle, int * a_socket));


extern int webdav_lookupinfo __P((int proxy_ok,struct webdav_cred *pcr,
							  char * key, int * a_socket,
								  webdav_filehandle_t * a_file_type));

extern int webdav_stat __P((int proxy_ok,struct webdav_cred *pcr,
							char * key,int * a_socket,
							int so, struct vattr * statbuf));

extern int webdav_statfs __P((int proxy_ok, struct webdav_cred *pcr,
							  char * key, int * a_socket,
							  int so, struct statfs *statfsbuf));

extern int webdav_close __P((int proxy_ok,webdav_filehandle_t file_type,
							 int * a_socket));

extern int webdav_mount __P((int proxy_ok, char *key, int * a_socket,
							 int * a_mount_args));

extern int webdav_fsync __P((int proxy_ok, struct webdav_cred * pcr, webdav_filehandle_t file_handle,
							 int * a_socket));

extern int webdav_create  __P((int proxy_ok, struct webdav_cred * pcr, char * key,
							   int * a_socket, webdav_filetype_t file_type));

extern int webdav_delete  __P((int proxy_ok, struct webdav_cred * pcr, char * key,
							   int * a_socket,
							   webdav_filetype_t file_type));

extern int webdav_rename  __P((int proxy_ok, struct webdav_cred * pcr, char * key,
							   int * a_socket));

extern int webdav_read_bytes __P((int proxy_ok,struct webdav_cred * pcr,char *key,
								  int * a_socket,char ** a_byte_addr,
								  off_t * a_size));

extern int webdav_lock	__P((int proxy_ok, struct file_array_element * array_elem,
							 int * a_socket));

extern void name_tempfile  __P((char *buf, char *seed_prefix));

/* Global Defines */

#define WEBDAV_MAX_OPEN_FILES 512
#define WEBDAV_REQUEST_THREADS 5

/* WEBDAV_RLIMIT_NOFILE needs to be large enough for us to have all cache files
 * open (WEBDAV_MAX_OPEN_FILES), plus some for the defaults (stdin/out, etc),
 * the sockets opened by threads, the dup'd file descriptors passed back to
 * the kext, the socket used to communicate with the kext, and a few extras for
 * libraries we call that might need a few. The most I've ever seen in use is
 * just under 530, so 1024 is more than enough.
 */
#define WEBDAV_RLIMIT_NOFILE 1024

#define MAX_HTTP_LINELEN 4096		/* large enough for fully escaped path + 1K for other data */
#define SHORT_HTTP_LINELEN 1024		/* large enough for the shorter headers we generate */
#define WEBDAV_MAX_USERNAME_LEN 256
#define WEBDAV_MAX_PASSWORD_LEN 256
#define WEBDAV_STOP_DL_TIMEOUT 10000	/* 10 milliseconds */

#define WEBDAV_FS_DONT_CLOSE 1
#define WEBDAV_FS_CLOSE 0

#define WEBDAV_DOWNLOAD_IN_PROGRESS 1
#define WEBDAV_DOWNLOAD_FINISHED 2
#define WEBDAV_DOWNLOAD_TERMINATED 3
#define WEBDAV_DOWNLOAD_ABORTED 4

#define PRIVATE_LOAD_COMMAND "/usr/libexec/load_webdav"
#define PRIVATE_UNMOUNT_COMMAND "/sbin/umount"
#define PRIVATE_UNMOUNT_FLAGS "-f"

/* WEBDAV_IO_TIMEOUT is the amount of time we'll wait for a server to
 * send a response.
 */
#define WEBDAV_IO_TIMEOUT 100			/* seconds */

#define WEBDAV_STATFS_TIMEOUT 60		/* Number of seconds gstatfsbuf is valid */
#define WEBDAV_PULSE_TIMEOUT "600"		/* Default time out = 10 minutes */
#define WEBDAV_CACHE_TIMEOUT 3600		/* 1 hour */
#define WEBDAV_CACHE_LOW_TIMEOUT 300	/* 5 minutes */
#define WEBDAV_CACHE_VALID_TIMEOUT 60	/* Number of seconds file is valid from lastvalidtime */

/*
 * Global functions
 */
extern void activate __P((int so, int proxy_ok, int * socketptr));
extern void webdav_pulse_thread __P((void *arg));

/* Global variables */
extern int glast_array_element;
extern FILE * logfile;
extern struct file_array_element gfile_array[];
extern pthread_mutex_t garray_lock;
extern int gtimeout_val;
extern char * gtimeout_string;
extern webdav_memcache_header_t gmemcache_header;
extern char gmountpt[MAXPATHLEN];
extern struct statfs gstatfsbuf;
extern time_t gstatfstime;

