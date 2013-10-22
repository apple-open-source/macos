/*
 * prof_file.c ---- routines that manipulate an individual profile file.
 */

#include "prof_int.h"

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <stddef.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef HAVE_PWD_H
#include <pwd.h>
#endif

#if defined(_WIN32)
#include <io.h>
#define HAVE_STAT	
#define stat _stat
#endif

#ifdef __APPLE__
#include <notify.h>
#include <syslog.h>
#endif

struct global_shared_profile_data {
	/* This is the head of the global list of shared trees */
	prf_data_t trees;
	/* Lock for above list.  */
	pthread_mutex_t mutex;
};
#define g_shared_trees		(krb5int_profile_shared_data.trees)
#define g_shared_trees_mutex	(krb5int_profile_shared_data.mutex)


#define APPLE_NOTIFICATION_NAME "com.apple.Kerberos.configuration-changed"
static int notify_token = -1;

static struct global_shared_profile_data krb5int_profile_shared_data = {
    0,
    PTHREAD_MUTEX_INITIALIZER
};

void
profile_library_initializer(void)
{
}

static void profile_free_file_data(prf_data_t);

#if 0

#define scan_shared_trees_locked()				\
	{							\
	    prf_data_t d;					\
	    k5_mutex_assert_locked(&g_shared_trees_mutex);	\
	    for (d = g_shared_trees; d; d = d->next) {		\
		assert(d->magic == PROF_MAGIC_FILE_DATA);	\
		assert((d->flags & PROFILE_FILE_SHARED) != 0);	\
		assert(d->filespec[0] != 0);			\
		assert(d->fslen <= 1000); /* XXX */		\
		assert(d->filespec[d->fslen] == 0);		\
		assert(d->fslen = strlen(d->filespec));		\
		assert(d->root != NULL);			\
	    }							\
	}

#define scan_shared_trees_unlocked()			\
	{						\
	    int r;					\
	    r = pthread_mutex_lock(&g_shared_trees_mutex);	\
	    assert (r == 0);				\
	    scan_shared_trees_locked();			\
	    pthread_mutex_unlock(&g_shared_trees_mutex);	\
	}

#else

#define scan_shared_trees_locked()	{ ; }
#define scan_shared_trees_unlocked()	{ ; }

#endif

static int rw_access(const_profile_filespec_t filespec)
{
#ifdef HAVE_ACCESS
	if (access(filespec, W_OK) == 0)
		return 1;
	else
		return 0;
#else
	/*
	 * We're on a substandard OS that doesn't support access.  So
	 * we kludge a test using stdio routines, and hope fopen
	 * checks the r/w permissions.
	 */
	FILE	*f;

	f = fopen(filespec, "r+");
	if (f) {
		fclose(f);
		return 1;
	}
	return 0;
#endif
}

static int r_access(prf_data_t data)
{
#ifdef __APPLE__
    if (data->uid != geteuid())
	return 0;
    return 1;
#else
    const_profile_filespec_t filespec = data->filespec;
#ifdef HAVE_ACCESS
	if (access(filespec, R_OK) == 0)
		return 1;
	else
		return 0;
#else
	/*
	 * We're on a substandard OS that doesn't support access.  So
	 * we kludge a test using stdio routines, and hope fopen
	 * checks the r/w permissions.
	 */
	FILE	*f;

	f = fopen(filespec, "r");
	if (f) {
		fclose(f);
		return 1;
	}
	return 0;
#endif
#endif /* __APPLE__ */
}

int profile_file_is_writable(prf_file_t profile)
{
    if (profile && profile->data) {
        return rw_access(profile->data->filespec);
    } else {
        return 0;
    }
}

prf_data_t
profile_make_prf_data(const char *filename)
{
    prf_data_t d;
    size_t len, flen, slen;
    char *fcopy;

    flen = strlen(filename);
    slen = offsetof(struct _prf_data_t, filespec);
    len = slen + flen + 1;
    if (len < sizeof(struct _prf_data_t))
	len = sizeof(struct _prf_data_t);
    d = malloc(len);
    if (d == NULL)
	return NULL;
    memset(d, 0, len);
    fcopy = (char *) d + slen;
    assert(fcopy == d->filespec);
    strlcpy(fcopy, filename, flen + 1);
    d->refcount = 1;
    d->comment = NULL;
    d->magic = PROF_MAGIC_FILE_DATA;
    d->root = NULL;
    d->next = NULL;
    d->fslen = flen;
#ifdef __APPLE__
    d->uid = geteuid();
    d->flags |= PROFILE_FILE_INVALID;
#endif
    return d;
}

