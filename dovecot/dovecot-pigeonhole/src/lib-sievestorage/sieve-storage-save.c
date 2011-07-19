/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "hostpid.h"
#include "ioloop.h"
#include "array.h"
#include "buffer.h"
#include "ostream.h"
#include "str.h"
#include "eacces-error.h"

#include "sieve-script.h"

#include "sieve-storage-private.h"
#include "sieve-storage-script.h"
#include "sieve-storage-save.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <utime.h>
#include <sys/stat.h>

struct sieve_save_context {
	pool_t pool;

	struct sieve_storage *storage;
	const char *scriptname;
	struct sieve_script *scriptobject;

	struct istream *input;
	struct ostream *output;
	int fd;
	const char *tmp_path;

	unsigned int failed:1;
	unsigned int moving:1;
	unsigned int finished:1;
};

static const char *sieve_generate_tmp_filename(const char *scriptname)
{
	static struct timeval last_tv = { 0, 0 };
	struct timeval tv;

	/* use secs + usecs to guarantee uniqueness within this process. */
	if (ioloop_timeval.tv_sec > last_tv.tv_sec ||
		(ioloop_timeval.tv_sec == last_tv.tv_sec &&
		ioloop_timeval.tv_usec > last_tv.tv_usec)) {
		tv = ioloop_timeval;
	} else {
		tv = last_tv;
		if (++tv.tv_usec == 1000000) {
			tv.tv_sec++;
			tv.tv_usec = 0;
		}
	}
	last_tv = tv;

	if ( scriptname == NULL )
		return t_strdup_printf("NULL_%s.M%sP%s.%s.sieve", dec2str(tv.tv_sec), 
			dec2str(tv.tv_usec), my_pid, my_hostname);

	return t_strdup_printf
		("%s-%s.M%sP%s.%s.sieve", scriptname, dec2str(tv.tv_sec), 
			dec2str(tv.tv_usec), my_pid, my_hostname);
}

static int sieve_storage_create_tmp
(struct sieve_storage *storage, const char *scriptname, const char **fpath_r)
{
	struct stat st;
	unsigned int prefix_len;
	const char *tmp_fname = NULL;
	string_t *path;
	int fd;

	path = t_str_new(256);	
	str_append(path, storage->dir);
	str_append(path, "/tmp/");
	prefix_len = str_len(path);

	for (;;) {
		tmp_fname = sieve_generate_tmp_filename(scriptname);
		str_truncate(path, prefix_len);
		str_append(path, tmp_fname);

		/* stat() first to see if it exists. pretty much the only
		   possibility of that happening is if time had moved
		   backwards, but even then it's highly unlikely. */
		if (stat(str_c(path), &st) == 0) {
			/* try another file name */	
		} else if (errno != ENOENT) {
			sieve_storage_set_critical(storage,
				"stat(%s) failed: %m", str_c(path));
			return -1;
		} else {
			/* doesn't exist */
			mode_t old_mask = umask(0777 & ~(storage->file_create_mode));
			fd = open(str_c(path),
				O_WRONLY | O_CREAT | O_TRUNC | O_EXCL, 0777);
			umask(old_mask);

			if (fd != -1 || errno != EEXIST)
				break;
			/* race condition between stat() and open().
				highly unlikely. */
		}
	}

	*fpath_r = str_c(path);
	if (fd == -1) {
		if (ENOSPACE(errno)) {
			sieve_storage_set_error(storage, SIEVE_ERROR_NO_SPACE,
				"Not enough disk space");
		} else {
			sieve_storage_set_critical(storage,
				"open(%s) failed: %m", str_c(path));
		}
	} 

	return fd;
}


static int sieve_storage_script_move(struct sieve_save_context *ctx,
  const char *dst)
{
	int failed;

	T_BEGIN {

		/* Using rename() to ensure existing files are replaced
		 * without conflicts with other processes using the same
		 * file. The kernel wont fully delete the original until
		 * all processes have closed the file.
		 */
		if (rename(ctx->tmp_path, dst) == 0)
			failed = FALSE;
		else {
			failed = TRUE;
			if ( ENOSPACE(errno) ) {
				sieve_storage_set_error
				  (ctx->storage, SIEVE_ERROR_NO_SPACE, "Not enough disk space");
			} else if ( errno == EACCES ) {
				sieve_storage_set_critical
				  (ctx->storage, "%s", eacces_error_get("rename", dst));
			} else {
				sieve_storage_set_critical
				  (ctx->storage, "rename(%s, %s) failed: %m", ctx->tmp_path, dst);
			}
		}

		/* Always destroy temp file */
		(void)unlink(ctx->tmp_path);

	} T_END;

	return !failed;
}

