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
 *	@(#)pt_file.c	8.3 (Berkeley) 7/3/94
 */


#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/syslog.h>

#include "fetch.h"
#include "http.h"
#include "pathnames.h"
#include "webdavd.h"
#include "../webdav_fs.kextproj/webdav_fs.kmodproj/vnops.h"

/*****************************************************************************/

void name_tempfile(buf, seed_prefix)
	char *buf;			/* buf must be of size MAXPATHLEN */
	char *seed_prefix;	/* seed_prefix must be a pointer to a null-terminated string (or a NULL pointer) */
{
	char *cp;

	if (buf)
	{
		if ((seed_prefix == (char *)NULL) ||
			(*seed_prefix == '\0') ||
			(strlen(seed_prefix) > (MAXPATHLEN - strlen(_PATH_TMPWEBDAVDIR) - strlen(_WEBDAVTMPEXT) - 2)))
		{
			cp = GENERIC_SEED_PREFIX;
		}
		else
		{
			cp = seed_prefix;
		}
		sprintf(buf, "%s/%s%s", _PATH_TMPWEBDAVDIR, cp, _WEBDAVTMPEXT);
	}
}

/*****************************************************************************/

int webdav_open(proxy_ok, pcr, key, a_socket, so, fdp, file_type, a_file_handle)
	int proxy_ok;
	struct webdav_cred *pcr;
	char *key;
	int *a_socket;
	int so;
	int *fdp;
	webdav_filetype_t file_type;
	webdav_filehandle_t *a_file_handle;
{
	char pbuf[MAXPATHLEN];
	char *utf8_key;
	struct webdav_lock_struct lock_struct;
	int error, error2;
	int locked = FALSE;
	int i, arrayelem = -1;
	struct fetch_state fs;
	struct timeval tv;
	struct timezone tz;

	lock_struct.refresh = 0;
	lock_struct.locktoken = NULL;

	utf8_key = utf8_encode((const unsigned char *)key);
	if (!utf8_key)
	{
		return (ENOMEM);
	}

	/* What we are starting with is a pathname - the http part
	  in the key variable.	 Start by initializing the fs_state
	  structure				
	*/

	bzero(&fs, sizeof(fs));
	fs.fs_use_connect = 1;
	fs.fs_socketptr = a_socket;
	fs.fs_uid = pcr->pcr_uid;

	error = http_parse(&fs, utf8_key, proxy_ok);
	if (error)
	{
#ifdef DEBUG
		fprintf(stderr, "webdav_open: parse returned error %d\n", error);
#endif

		free(utf8_key);
		return (error);
	}

	/* After the call to http parse, the fs structure will have all
	  the right setting to do the retrieval except that the output file
	  will be going to the wrong place and the file descriptor will 
	  be zero.	 We'll update that here by adding the prefix for our 
	  cache directory and then go open the file. Note that it is possible 
	  for us to have a null file because this is the root. We'll use the 
	  generic name in that case.
	  
	  Also note that http_parse mallocs several chunks of memory and
	  sets fs->fs_proto to point to them. That memory must be freed before
	  exiting this function.
	 */

	name_tempfile(pbuf, fs.fs_outputfile);
	fs.fs_outputfile = pbuf;

#ifdef DEBUG
	fprintf(stderr, "Output file for open is %s, glast=%d\n", fs.fs_outputfile, glast_array_element);
#endif

	error = pthread_mutex_lock(&garray_lock);
	if (error)
	{
		free(utf8_key);
		return (error);
	}

	if (file_type == WEBDAV_FILE_TYPE)
	{

		/* webdav_get_file_handle() doesn't return an error when no 
		  entry is found, and a directory entry is of no use, so do
		  this just on files. */
		error = webdav_get_file_handle(key, strlen(key), &arrayelem);
		if (error)
		{
			free(utf8_key);
			error2 = pthread_mutex_unlock(&garray_lock);
			if (error2)
			{
				error = error2;
				/* *** fatal error for mount_webdav ??? *** */
			}
			return (error);
		}
	}

	if ((arrayelem != -1) &&
		(gfile_array[arrayelem].fd != 0) &&
		gfile_array[arrayelem].cachetime && /* *** just for now *** */
		gfile_array[arrayelem].file_type == WEBDAV_FILE_TYPE &&
		(!memcmp(utf8_key, gfile_array[arrayelem].uri, strlen(utf8_key))))
	{

#ifdef NOT_YET
		if (!gfile_array[arrayelem].cachetime)
		{
			/* *** We really should return an error to the kernel, telling it
			  to repeat the lookup, but for now, just keep going.
			*** */
		}
#endif

		/* We found the element we needed so we're good,  Since
		  we have the actual contents set the mirror flag
		  in the fs structure.	 Note that if the cachetime is zero, this would
		  be a second open (presumably by a 2nd user) of the same file and
		  should get it's own array element.  We never want to hand out
		  the same file handle to two seperate open.  That's why we make
		  sure that the cachetime is not zero.	 Also we double check that
		  this is a file since we don't cache directories.
		*/

		fs.fs_restart =
			(gfile_array[arrayelem].download_status == WEBDAV_DOWNLOAD_FINISHED) ? FALSE : TRUE;
		fs.fs_mirror = TRUE;
		fs.fs_st_mtime = gfile_array[arrayelem].modtime;
		gfile_array[arrayelem].cachetime = 0;

	}
	else
	{

#ifdef DEBUG
		fprintf(stderr, "webdav_open: path = %s, uid = %d, gid = %d, fflag = %x, oflag = %x\n",
			pbuf, pcr->pcr_uid, pcr->pcr_groups[0], pcr->pcr_flag, (pcr->pcr_flag) - 1);
#endif

		/* Find an open array element in our file table and open the file again
		  since our orignal fd will be closed by the kernel after being reassigned
		  to the client proces.  This file descriptor will be kept local to this
		  process and used to push data to the server when the time comes
		*/

		/* start with element after glast_array_element */
		if (glast_array_element != (WEBDAV_MAX_OPEN_FILES - 1))
		{
			arrayelem = glast_array_element + 1;
		}
		else
		{
			arrayelem = 0;
		}

		/* first look for an empty entry */
		i = arrayelem;
		do
		{
			/* if the file descriptor is zero, we found a space so fill it in
			  and update our counters.	 Otherwise we have too many open files
			  and we should return an error, unless we can steal a closed file
			  which has been cached */
			if (gfile_array[i].fd == 0)
			{
				goto success;
			}

			/* next element */
			if (i != (WEBDAV_MAX_OPEN_FILES - 1))
			{
				++i;
			}
			else
			{
				i = 0;
			}
		} while (i != arrayelem);

		/* Clear out all of the cache files that have been closed for a while */
		gettimeofday(&tv, &tz);
		for (i = 0; i < WEBDAV_MAX_OPEN_FILES; ++i)
		{
			/* Of course we know that there aren't any unused fd's at this 
			  point, or we wouldn't be here, but since the macro depends 
			  on it, check whether the fd is set. */
			if (gfile_array[i].fd)
			{
				DEL_EXPIRED_CACHE(i, tv.tv_sec, WEBDAV_CACHE_LOW_TIMEOUT);
			}
		}

		/* Look for an empty entry again */
		i = arrayelem;
		do
		{
			/* if the file descriptor is zero, we found a space so fill it in
			  and update our counters.	 Otherwise we have too many open files
			  and we should return an error, unless we can steal a closed file
			  which has been cached */
			if (gfile_array[i].fd == 0)
			{
				goto success;
			}

			/* next element */
			if (i != (WEBDAV_MAX_OPEN_FILES - 1))
			{
				++i;
			}
			else
			{
				i = 0;
			}
		} while (i != arrayelem);

		/* We couldn't find an open file so steal a cache file */
		i = arrayelem;
		do
		{
			if (gfile_array[i].fd && gfile_array[i].cachetime)
			{
				error = webdav_set_file_handle(gfile_array[i].uri, strlen(gfile_array[i].uri), -1);
				if (!error || (error == ENOENT))
				{
					CLEAR_GFILE_ENTRY(i);
					error = 0;
					/* Ok now the array element is prepared for our reuse */
					goto success;
				}
				/* else, can't clear out the file handle so 
				  don't delete the cache, just move on */
			}

			/* next element */
			if (i != (WEBDAV_MAX_OPEN_FILES - 1))
			{
				++i;
			}
			else
			{
				i = 0;
			}
		} while (i != arrayelem);

		/* we never found an entry */
		error = EMFILE;							/* too many open files */
		goto free_unlock_done;

success:

		glast_array_element = arrayelem = i;

		/* Now open the file and squirrel the file descriptor
		 * in the file_array.
		 */
		gfile_array[arrayelem].fd = mkstemp(fs.fs_outputfile);
		if (gfile_array[arrayelem].fd == -1)
		{
			gfile_array[arrayelem].fd = 0;

			/* Clear out all of the cache files that have been closed for a while */
			gettimeofday(&tv, &tz);
			for (i = 0; i < WEBDAV_MAX_OPEN_FILES; ++i)
			{
				if (gfile_array[i].fd)
				{
					DEL_EXPIRED_CACHE(i, tv.tv_sec, WEBDAV_CACHE_LOW_TIMEOUT);
				}
			}

			/* Try again */
			gfile_array[arrayelem].fd = mkstemp(fs.fs_outputfile);
			if (gfile_array[arrayelem].fd == -1)
			{
				gfile_array[arrayelem].fd = 0;
				error = errno;
				goto free_unlock_done;
			}
		}

		/* Get rid of that file. */
		(void)unlink(fs.fs_outputfile);

		/* Fill in the file array. */
		gfile_array[arrayelem].cachetime = 0;
		gfile_array[arrayelem].lastvalidtime = 0;
		gfile_array[arrayelem].uri = utf8_key;
		utf8_key = NULL;
		gfile_array[arrayelem].uid = pcr->pcr_uid;
		gfile_array[arrayelem].download_status = 0;
		gfile_array[arrayelem].deleted = 0;
		gfile_array[arrayelem].file_type = file_type;

		/* If we get an error beyond this point we need to clean
		 * out the file_array element */
	}

	if (file_type == WEBDAV_FILE_TYPE)
	{

		if ((((pcr->pcr_flag) - 1) & O_WRONLY) || (((pcr->pcr_flag) - 1) & O_RDWR))
		{
			/* -1 on pcr_flag converts kernel FFLAGS to OFLAGS, see <fcntl.h> */

			/* If we are opening this file for write access, lock it first,
			  before we copy it into the cache file from the server, 
			  or truncate the cache file. */
			error = make_request(&fs, http_lock, (void *) & lock_struct, WEBDAV_FS_DONT_CLOSE);
			if (error)
			{
#ifdef DEBUG
				fprintf(stderr, "webdav_open: lock returned error %d\n", error);
#endif

				goto clear_free_unlock_done;
			}
			locked = TRUE;

			/* If opened for write and O_TRUNC we can set the length to zero 
			  and not get it from the server.
			*/
			if (((pcr->pcr_flag) - 1) & O_TRUNC)
			{
				if (ftruncate(gfile_array[arrayelem].fd, 0))
				{
					error = errno;
					goto clear_free_unlock_done;
				}

				/* fsync will reset the modtime */

				goto get_finished;
			}
		}

		/* Get the file from the server */
		gettimeofday(&tv, &tz);
                
		/* Skip the GET if the file is being opened read-only, it was completely downloaded,
		 * and it was validated in the last WEBDAV_CACHE_VALID_TIMEOUT seconds.
		 */
		if ( locked ||
			 (gfile_array[arrayelem].download_status != WEBDAV_DOWNLOAD_FINISHED) ||
			 (gfile_array[arrayelem].lastvalidtime + WEBDAV_CACHE_VALID_TIMEOUT < tv.tv_sec) )
		{
			/* Ok, now put the file descriptor in to the fs for get to use */
			fs.fs_fd = dup(gfile_array[arrayelem].fd);
			if (fs.fs_fd == -1)
			{
				
				/* Clear out all of the cache files that have been closed for a while */
				gettimeofday(&tv, &tz);
				for (i = 0; i < WEBDAV_MAX_OPEN_FILES; ++i)
				{
					if (gfile_array[i].fd)
					{
						DEL_EXPIRED_CACHE(i, tv.tv_sec, WEBDAV_CACHE_LOW_TIMEOUT);
					}
				}
	
				/* Try again */
				fs.fs_fd = dup(gfile_array[arrayelem].fd);
				if (fs.fs_fd == -1)
				{
					error = errno;
					goto clear_free_unlock_done;
				}
			}
			
			error = get(&fs, &(gfile_array[arrayelem].download_status));
			if (error)
			{
#ifdef DEBUG
				fprintf(stderr, "webdav_open: get returned error %d\n", error);
#endif
				goto clear_free_unlock_done;
			}
			
			gfile_array[arrayelem].lastvalidtime = tv.tv_sec;
		}
		else
		{
			/* we skipped the get, so we have to free up the memory allocated by http_parse */
			fs.fs_close(&fs);
		}

		/* The file was retrieved or certified current by the server.
		 * If there wasn't a last-modified and modtime hasn't been initialized, 
		 * put the current time in for modtime.	 On read only files, that will 
		 * be a safe time (before the file is retrieved).  For read/write files,
		 * fsync will reset the modtime.
		 */
		if (fs.fs_st_mtime)
		{
			gfile_array[arrayelem].modtime = fs.fs_st_mtime;
		}
		else if (!gfile_array[arrayelem].modtime)
		{
			gfile_array[arrayelem].modtime = tv.tv_sec;
		}

get_finished:
		/* Put the file handle in the inode array.	If the entry is not
		  there, something has gone wrong.	 (We don't cache directories.)
		*/
		error = webdav_set_file_handle(key, strlen(key), arrayelem);
		if (error)
		{
			goto clear_free_unlock_done;
		}
	}
	else
	{
		/* free up the memory allocated by http_parse */
		fs.fs_close(&fs);
		
		if (file_type == WEBDAV_DIR_TYPE)
		{

			/* Directory opens are always done in the foreground so set the
			 * download status to done
			 */

			gfile_array[arrayelem].download_status = WEBDAV_DOWNLOAD_FINISHED;

		}
		else
		{
			error = EFTYPE;
			goto clear_free_unlock_done;
		}
	}

	/* now put in the new lock data */

	gfile_array[arrayelem].lockdata.locktoken = lock_struct.locktoken;
	if (lock_struct.locktoken)
	{
		gfile_array[arrayelem].lockdata.refresh = 1;
	}
	else
	{
		gfile_array[arrayelem].lockdata.refresh = 0;
	}

#ifdef DEBUG
	fprintf(stderr, "intermediate values, i:%d, arrayelem:%d, glast=%d\n", i, arrayelem,
		glast_array_element);
#endif

clear_free_unlock_done:

	if (error)
	{

		/* Unlock it on the server if it was opened for write and there's 
		  been an error. */
		if (locked)
		{
			fs.fs_fd = -1;						/* just in case */

			error2 = make_request(&fs, http_unlock, (void *) & lock_struct, WEBDAV_FS_CLOSE);
#ifdef DEBUG
			if (error2)
				fprintf(stderr, "webdav_open: unlock returned error %d\n", error2);
#endif

			locked = FALSE;
		}
		else
		{
			fs.fs_close(&fs);
		}


		/* if the file was not found on the server when we went to get it,
		  remove it from the cache. */
		if (error == ENOENT)
		{
			(void)webdav_remove_inode(key, strlen(key));
			(void)webdav_memcache_remove(pcr->pcr_uid, key, &gmemcache_header);
		}

		/* If there is an error, clean up the file array. */
		CLEAR_GFILE_ENTRY(arrayelem);
	}

free_unlock_done:

	if (error == 0)
	{
		/* fdp is only used when error is not set */
		*fdp = dup(gfile_array[arrayelem].fd);
		if (*fdp == -1)
		{
			/* Clear out all of the cache files that have been closed for a while */
			gettimeofday(&tv, &tz);
			for (i = 0; i < WEBDAV_MAX_OPEN_FILES; ++i)
			{
				if (gfile_array[i].fd)
				{
					DEL_EXPIRED_CACHE(i, tv.tv_sec, WEBDAV_CACHE_LOW_TIMEOUT);
				}
			}

			/* Try again */
			*fdp = dup(gfile_array[arrayelem].fd);
			if (*fdp == -1)
			{
				error = errno;
				CLEAR_GFILE_ENTRY(arrayelem);
			}
			else
			{
				*a_file_handle = arrayelem;
			}
		}
		else
		{
			*a_file_handle = arrayelem;
		}
	}

	error2 = pthread_mutex_unlock(&garray_lock);
	if (error2)
	{
		if (!error)
		{
			error = error2;
			/* *** fatal error for mount_webdav ??? *** */
		}
	}

	/* free utf8_key if we didn't use it */ 
	if (utf8_key != NULL)
	{
		free(utf8_key);
	}
	
#ifdef DEBUG
	fprintf(stderr, "webdav_open returns fd = %d, error = %d, arrayelem = %d\n", *fdp, error,
		*a_file_handle);
#endif

	return (error);
}

/*****************************************************************************/

int webdav_close(proxy_ok, file_handle, a_socket)
	int proxy_ok;
	webdav_filehandle_t file_handle;
	int *a_socket;
{
	int newerror, error = 0;
	struct fetch_state fs;
	char *uri;
	struct timeval tv;
	struct timezone tz;
	int was_locked;

	error = pthread_mutex_lock(&garray_lock);
	if (error)
	{
		goto done;
	}

	if (!gfile_array[file_handle].fd)
	{
		/* Trying to close something we did not open
		 * uh oh
		 */
		error = EBADF;
		goto unlock_finish;
	}


	/* Kill any threads that may be downloading data for this file */
	/* XXX CHW. This needs to be an atomic test and set which I will do */
	/* as soon as I find out where the userlevel library for that is */

	if (gfile_array[file_handle].download_status == WEBDAV_DOWNLOAD_IN_PROGRESS)
	{
		gfile_array[file_handle].download_status = WEBDAV_DOWNLOAD_TERMINATED;
	}

	while (gfile_array[file_handle].download_status == WEBDAV_DOWNLOAD_TERMINATED)
	{
		/* wait for the downloading thread to acknowledge that we stopped.*/
		usleep(WEBDAV_STOP_DL_TIMEOUT);
	}

	/* Now get the current time and update the cache time.	We basically start
	 * the cache clock on close.  We will reset the files mod time if we
	 * had the file locked since proper observers of the protocol will not have
	 * changed the file while we had it locked.
	 */

	gettimeofday(&tv, &tz);
	gfile_array[file_handle].cachetime = tv.tv_sec;


	if (gfile_array[file_handle].lockdata.locktoken)
	{
		was_locked = TRUE;
		bzero(&fs, sizeof(fs));
		fs.fs_use_connect = 1;
		fs.fs_uid = gfile_array[file_handle].uid;
		fs.fs_socketptr = a_socket;

		error = http_parse(&fs, gfile_array[file_handle].uri, proxy_ok);
		if (error)
		{
			/* this should never happen */
			fprintf(stderr, "webdav_close: parse returned error %d\n", error);
		}

		error = make_request(&fs, http_unlock, (void *) & gfile_array[file_handle].lockdata,
			WEBDAV_FS_CLOSE);
		if (error)
		{
			/* Ignore the error, we still want to do all our cleanup */
			/* The lock will eventually timeout, besides history suggests that*/
			/* the server may send us a 412 but still clear the lock */
		}
		free(gfile_array[file_handle].lockdata.locktoken);
		gfile_array[file_handle].lockdata.locktoken = 0;
		gfile_array[file_handle].lockdata.refresh = 0;
	}
	else
	{
		was_locked = FALSE;
	}
	
	/* was the file opened with write access? */
	if ( was_locked )
	{
		/* If the file had write access, it is possible that a client may
		* have created a new file, stat'd it, gotten a size of zero and
		* entered the results in the stat cache. If the same client fills
		* up the file and closes it within the stat cache timeout,
		* it is possible that it will then do a stat and get
		* the zero size instead of talking to the server and getting the
		* correct size.  Thus we need to do a memcache_remove here now that we
		* know the file is being closed and the kernel will stop returning the
		* stat information from it's cache file and the stat cache entry may
		* get used.
		*/
		
		/* the key, gfile_array[file_handle].uri, has been utf8_encode'd,
		so get a non encoded copy and remove it. */
		uri = percent_decode(gfile_array[file_handle].uri);
		error = webdav_memcache_remove(gfile_array[file_handle].uid, uri, &gmemcache_header);
		free(uri);
	}
	
	/* We'll keep the file in the zombie cache even if something went 
	  wrong downloading the cache file, and try the restart mechanism 
	  on it when it's re-opened. */

	if (error ||
		(gfile_array[file_handle].deleted) ||
		(gfile_array[file_handle].file_type == WEBDAV_DIR_TYPE))
	{
		/* Something went wrong with this file, it was deleted,
		 * or it is a directory so we will close it and not put 
		 * it in the zombie cache.
		 */
		newerror = close(gfile_array[file_handle].fd);
		if (newerror)
		{
			error = newerror;					/*close error supercedes memcache remove error */
		}

		newerror = webdav_set_file_handle(gfile_array[file_handle].uri,
			strlen(gfile_array[file_handle].uri), -1);
		if (newerror)
		{
			if (!error)
			{
				error = newerror;
			}
		}

		gfile_array[file_handle].fd = 0;		/* since it's already closed */

		CLEAR_GFILE_ENTRY(file_handle)

		goto unlock_finish;
	}

unlock_finish:

	newerror = pthread_mutex_unlock(&garray_lock);
	if (newerror)
	{
		if (!error)
		{
			error = newerror;
		}
		goto done;
	}

done:

	return (error);
}

/*****************************************************************************/

int webdav_lookupinfo(proxy_ok, pcr, key, a_socket, a_file_type)
	int proxy_ok;
	struct webdav_cred *pcr;
	char *key;
	int *a_socket;
	webdav_filetype_t *a_file_type;
{
	char pbuf[MAXPATHLEN];
	char *utf8_key;
	int error;
	struct fetch_state fs;

	/* What we are starting with is a pathname - the http part
	  in the key variable.  Someday that may change but this is
	  still the prototype.  Start by initializing the fs_state
	  structure							 */

	bzero(&fs, sizeof(fs));
	fs.fs_use_connect = 1;
	fs.fs_socketptr = a_socket;
	utf8_key = utf8_encode((const unsigned char *)key);
	if (!utf8_key)
	{
		return (ENOMEM);
	}

	fs.fs_uid = pcr->pcr_uid;
	
	error = http_parse(&fs, utf8_key, proxy_ok);
	if (error)
	{
		goto done;
	}

	/* After the call to http parse, the fs structure will have all
	  the right settings except that it will have set pbuf */

	name_tempfile(pbuf, fs.fs_outputfile);
	fs.fs_outputfile = pbuf;

#ifdef DEBUG
	fprintf(stderr, "Output file for lookup is %s, glast=%d\n", fs.fs_outputfile, glast_array_element);
#endif

	error = make_request(&fs, http_lookup, (void *)a_file_type, WEBDAV_FS_CLOSE);
	if (error)
	{
		goto done;
	}

done:

	free(utf8_key);
	return (error);
}

/*****************************************************************************/

int webdav_stat(proxy_ok, pcr, key, a_socket, so, statbuf)
	int proxy_ok;
	struct webdav_cred *pcr;
	char *key;
	int *a_socket;
	int so;
	struct vattr *statbuf;
{
	char pbuf[MAXPATHLEN];
	char *utf8_key;
	int error, error2;
	struct fetch_state fs;
	struct webdav_stat_struct statstruct;

	/* What we are starting with is a pathname - the http part
	  in the key variable.						 */

	bzero(&fs, sizeof(fs));
	fs.fs_use_connect = 1;
	fs.fs_socketptr = a_socket;

	/* ok, now that we have assembled the uri, check the cache to see if
	  we have this one already we look using the pretranslated uri to
	  save time */

	if ( webdav_memcache_retrieve(pcr->pcr_uid, key, &gmemcache_header, statbuf) )
	{
		return (0);
	}

	utf8_key = utf8_encode((const unsigned char *)key);
	if (!utf8_key)
	{
		return (ENOMEM);
	}

	fs.fs_uid = pcr->pcr_uid;

	error = http_parse(&fs, utf8_key, proxy_ok);
	if (error)
	{
		goto done;
	}

	/* After the call to http parse, the fs structure will have all
	  the right settings except that fs_outputfile will be incomplete.  
	*/

	name_tempfile(pbuf, fs.fs_outputfile);
	fs.fs_outputfile = pbuf;

#ifdef DEBUG
	fprintf(stderr, "Output file for stat is %s, glast=%d\n", fs.fs_outputfile, glast_array_element);
#endif

	/* Set up the stat structure which we will use for the call to http_stat */

	statstruct.orig_uri = key;
	statstruct.statbuf = statbuf;

	error = make_request(&fs, http_stat, (void *) & statstruct, WEBDAV_FS_CLOSE);
	if (error)
	{
		goto done;
	}

	/* we didn't get an error so add the result to the cache. Ignore errors from
	  the cache insert.  If we can't cache it, we don't really care */

	error2 = webdav_memcache_insert(pcr->pcr_uid, key, &gmemcache_header, statbuf);

done:

	free(utf8_key);
	return (error);
}

/*****************************************************************************/

int webdav_statfs(proxy_ok, pcr, key, a_socket, so, statfsbuf)
	int proxy_ok;
	struct webdav_cred *pcr;
	char *key;
	int *a_socket;
	int so;
	struct statfs *statfsbuf;
{
	char pbuf[MAXPATHLEN];
	char *utf8_key;
	int error;
	struct fetch_state fs;
	time_t thetime;

	/* What we are starting with is a pathname - the http part
	  in the key variable.
	 */
	thetime = time(0);
	if (thetime != -1)
	{
		if (gstatfstime && (gstatfstime + WEBDAV_STATFS_TIMEOUT) > thetime)
		{
			bcopy(&gstatfsbuf, statfsbuf, sizeof(gstatfsbuf));
			return (0);
		}
	}
	else
	{
		thetime = 0;							/* if we can't get the right time we'll zero it */
	}

	bzero(&fs, sizeof(fs));
	fs.fs_use_connect = 1;
	fs.fs_socketptr = a_socket;

	utf8_key = utf8_encode((const unsigned char *)key);
	if (!utf8_key)
	{
		return (ENOMEM);
	}

	fs.fs_uid = pcr->pcr_uid;

	error = http_parse(&fs, utf8_key, proxy_ok);
	if (error)
	{
		goto done;
	}

	/* After the call to http parse, the fs structure will have all
	  the right settings except that fs_outputfile will be incomplete.  
	*/

	name_tempfile(pbuf, fs.fs_outputfile);
	fs.fs_outputfile = pbuf;

#ifdef DEBUG
	fprintf(stderr, "Output file for stat is %s, glast=%d\n", fs.fs_outputfile, glast_array_element);
#endif

	error = make_request(&fs, http_statfs, (void *)statfsbuf, WEBDAV_FS_CLOSE);
	if (error)
	{
		goto done;
	}


done:

	bcopy(statfsbuf, &gstatfsbuf, sizeof(gstatfsbuf));
	gstatfstime = thetime;
	free(utf8_key);
	return (error);
}

/*****************************************************************************/

int webdav_mount(proxy_ok, key, a_socket, a_mount_args)
	int proxy_ok;
	char *key;
	int *a_socket;
	int *a_mount_args;
{
	char pbuf[MAXPATHLEN];
	char *utf8_key;
	int error;
	struct fetch_state fs;
	webdav_filetype_t file_type;
	struct webdav_cred cred;


	/* What we are starting with is a pathname - the http part
	  in the key variable.  Someday that may change but this is
	  still the prototype.  Start by initializing the fs_state
	  structure
	 */

	bzero(&fs, sizeof(fs));
	fs.fs_use_connect = 1;
	fs.fs_socketptr = a_socket;

	utf8_key = utf8_encode((const unsigned char *)key);
	if (!utf8_key)
	{
		return (ENOMEM);
	}

	/* now set up the cred structure, we will need it later */

	cred.pcr_flag = 0;
	cred.pcr_uid = getuid();
	cred.pcr_ngroups = 0;


	fs.fs_uid = cred.pcr_uid;					/* mount with callers permisions*/

	error = http_parse(&fs, utf8_key, proxy_ok);
	if (error)
	{
		goto done;
	}

	/* After the call to http parse, the fs structure will have all
	  the right settings except that fs_outputfile will be incomplete.  
	*/

	name_tempfile(pbuf, fs.fs_outputfile);
	fs.fs_outputfile = pbuf;

#ifdef DEBUG
	fprintf(stderr, "Output file for mount is %s, glast=%d\n", fs.fs_outputfile, glast_array_element);
#endif

	error = make_request(&fs, http_mount, (void *)a_mount_args, WEBDAV_FS_CLOSE);
	if (error)
	{
		goto done;
	}

	/* Now make sure that this is a directory and that the URI is valid */
	error = webdav_lookupinfo(proxy_ok, &cred, key, a_socket, &file_type);
	if (error || (file_type != WEBDAV_DIR_TYPE))
	{
		error = ENOENT;
	}

done:

	free(utf8_key);
	return (error);
}

/*****************************************************************************/

int webdav_create(proxy_ok, pcr, key, a_socket, file_type)
	int proxy_ok;
	struct webdav_cred *pcr;
	char *key;
	int *a_socket;
	webdav_filetype_t file_type;
{
	char pbuf[MAXPATHLEN];
	char *utf8_key;
	int error;
	struct fetch_state fs;
	u_int32_t inode = 0;

	/* What we are starting with is a pathname - the http part
	  in the key variable.  Someday that may change but this is
	  still the prototype.  Start by initializing the fs_state
	  structure							 */

	bzero(&fs, sizeof(fs));
	fs.fs_use_connect = 1;
	fs.fs_socketptr = a_socket;

	utf8_key = utf8_encode((const unsigned char *)key);
	if (!utf8_key)
	{
		return (ENOMEM);
	}

	fs.fs_uid = pcr->pcr_uid;

	error = http_parse(&fs, utf8_key, proxy_ok);
	if (error)
	{
		goto error;
	}

	/* After the call to http parse, the fs structure will have all
	  the right settings except that fs_outputfile will be incomplete.  
	  Even though we are not doing a retrievel, we still need to set 
	  pbuf so that http_get_body will have a place to put any 
	  extraneous data that comes back on the reply. */

	name_tempfile(pbuf, fs.fs_outputfile);
	fs.fs_outputfile = pbuf;

	if (file_type == WEBDAV_FILE_TYPE)
	{
		error = make_request(&fs, http_put, (void *) - 1, WEBDAV_FS_CLOSE);
		if (error)
		{
			goto error;
		}
	}
	else
	{
		if (file_type == WEBDAV_DIR_TYPE)
		{
			error = make_request(&fs, http_mkcol, (void *) - 1, WEBDAV_FS_CLOSE);
			if (error)
			{
				goto error;
			}
		}
		else
		{
			error = EFTYPE;
			goto error;
		}
	}

	/* Call webdav_get_inode to generate an inode number for this newly
	 * created object.	If we don't do this the code which gets file_handles
	 * will be very upset.	
	 */
	error = webdav_get_inode(key, strlen(key), TRUE, &inode);
	if (error)
	{
		goto error;
	}

error:

	free(utf8_key);
	return (error);
}

/*****************************************************************************/

int webdav_read_bytes(proxy_ok, pcr, key, a_socket, a_byte_addr, a_size)
	int proxy_ok;
	struct webdav_cred *pcr;
	char *key;
	int *a_socket;
	char **a_byte_addr;
	off_t *a_size;
{
	char *utf8_uri,  *uri;
	char pbuf[MAXPATHLEN];
	int error;
	struct fetch_state fs;
	struct http_state *https;
	struct webdav_read_byte_info byte_info;

	/* initialize, in case there's an error */
	*a_byte_addr = NULL;
	*a_size = 0;

	/* What we are starting with is a pathname - the http part
	  in the key variable.  Someday that may change but this is
	  still the prototype.  Start by initializing the fs_state
	  structure */

	bzero(&fs, sizeof(fs));
	uri = (char *)(key + sizeof(webdav_byte_read_header_t));

	fs.fs_use_connect = 1;
	fs.fs_socketptr = a_socket;

	utf8_uri = utf8_encode((const char *)uri);
	if (!utf8_uri)
	{
		return (ENOMEM);
	}

	fs.fs_uid = pcr->pcr_uid;

	error = http_parse(&fs, utf8_uri, proxy_ok);
	if (error)
	{
		goto first_free;
	}

	/* After the call to http parse, the fs structure will have all
	  the right settings except that it will have set pbuf.	 Even though
	  we are not doing a retrievel, we still need to set pbuf so that
	  get_body will have a place to put any extraneous data that comes
	  back on the reply. */

	name_tempfile(pbuf, fs.fs_outputfile);
	fs.fs_outputfile = pbuf;

	bzero(&byte_info, sizeof(byte_info));
	byte_info.num_bytes = ((webdav_byte_read_header_t *)key)->wd_num_bytes;
	byte_info.byte_start = ((webdav_byte_read_header_t *)key)->wd_byte_start;
	byte_info.uri = utf8_uri;

	https = (struct http_state *)fs.fs_proto;

	error = make_request(&fs, http_read_bytes, (void *) & byte_info, WEBDAV_FS_CLOSE);
	if (error)
	{
		goto first_free;
	}

	*a_byte_addr = byte_info.byte_addr;
	*a_size = byte_info.num_read_bytes;

first_free:

	free(utf8_uri);
	return (error);
}

/*****************************************************************************/

int webdav_rename(proxy_ok, pcr, key, a_socket)
	int proxy_ok;
	struct webdav_cred *pcr;
	char *key;
	int *a_socket;
{
	char *f_utf8_key,  *f_key, 					/* original / "from" */
    *t_utf8_key,  *t_key;						/* new / "to" */
	char pbuf[MAXPATHLEN];
	int error = 0, error2 = 0;
	int from_gindex = -1, to_gindex = -1;
	struct fetch_state fs;
	int length;
	struct http_state *https;
	u_int32_t inode = 0;

	/* What we are starting with is a pathname - the http part
	  in the key variable.  Someday that may change but this is
	  still the prototype.  Start by initializing the fs_state
	  structure */

	bzero(&fs, sizeof(fs));
	f_key = (char *)(key + sizeof(webdav_rename_header_t));

	fs.fs_use_connect = 1;
	fs.fs_socketptr = a_socket;

	f_utf8_key = utf8_encode((const unsigned char *)f_key);
	if (!f_utf8_key)
	{
		return (ENOMEM);
	}

	fs.fs_uid = pcr->pcr_uid;

	error = http_parse(&fs, f_utf8_key, proxy_ok);
	if (error)
	{
		goto first_free;
	}

	/* After the call to http parse, the fs structure will have all
	  the right settings except that fs_outputfile will be incomplete.  
	  Even though we are not doing a retrievel, we still need to set 
	  pbuf so that http_get_body will have a place to put any 
	  extraneous data that comes back on the reply. */

	name_tempfile(pbuf, fs.fs_outputfile);
	fs.fs_outputfile = pbuf;

	/* Now set up the complete second uri */

	length = ((webdav_rename_header_t *)key)->wd_first_uri_size;
	https = (struct http_state *)fs.fs_proto;

	t_key = f_key + length;
	t_utf8_key = utf8_encode((const unsigned char *)t_key);
	if (!t_utf8_key)
	{
		error = ENOMEM;
		goto first_free;
	}

	error = make_request(&fs, http_move, (void *)t_utf8_key, WEBDAV_FS_CLOSE);
	if (error)
	{
		goto done;
	}

	/* Since we sucessfully deleted this thing, get it out of the cache. We'll
	  deal with the inode cache first.	 We can tolerate a thread thinking the
	  rename hasn't happened yet becuase it finds it in the stat cache.  We
	  don't want them to get the wrong inode number, however.
	*/

	/* take the gfile_array lock, to avoid a conflict with webdav_open */
	error = pthread_mutex_lock(&garray_lock);
	if (error)
	{
		goto done;
	}

	error = webdav_get_inode(f_key, strlen(f_key), FALSE, &inode);
	if (inode && !error)
	{
		/* look up the current "from" entry */
		if (!webdav_get_file_handle(f_key, strlen(f_key), &from_gindex))
		{
			if (from_gindex != -1)
			{
				if ((gfile_array[from_gindex].fd == 0) ||
					(memcmp(f_utf8_key, gfile_array[from_gindex].uri, strlen(f_utf8_key))))
				{
					/* nevermind, it didn't match */
					from_gindex = -1;
				}
			}
		}

		error2 = webdav_remove_inode(f_key, strlen(f_key));
		if (!error)
		{
			error = error2;
		}

		error2 = webdav_set_inode(t_key, strlen(t_key), inode);
		if (!error)
		{
			error = error2;
		}
	}

	/* look up the current "to" entry */
	if (!webdav_get_file_handle(t_key, strlen(t_key), &to_gindex))
	{
		if (to_gindex != -1)
		{
			if (gfile_array[to_gindex].fd == 0 ||
				(memcmp(t_utf8_key, gfile_array[to_gindex].uri, strlen(t_utf8_key))))
			{
				/* nevermind, it didn't match */
				to_gindex = -1;
			}
		}
	}

	/* Fix up the "from" entry so that it becomes the new "to" entry.
	  We have to keep the gfile_array index the same, because if the file
	  is open, the kernel is counting on the file handle remaining the same.
	*/
	if (from_gindex != -1)
	{
		if (gfile_array[from_gindex].uri)
		{
			(void)free(gfile_array[from_gindex].uri);
		}
		gfile_array[from_gindex].uri = malloc(strlen(t_utf8_key) + 1);
		strcpy(gfile_array[from_gindex].uri, t_utf8_key);
	}

	/* mark the "to" entry for deletion, or clear it if it's already closed */
	if (to_gindex != -1)
	{
		if (!gfile_array[to_gindex].cachetime)
		{
			gfile_array[to_gindex].deleted = 1;
		}
		else
		{
			CLEAR_GFILE_ENTRY(to_gindex);
		}
	}

	/* set the file handles for both "from" and "to" URLs */
	error2 = webdav_set_file_handle(f_key, strlen(f_key), -1);
	if (!error)
	{
		error = error2;
	}
	error2 = webdav_set_file_handle(t_key, strlen(t_key), from_gindex);
	/* even if from_gindex is -1 */
	if (!error)
	{
		error = error2;
	}

	error = pthread_mutex_unlock(&garray_lock);
	if (error)
	{
		goto done;
	}

	error2 = webdav_memcache_remove(pcr->pcr_uid, f_key, &gmemcache_header);
	if (!error)
	{
		error = error2;
	}

	error2 = webdav_memcache_remove(pcr->pcr_uid, t_key, &gmemcache_header);
	if (!error)
	{
		error = error2;
	}


	/* fall through */

done:

	free(t_utf8_key);

first_free:

	free(f_utf8_key);
	return (error);
}

/*****************************************************************************/

int webdav_delete(proxy_ok, pcr, key, a_socket, file_type)
	int proxy_ok;
	struct webdav_cred *pcr;
	char *key;
	int *a_socket;
	webdav_filetype_t file_type;
{
	char pbuf[MAXPATHLEN];
	char *utf8_key;
	int error = 0, error2 = 0;
	int arrayelem = -1;
	struct fetch_state fs;

	/* What we are starting with is a pathname - the http part
	  in the key variable.  Someday that may change but this is
	  still the prototype.  Start by initializing the fs_state
	  structure */

	bzero(&fs, sizeof(fs));
	fs.fs_use_connect = 1;
	fs.fs_socketptr = a_socket;

	utf8_key = utf8_encode((const unsigned char *)key);
	if (!utf8_key)
	{
		return (ENOMEM);
	}

	fs.fs_uid = pcr->pcr_uid;

	error = http_parse(&fs, utf8_key, proxy_ok);
	if (error)
	{
		goto done;
	}

	/* After the call to http parse, the fs structure will have all
	  the right settings except that fs_outputfile will be incomplete.  
	  Even though we are not doing a retrievel, we still need to set 
	  pbuf so that http_get_body will have a place to put any 
	  extraneous data that comes back on the reply. */

	name_tempfile(pbuf, fs.fs_outputfile);
	fs.fs_outputfile = pbuf;

	if (file_type == WEBDAV_FILE_TYPE)
	{
		error = make_request(&fs, http_delete, (void *)0, WEBDAV_FS_CLOSE);
		if (error)
		{
			goto done;
		}
	}
	else
	{
		if (file_type == WEBDAV_DIR_TYPE)
		{
			error = make_request(&fs, http_delete_dir, (void *)0, WEBDAV_FS_CLOSE);
			if (error)
			{
				goto done;
			}
		}
		else
		{
			error = EFTYPE;
			goto done;
		}
	}

	/* Since we sucessfully deleted this thing, get it out of the cache.
	  First, get it out of the inode hash.	 It it is in the inode hash
	  and the stat cache, the stat cache will prevail and the file will
	  just look like it hasn't been deleted yet.  That's an acceptable
	  race.  */

	/* take the gfile_array lock, to avoid a conflict with webdav_open */
	error = pthread_mutex_lock(&garray_lock);
	if (error)
	{
		goto done;
	}

	/* take the entry out of the gfile_array */
	if (!webdav_get_file_handle(key, strlen(key), &arrayelem))
	{
		if ((arrayelem != -1) && (gfile_array[arrayelem].fd != 0) && (!memcmp(utf8_key, gfile_array[arrayelem].uri, strlen(utf8_key))))
		{
			if (!gfile_array[arrayelem].cachetime)
			{
				/* It's open, so mark it deleted.  The entry will be
				  cleared when it's closed.  */
				gfile_array[arrayelem].deleted = 1;
			}
			else
			{
				CLEAR_GFILE_ENTRY(arrayelem);
			}
		}
	}

	error2 = webdav_remove_inode(key, strlen(key));
	error = pthread_mutex_unlock(&garray_lock);
	if (error)
	{
		goto done;
	}

	error = webdav_memcache_remove(pcr->pcr_uid, key, &gmemcache_header);

	if (!error)
	{
		error = error2;
	}

	/* fall through */

done:

	free(utf8_key);
	return (error);
}

/*****************************************************************************/

int webdav_fsync(proxy_ok, pcr, file_handle, a_socket)
	int proxy_ok;
	struct webdav_cred *pcr;
	webdav_filehandle_t file_handle;
	int *a_socket;
{
	int error, newerror;
	struct fetch_state fs;
	struct timeval tv;
	struct timezone tz;
	struct webdav_put_struct putinfo;

	putinfo.locktoken = NULL;
	
	error = pthread_mutex_lock(&garray_lock);
	if (error)
	{
		goto done;
	}

	if (!gfile_array[file_handle].fd)
	{
		/* Trying to close something we did not open
		 * uh oh
		 */
		error = EBADF;
		goto unlock_finish;
	}

	bzero(&fs, sizeof(fs));
	fs.fs_use_connect = 1;
	fs.fs_socketptr = a_socket;
	fs.fs_uid = pcr->pcr_uid;

	error = http_parse(&fs, gfile_array[file_handle].uri, proxy_ok);
	if (error)
	{
		goto unlock_finish;
	}

	if (gfile_array[file_handle].download_status == WEBDAV_DOWNLOAD_IN_PROGRESS)
	{
		/* The kernel should not send us an fsync until the file is downloaded */
		error = EIO;
		goto unlock_finish;
	}
	
	putinfo.fd = gfile_array[file_handle].fd;
	if ( gfile_array[file_handle].lockdata.locktoken != NULL )
	{
		putinfo.locktoken = malloc(strlen(gfile_array[file_handle].lockdata.locktoken) + 1);
		if ( putinfo.locktoken != NULL )
		{
			strcpy(putinfo.locktoken, gfile_array[file_handle].lockdata.locktoken);
		}
		else
		{
			goto unlock_finish;
		}
	}
	
	error = pthread_mutex_unlock(&garray_lock);
	if (error)
	{
		goto done;
	}

	error = make_request(&fs, http_put, (void *) &putinfo, WEBDAV_FS_CLOSE);
	if (error)
	{
		goto done;
	}

	/* clear out the time so that the next statfs will get the value from
	 * the server
	 */
	gstatfstime = 0;

	/* Since we just wrote the file out to the server, update our internal
	  idea of the modtime, for use in the get-if-modified-since when the 
	  file is re-opened.
	*/
	/* *** it would be better to use the date from the response *** */
	gettimeofday(&tv, &tz);
	
	/* grab the lock again */
	error = pthread_mutex_lock(&garray_lock);
	if (error)
	{
		goto done;
	}
	
	/* change the modtime */
	gfile_array[file_handle].modtime = tv.tv_sec;

unlock_finish:

	newerror = pthread_mutex_unlock(&garray_lock);
	if (newerror)
	{
		if (!error)
		{
			error = newerror;
		}
		goto done;
	}

done:

	if ( putinfo.locktoken != NULL )
	{
		free(putinfo.locktoken);
	}

	return (error);
}

/*****************************************************************************/

int webdav_refreshdir(proxy_ok, pcr, file_handle, a_socket)
	int proxy_ok;
	struct webdav_cred *pcr;
	webdav_filehandle_t file_handle;
	int *a_socket;
{
	int error, newerror;
	struct fetch_state fs;
	char pbuf[MAXPATHLEN];

	error = pthread_mutex_lock(&garray_lock);
	if (error)
	{
		goto done;
	}

	if (!gfile_array[file_handle].fd)
	{
		/* Trying to refresh something we did not open -- uh oh */
		error = EBADF;
		goto unlock_finish;
	}

	bzero(&fs, sizeof(fs));
	fs.fs_use_connect = 1;
	fs.fs_socketptr = a_socket;
	fs.fs_uid = pcr->pcr_uid;

	error = http_parse(&fs, gfile_array[file_handle].uri, proxy_ok);
	if (error)
	{
		goto unlock_finish;
	}

	/* After the call to http parse, the fs structure will have all
	  the right setting to do the retrieval except that the output file
	  will be going to the wrong place.  We'll update that here by adding
	  the prefix for our cache directory and then go get the file. Note that
	  it is possible for us to have a null file becuase this is the root.
	  We'll use the generic name in that case.*/

	name_tempfile(pbuf, fs.fs_outputfile);
	fs.fs_outputfile = pbuf;

	error = make_request(&fs, http_refreshdir, (void *) & gfile_array[file_handle], WEBDAV_FS_CLOSE);
	if (error)
	{
		goto unlock_finish;
	}

unlock_finish:

	newerror = pthread_mutex_unlock(&garray_lock);
	if (newerror)
	{
		if (!error)
		{
			error = newerror;
		}
		goto done;
	}

done:

	return (error);
}

/*****************************************************************************/

/* This routine assumes that the caller has the garray_lock
 * already held.  This is to make things more convenient for
 * the pulse thread which is the only caller of this routine
 */

int webdav_lock(proxy_ok, file_array_elem, a_socket)
	int proxy_ok;
	struct file_array_element *file_array_elem;
	int *a_socket;
{
	int error;
	struct fetch_state fs;

	bzero(&fs, sizeof(fs));
	fs.fs_use_connect = 1;
	fs.fs_socketptr = a_socket;
	fs.fs_uid = file_array_elem->uid;
	error = http_parse(&fs, file_array_elem->uri, proxy_ok);
	if (error)
	{
		goto done;
	}

	error = make_request(&fs, http_lock, (void *) & (file_array_elem->lockdata), WEBDAV_FS_CLOSE);
	if (error)
	{
		goto done;
	}

done:

	return (error);
}

/*****************************************************************************/