errcode_t profile_open_file(const_profile_filespec_t filespec,
			    prf_file_t *ret_prof)
{
	prf_file_t	prf;
	errcode_t	retval;
	char		*home_env = 0;
	prf_data_t	data;
	char		*expanded_filename;

	scan_shared_trees_unlocked();

	prf = malloc(sizeof(struct _prf_file_t));
	if (!prf)
		return ENOMEM;
	memset(prf, 0, sizeof(struct _prf_file_t));
	prf->magic = PROF_MAGIC_FILE;

	if (filespec[0] == '~' && filespec[1] == '/') {
		home_env = getenv("HOME");
#ifdef HAVE_PWD_H
		if (home_env == NULL) {
		    uid_t uid;
		    struct passwd *pw, pwx;
		    char pwbuf[BUFSIZ];

		    uid = getuid();
		    if (!k5_getpwuid_r(uid, &pwx, pwbuf, sizeof(pwbuf), &pw)
			&& pw != NULL && pw->pw_dir[0] != 0)
			home_env = pw->pw_dir;
		}
#endif
	}
	if (home_env) {
	    if (asprintf(&expanded_filename, "%s%s", home_env,
			 filespec + 1) < 0)
		expanded_filename = 0;
	} else
	    expanded_filename = strdup(filespec);
	if (expanded_filename == 0) {
	    free(prf);
	    return ENOMEM;
	}

	retval = pthread_mutex_lock(&g_shared_trees_mutex);
	if (retval) {
	    free(expanded_filename);
	    free(prf);
	    scan_shared_trees_unlocked();
	    return retval;
	}

#ifdef __APPLE__
	/*
	 * Check semaphore, and if set, invalidate all shared pages.
	 */
	if (notify_token != -1) {
	    int check = 0;
	    if (notify_check(notify_token, &check) == 0 && check)
		for (data = g_shared_trees; data; data = data->next)
		    data->flags |= PROFILE_FILE_INVALID;
	}
#endif

	scan_shared_trees_locked();
	for (data = g_shared_trees; data; data = data->next) {
	    if (!strcmp(data->filespec, expanded_filename)
		/* Check that current uid has read access.  */
		&& r_access(data))
		break;
	}
	if (data) {
	    data->refcount++;
	    (void) pthread_mutex_unlock(&g_shared_trees_mutex);
	    retval = profile_update_file_data(data);
	    free(expanded_filename);
	    if (retval == 0) {
		prf->data = data;
		*ret_prof = prf;
	    } else {
		data->refcount--;
		free(prf);
		*ret_prof = NULL;
	    }
	    scan_shared_trees_unlocked();
	    return retval;
	}
#ifdef __APPLE__
	/* lets find a matching cache entry  */
	for (data = g_shared_trees; data; data = data->next) {
	    if (!strcmp(data->filespec, expanded_filename) &&
		(data->flags & PROFILE_FILE_SHARED))
	    {
		profile_dereference_data_locked(data);
		break;
	    }
	}
	/* Didn't fine one, so lets remove last cached entry. This
	   make this a LRU with the length of the maxium length is was
	   the concurrent used entries.
	*/
	if (data == NULL) {
	    prf_data_t	last = NULL;

	    for (data = g_shared_trees; data; data = data->next) {
		if ((data->flags & PROFILE_FILE_SHARED) && data->refcount == 1)
		    last = data;
	    }
	    if (last)
		profile_dereference_data_locked(last);
	}
#endif
	(void) pthread_mutex_unlock(&g_shared_trees_mutex);
	data = profile_make_prf_data(expanded_filename);
	if (data == NULL) {
	    free(prf);
	    free(expanded_filename);
	    return ENOMEM;
	}
	free(expanded_filename);
	prf->data = data;

	retval = pthread_mutex_init(&data->lock, NULL);
	if (retval) {
	    free(data);
	    free(prf);
	    return retval;
	}

	retval = profile_update_file_data(prf->data);
	if (retval != 0 && retval != ENOENT) {
		profile_close_file(prf);
		return retval;
	}

	retval = pthread_mutex_lock(&g_shared_trees_mutex);
	if (retval != 0 && retval != ENOENT) {
	    profile_close_file(prf);
	    scan_shared_trees_unlocked();
	    return retval;
	}
	scan_shared_trees_locked();
	data->flags |= PROFILE_FILE_SHARED;
#ifdef __APPLE__
	data->refcount++;
#endif
	data->next = g_shared_trees;
	g_shared_trees = data;
	scan_shared_trees_locked();

#ifdef __APPLE__
	if (notify_token == -1)
	    notify_register_check(APPLE_NOTIFICATION_NAME, &notify_token);
#endif

	(void) pthread_mutex_unlock(&g_shared_trees_mutex);

	*ret_prof = prf;
	return 0;
}

