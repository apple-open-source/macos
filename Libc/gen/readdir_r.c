#include <sys/types.h>
#include <dirent.h>
#include <pthread.h>

/*
 * Synopsis:
 * dir_table is a global static array of pthread mutexes.  The array is NMUTEXES
 * in length (NMUTEXES * sizeof(pthread_mutex_t)).
 *
 * Whenever a call to readdir_r is made, the element in the array indexed by the
 * modulus of the file descriptor associated with dirp is locked, and readdir is
 * called on dirp.  The result is returned in readdir_r semantics and the
 * element in dir_table is unlocked.
 */

static pthread_mutex_t dir_table[]  = {
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER
#define	NMUTEXES	8
};

int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result)
{
	int ret;
	int mindex = dirp->dd_fd % NMUTEXES;
	struct dirent *tmpdp;

	pthread_mutex_lock(&dir_table[mindex]);

	tmpdp = readdir(dirp);
	if( tmpdp == NULL ) {
		*result = NULL;
		ret = errno;
		pthread_mutex_unlock(&dir_table[mindex]);
		return ret;
	}

	memcpy(entry, tmpdp, sizeof(struct dirent));
	*result = entry;

	pthread_mutex_unlock(&dir_table[mindex]);
	return 0;
}


