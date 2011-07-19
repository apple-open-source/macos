/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str.h"
#include "sieve-storage-private.h"
#include "sieve-storage-script.h"
#include "sieve-storage-quota.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

bool sieve_storage_quota_validsize
(struct sieve_storage *storage, size_t size, uint64_t *limit_r)
{
	uint64_t max_size;

	max_size = sieve_max_script_size(storage->svinst);
	if ( max_size > 0 && size > max_size ) {
		*limit_r = max_size;
		return FALSE;
	}

	return TRUE;
}

int sieve_storage_quota_havespace
(struct sieve_storage *storage, const char *scriptname, size_t size,
	enum sieve_storage_quota *quota_r, uint64_t *limit_r)
{	
	struct dirent *dp;
	DIR *dirp;
	uint64_t script_count = 1;
	uint64_t script_storage = size;
	int result = 1;

	*limit_r = 0;
	*quota_r = SIEVE_STORAGE_QUOTA_NONE;

	/* Check the script size */
	if ( !sieve_storage_quota_validsize(storage, size, limit_r) ) {
		*quota_r = SIEVE_STORAGE_QUOTA_MAXSIZE;
        return 0;
    }

	/* Do we need to scan the storage (quota enabled) ? */
	if ( storage->max_scripts == 0 && storage->max_storage == 0 ) {
		return 1;
	}

	/* Open the directory */
	if ( (dirp = opendir(storage->dir)) == NULL ) {
		sieve_storage_set_critical
			(storage, "quota: opendir(%s) failed: %m", storage->dir);
		return -1;
	}

	/* Scan all files */
	for (;;) {
		const char *name;
		bool replaced = FALSE;

		/* Read next entry */
		errno = 0;
		if ( (dp = readdir(dirp)) == NULL ) {
			if ( errno != 0 ) {
				sieve_storage_set_critical
		            (storage, "quota: readdir(%s) failed: %m", storage->dir);
				result = -1;
			}
			break;
		}

		/* Parse filename */
		name = sieve_storage_file_get_scriptname(storage, dp->d_name);	

		/* Ignore non-script files */
		if ( name == NULL )
			continue;
			
		/* Don't list our active sieve script link if the link 
		 * resides in the script dir (generally a bad idea).
		 */
		if ( *(storage->link_path) == '\0' && 
			strcmp(storage->active_fname, dp->d_name) == 0 )
			continue;

		if ( strcmp(name, scriptname) == 0 )
			replaced = TRUE;

		/* Check cont quota if necessary */
		if ( storage->max_scripts > 0 ) {
			if ( !replaced ) {
				script_count++;
			
				if ( script_count > storage->max_scripts ) {				
					*quota_r = SIEVE_STORAGE_QUOTA_MAXSCRIPTS;
					*limit_r = storage->max_scripts;
					result = 0;
					break;
				}
			}
		}

		/* Check storage quota if necessary */
		if ( storage->max_storage > 0 ) { 
			const char *path;
			struct stat st;
			int ret;

			path = t_strconcat(storage->dir, "/", dp->d_name, NULL);
		
			if ( (ret=stat(path, &st)) < 0 ) {
				i_warning
					("sieve-storage: quota: stat(%s) failed: %m", path);
				continue;
			}

			if ( !replaced ) {
				script_storage += st.st_size;

				if ( script_storage > storage->max_storage ) {
					*quota_r = SIEVE_STORAGE_QUOTA_MAXSTORAGE;
					*limit_r = storage->max_storage;
					result = 0;
					break;	
				}
			}
		}
	}

	/* Close directory */
	if ( closedir(dirp) < 0 ) {
		sieve_storage_set_critical
			(storage, "quota: closedir(%s) failed: %m", storage->dir);
	}

	return result;
}


	
    