void profile_configuration_updated(void)
{
#ifdef __APPLE__
    notify_post(APPLE_NOTIFICATION_NAME);
#endif
}

errcode_t profile_update_file_data(prf_data_t data)
{
	errcode_t retval;
#ifndef __APPLE__
#ifdef HAVE_STAT
	struct stat st;
	unsigned long frac;
	time_t now;
#endif
#endif /* !__APPLE__ */
	FILE *f;

	retval = pthread_mutex_lock(&data->lock);
	if (retval)
	    return retval;

#ifdef __APPLE__
	retval = (data->flags & (PROFILE_FILE_INVALID|PROFILE_FILE_HAVE_DATA));
	if (retval == PROFILE_FILE_HAVE_DATA) {
	    retval = 0;
	    if (data->root == NULL)
		retval = ENOENT;
	    pthread_mutex_unlock(&data->lock);
	    return retval;
	}
	data->flags &= ~PROFILE_FILE_INVALID; /* invalid would be striped of below, but humor us */
#else /* !__APPLE__ */
#ifdef HAVE_STAT
	now = time(0);
	if (now == data->last_stat && data->root != NULL) {
	    pthread_mutex_unlock(&data->lock);
	    return 0;
	}
	if (stat(data->filespec, &st)) {
	    retval = errno;
	    pthread_mutex_unlock(&data->lock);
	    return retval;
	}
	data->last_stat = now;
#if defined HAVE_STRUCT_STAT_ST_MTIMENSEC
	frac = st.st_mtimensec;
#elif defined HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC
	frac = st.st_mtimespec.tv_nsec;
#elif defined HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
	frac = st.st_mtim.tv_nsec;
#else
	frac = 0;
#endif
	if (st.st_mtime == data->timestamp
	    && frac == data->frac_ts
	    && data->root != NULL) {
	    pthread_mutex_unlock(&data->lock);
	    return 0;
	}
#else
	/*
	 * If we don't have the stat() call, assume that our in-core
	 * memory image is correct.  That is, we won't reread the
	 * profile file if it changes.
	 */
	if (data->root) {
	    pthread_mutex_unlock(&data->lock);
	    return 0;
	}
#endif
#endif /* !__APPLE__ */
	if (data->root) {
		profile_free_node(data->root);
		data->root = 0;
	}
	if (data->comment) {
		free(data->comment);
		data->comment = 0;
	}

	data->upd_serial++;
	data->flags &= PROFILE_FILE_SHARED;  /* FIXME same as '=' operator */

	errno = 0;
	f = fopen(data->filespec, "r");
	if (f == NULL) {
		retval = errno;
		if (retval == 0)
			retval = ENOENT;
		if (retval == ENOENT)
			data->flags |= PROFILE_FILE_HAVE_DATA;
		pthread_mutex_unlock(&data->lock);
		return retval;
	}
#if 0
	set_cloexec_file(f);
#endif
	retval = profile_parse_file(f, &data->root);
	fclose(f);
	if (retval) {
	    pthread_mutex_unlock(&data->lock);
	    return retval;
	}
	assert(data->root != NULL);
#ifndef __APPLE__
#ifdef HAVE_STAT
	data->timestamp = st.st_mtime;
	data->frac_ts = frac;
#endif
#endif
	data->flags |= PROFILE_FILE_HAVE_DATA;
	pthread_mutex_unlock(&data->lock);
	return 0;
}

static int
make_hard_link(const char *oldpath, const char *newpath)
{
#ifdef _WIN32
    return -1;
#else
    return link(oldpath, newpath);
#endif
}