struct sieve_save_context *
sieve_storage_save_init(struct sieve_storage *storage,
	const char *scriptname, struct istream *input)
{
	struct sieve_save_context *ctx;
	pool_t pool;
	const char *path;

	if ( scriptname != NULL ) {
		/* Validate script name */
		if ( !sieve_script_name_is_valid(scriptname) ) {
			sieve_storage_set_error(storage, 
				SIEVE_ERROR_BAD_PARAMS,
				"Invalid script name '%s'.", scriptname);
			return NULL;
		}

		/* Prevent overwriting the active script link when it resides in the 
		 * sieve storage directory.
		 */
		if ( *(storage->link_path) == '\0' ) {
			const char *svext;
			size_t namelen;

			svext = strrchr(storage->active_fname, '.');
			namelen = svext - storage->active_fname;
			if ( svext != NULL && strncmp(svext+1, "sieve", 5) == 0 &&
				strlen(scriptname) == namelen && 
				strncmp(scriptname, storage->active_fname, namelen) == 0 ) 
			{
				sieve_storage_set_error(
					storage, SIEVE_ERROR_BAD_PARAMS, 
					"Script name '%s' is reserved for internal use.", scriptname); 
				return NULL;
			}
		}
	}

	pool = pool_alloconly_create("sieve_save_context", 4096);
	ctx = p_new(pool, struct sieve_save_context, 1);
	ctx->pool = pool;
	ctx->storage = storage;
	ctx->scriptname = scriptname;
	ctx->scriptobject = NULL;

	T_BEGIN {
		ctx->fd = sieve_storage_create_tmp(storage, scriptname, &path);
		if (ctx->fd == -1) {
			ctx->failed = TRUE;
			pool_unref(&pool);
			ctx = NULL;
		} else {
			ctx->input = input;
			ctx->output = o_stream_create_fd(ctx->fd, 0, FALSE);
			ctx->tmp_path = p_strdup(pool, path);
			ctx->failed = FALSE;
		}
	} T_END;

	return ctx;
}

int sieve_storage_save_continue(struct sieve_save_context *ctx)
{
	if (o_stream_send_istream(ctx->output, ctx->input) < 0) {
		sieve_storage_set_critical(ctx->storage,
			"o_stream_send_istream(%s) failed: %m", ctx->tmp_path);
		ctx->failed = TRUE;
		return -1;
	}
	return 0;
}

int sieve_storage_save_finish(struct sieve_save_context *ctx)
{
	int output_errno;

	ctx->finished = TRUE;
	if ( ctx->failed && ctx->fd == -1 ) {
		/* tmp file creation failed */
		return -1;
	}

	T_BEGIN {
		output_errno = ctx->output->stream_errno;
		o_stream_destroy(&ctx->output);

		if ( fsync(ctx->fd) < 0 ) {
			sieve_storage_set_critical(ctx->storage,
				"fsync(%s) failed: %m", ctx->tmp_path);
			ctx->failed = TRUE;
		}
		if ( close(ctx->fd) < 0 ) {
			sieve_storage_set_critical(ctx->storage,
				"close(%s) failed: %m", ctx->tmp_path);
			ctx->failed = TRUE;
		}
		ctx->fd = -1;

		if ( ctx->failed ) {
			/* delete the tmp file */
			if (unlink(ctx->tmp_path) < 0 && errno != ENOENT) 
				i_warning("sieve-storage: Unlink(%s) failed: %m", ctx->tmp_path);

			errno = output_errno;
			if ( ENOSPACE(errno) ) {
				sieve_storage_set_error(ctx->storage, SIEVE_ERROR_NO_SPACE,
					"Not enough disk space");
			} else if ( errno != 0 ) {
				sieve_storage_set_critical(ctx->storage,
					"write(%s) failed: %m", ctx->tmp_path);
			}
		}
	} T_END;

	return ( ctx->failed ? -1 : 0 );
}

static void sieve_storage_save_destroy(struct sieve_save_context **ctx)
{
	if ((*ctx)->scriptobject != NULL)
		sieve_script_unref(&((*ctx)->scriptobject));

	pool_unref(&(*ctx)->pool);
	*ctx = NULL;
}

struct sieve_script *sieve_storage_save_get_tempscript
(struct sieve_save_context *ctx)
{
	const char *scriptname = 
		( ctx->scriptname == NULL ? "" : ctx->scriptname ); 

	if (ctx->failed) 
		return NULL;

	if ( ctx->scriptobject != NULL )
		return ctx->scriptobject;

	ctx->scriptobject = sieve_storage_script_init_from_path
		(ctx->storage, ctx->tmp_path, scriptname);	

	if ( ctx->scriptobject == NULL ) {
		if ( ctx->storage->error_code == SIEVE_ERROR_NOT_FOUND ) {
			sieve_storage_set_critical(ctx->storage, 
				"save: Temporary script file with name '%s' got lost, "
				"which should not happen (possibly deleted externally).", 
				ctx->tmp_path);
		}
		return NULL;
	}

	return ctx->scriptobject;
}

int sieve_storage_save_commit(struct sieve_save_context **ctx)
{
	const char *dest_path;
	bool failed = FALSE;

	i_assert((*ctx)->output == NULL);
	i_assert((*ctx)->finished);
	i_assert((*ctx)->scriptname != NULL);

	T_BEGIN {
		dest_path = t_strconcat((*ctx)->storage->dir, "/", 
			(*ctx)->scriptname, ".sieve", NULL);

		failed = !sieve_storage_script_move((*ctx), dest_path);
	} T_END;

	sieve_storage_save_destroy(ctx);

	return ( failed ? -1 : 0 );
}

void sieve_storage_save_cancel(struct sieve_save_context **ctx)
{
	(*ctx)->failed = TRUE;

	if (!(*ctx)->finished) 
		(void)sieve_storage_save_finish(*ctx);
	else
		(void)unlink((*ctx)->tmp_path);

	i_assert((*ctx)->output == NULL);

	sieve_storage_save_destroy(ctx);
}
