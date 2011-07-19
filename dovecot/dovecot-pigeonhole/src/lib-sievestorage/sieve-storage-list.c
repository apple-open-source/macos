/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str.h"
#include "sieve-storage-private.h"
#include "sieve-storage-script.h"
#include "sieve-storage-list.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>

struct sieve_list_context {
	pool_t pool;
	struct sieve_storage *storage;

	const char *active;
	const char *dir;
	DIR *dirp;

	unsigned int seen_active:1; // Just present for assertions
};

struct sieve_list_context *sieve_storage_list_init
(struct sieve_storage *storage)
{	
	struct sieve_list_context *ctx;
	const char *active = NULL;
	pool_t pool;
	DIR *dirp;

	/* Open the directory */
	if ( (dirp = opendir(storage->dir)) == NULL ) {
		sieve_storage_set_critical(storage, "opendir(%s) failed: %m", storage->dir);
		return NULL;
	}

	T_BEGIN {
		/* Get the name of the active script */
		if ( sieve_storage_get_active_scriptfile(storage, &active) < 0) {
			ctx = NULL;
		} else {
			pool = pool_alloconly_create("sieve_list_context", 4096);
			ctx = p_new(pool, struct sieve_list_context, 1);
			ctx->pool = pool;
			ctx->storage = storage;
			ctx->dirp = dirp;
			ctx->active = ( active != NULL ? p_strdup(pool, active) : NULL );
			ctx->seen_active = FALSE;
		}
	} T_END;
		
	return ctx;
}

const char *sieve_storage_list_next
(struct sieve_list_context *ctx, bool *active)
{
	const struct sieve_storage *storage = ctx->storage;
	struct dirent *dp;
	const char *scriptname;

	*active = FALSE;

	for (;;) {
		if ( (dp = readdir(ctx->dirp)) == NULL )
			return NULL;

		scriptname = sieve_storage_file_get_scriptname
			(storage, dp->d_name);	
		
		if (scriptname != NULL ) {
			/* Don't list our active sieve script link if the link 
			 * resides in the script dir (generally a bad idea).
			 */
			if ( *(storage->link_path) == '\0' && 
				strcmp(storage->active_fname, dp->d_name) == 0 )
				continue;
		
			break;
		}
	}

	if ( ctx->active != NULL && strcmp(dp->d_name, ctx->active) == 0 ) {
		*active = TRUE;
		ctx->active = NULL;
	}

	return scriptname;
}

int sieve_storage_list_deinit(struct sieve_list_context **ctx)
{
	if (closedir((*ctx)->dirp) < 0) {
		sieve_storage_set_critical((*ctx)->storage, "closedir(%s) failed: %m",
			(*ctx)->storage->dir);
	}

	pool_unref(&(*ctx)->pool);
	*ctx = NULL;
	return 1;
}


	
    