static errcode_t write_data_to_file(prf_data_t data, const char *outfile,
				    int can_create)
{
	FILE		*f;
	profile_filespec_t new_file;
	profile_filespec_t old_file;
	errcode_t	retval = 0;

	retval = ENOMEM;
	
	new_file = old_file = 0;
	if (asprintf(&new_file, "%s.$$$", outfile) < 0) {
	    new_file = NULL;
	    goto errout;
	}
	if (asprintf(&old_file, "%s.bak", outfile) < 0) {
	    old_file = NULL;
	    goto errout;
	}

	errno = 0;

	f = fopen(new_file, "w");
	if (!f) {
		retval = errno;
		if (retval == 0)
			retval = PROF_FAIL_OPEN;
		goto errout;
	}
#if 0
	set_cloexec_file(f);
#endif
	if (data->root)
	    profile_write_tree_file(data->root, f);
	if (fclose(f) != 0) {
		retval = errno;
		goto errout;
	}

	unlink(old_file);
	if (make_hard_link(outfile, old_file) == 0) {
	    /* Okay, got the hard link.  Yay.  Now we've got our
	       backup version, so just put the new version in
	       place.  */
	    if (rename(new_file, outfile)) {
		/* Weird, the rename didn't work.  But the old version
		   should still be in place, so no special cleanup is
		   needed.  */
		retval = errno;
		goto errout;
	    }
	} else if (errno == ENOENT && can_create) {
	    if (rename(new_file, outfile)) {
		retval = errno;
		goto errout;
	    }
	} else {
	    /* Couldn't make the hard link, so there's going to be a
	       small window where data->filespec does not refer to
	       either version.  */
#ifndef _WIN32
	    sync();
#endif
	    if (rename(outfile, old_file)) {
		retval = errno;
		goto errout;
	    }
	    if (rename(new_file, outfile)) {
		retval = errno;
		rename(old_file, outfile); /* back out... */
		goto errout;
	    }
	}

	data->flags = 0;
	retval = 0;

	profile_configuration_updated();

errout:
	if (new_file)
		free(new_file);
	if (old_file)
		free(old_file);
	return retval;
}

errcode_t profile_flush_file_data_to_buffer (prf_data_t data, char **bufp)
{
	errcode_t	retval;
	retval = pthread_mutex_lock(&data->lock);
	if (retval)
		return retval;
	retval = profile_write_tree_to_buffer(data->root, bufp);
	pthread_mutex_unlock(&data->lock);
	return retval;
}

errcode_t profile_flush_file_data(prf_data_t data)
{
	errcode_t	retval = 0;

	if (!data || data->magic != PROF_MAGIC_FILE_DATA)
		return PROF_MAGIC_FILE_DATA;

	retval = pthread_mutex_lock(&data->lock);
	if (retval)
	    return retval;
	
	if ((data->flags & PROFILE_FILE_DIRTY) == 0) {
	    pthread_mutex_unlock(&data->lock);
	    return 0;
	}

	retval = write_data_to_file(data, data->filespec, 0);
	pthread_mutex_unlock(&data->lock);
	return retval;
}

errcode_t profile_flush_file_data_to_file(prf_data_t data, const char *outfile)
{
    errcode_t retval = 0;

    if (!data || data->magic != PROF_MAGIC_FILE_DATA)
	return PROF_MAGIC_FILE_DATA;

    retval = pthread_mutex_lock(&data->lock);
    if (retval)
	return retval;
    retval = write_data_to_file(data, outfile, 1);
    pthread_mutex_unlock(&data->lock);
    return retval;
}



void profile_dereference_data(prf_data_t data)
{
    int err;
    err = pthread_mutex_lock(&g_shared_trees_mutex);
    if (err)
	return;
    profile_dereference_data_locked(data);
    (void) pthread_mutex_unlock(&g_shared_trees_mutex);
}
void profile_dereference_data_locked(prf_data_t data)
{
    scan_shared_trees_locked();
    data->refcount--;
    if (data->refcount == 0)
	profile_free_file_data(data);
    scan_shared_trees_locked();
}

int profile_lock_global()
{
    return pthread_mutex_lock(&g_shared_trees_mutex);
}
int profile_unlock_global()
{
    return pthread_mutex_unlock(&g_shared_trees_mutex);
}

void profile_free_file(prf_file_t prf)
{
    profile_dereference_data(prf->data);
    free(prf);
}

/* Call with mutex locked!  */
static void profile_free_file_data(prf_data_t data)
{
    scan_shared_trees_locked();
    if (data->flags & PROFILE_FILE_SHARED) {
	/* Remove from linked list.  */
	if (g_shared_trees == data)
	    g_shared_trees = data->next;
	else {
	    prf_data_t prev, next;
	    prev = g_shared_trees;
	    next = prev->next;
	    while (next) {
		if (next == data) {
		    prev->next = next->next;
		    break;
		}
		prev = next;
		next = next->next;
	    }
	}
    }
    if (data->root)
	profile_free_node(data->root);
    if (data->comment)
	free(data->comment);
    data->magic = 0;
    pthread_mutex_destroy(&data->lock);
    free(data);
    scan_shared_trees_locked();
}

errcode_t profile_close_file(prf_file_t prf)
{
	errcode_t	retval;

	if (prf == NULL)
		return 0;
	
	retval = profile_flush_file(prf);
	profile_free_file(prf);
	if (retval)
		return retval;
	return 0;
}
