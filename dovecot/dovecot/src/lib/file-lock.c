/* Copyright (c) 2002-2011 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "file-lock.h"

#ifdef HAVE_FLOCK
#  include <sys/file.h>
#endif

/* APPLE */
static bool multiclient_may_lock(int fd, const char *path, int lock_type,
				 uintmax_t client_id, bool *apply_to_file);
static void multiclient_did_lock(int fd, const char *path, int lock_type,
				 uintmax_t client_id);

struct file_lock {
	int fd;
	char *path;

	int lock_type;
	enum file_lock_method lock_method;
};

bool file_lock_method_parse(const char *name, enum file_lock_method *method_r)
{
	if (strcasecmp(name, "fcntl") == 0)
		*method_r = FILE_LOCK_METHOD_FCNTL;
	else if (strcasecmp(name, "flock") == 0)
		*method_r = FILE_LOCK_METHOD_FLOCK;
	else if (strcasecmp(name, "dotlock") == 0)
		*method_r = FILE_LOCK_METHOD_DOTLOCK;
	else
		return FALSE;
	return TRUE;
}

int file_try_lock(int fd, const char *path, int lock_type,
		  enum file_lock_method lock_method,
		  struct file_lock **lock_r)
{
	return file_wait_lock(fd, path, lock_type, lock_method, 0, lock_r);
}

static int file_lock_do(int fd, const char *path, int lock_type,
			enum file_lock_method lock_method,
			unsigned int timeout_secs,
			uintmax_t client_id)			/* APPLE */
{
	int ret;
	bool apply_to_file = TRUE;				/* APPLE */

	i_assert(fd != -1);

	/* APPLE */
	if (!multiclient_may_lock(fd, path, lock_type, client_id,
				  &apply_to_file)) {
		errno = EAGAIN;
		return 0;
	} else if (!apply_to_file)
		return 1;

	if (timeout_secs != 0)
		alarm(timeout_secs);

	switch (lock_method) {
	case FILE_LOCK_METHOD_FCNTL: {
#ifndef HAVE_FCNTL
		i_fatal("fcntl() locks not supported");
#else
		struct flock fl;
		const char *errstr;

		fl.l_type = lock_type;
		fl.l_whence = SEEK_SET;
		fl.l_start = 0;
		fl.l_len = 0;

		ret = fcntl(fd, timeout_secs ? F_SETLKW : F_SETLK, &fl);
		if (timeout_secs != 0) alarm(0);

		if (ret == 0)
			break;

		if (timeout_secs == 0 &&
		    (errno == EACCES || errno == EAGAIN)) {
			/* locked by another process */
			return 0;
		}

		if (errno == EINTR) {
			/* most likely alarm hit, meaning we timeouted.
			   even if not, we probably want to be killed
			   so stop blocking. */
			errno = EAGAIN;
			return 0;
		}
		errstr = errno != EACCES ? strerror(errno) :
			"File is locked by another process (EACCES)";
		i_error("fcntl(%s) locking failed for file %s: %s",
			lock_type == F_UNLCK ? "unlock" :
			lock_type == F_RDLCK ? "read-lock" : "write-lock",
			path, errstr);
		return -1;
#endif
	}
	case FILE_LOCK_METHOD_FLOCK: {
#ifndef HAVE_FLOCK
		i_fatal("flock() locks not supported");
#else
		int operation = timeout_secs != 0 ? 0 : LOCK_NB;

		switch (lock_type) {
		case F_RDLCK:
			operation |= LOCK_SH;
			break;
		case F_WRLCK:
			operation |= LOCK_EX;
			break;
		case F_UNLCK:
			operation |= LOCK_UN;
			break;
		}

		ret = flock(fd, operation);
		if (timeout_secs != 0) alarm(0);

		if (ret == 0)
			break;

		if (errno == EWOULDBLOCK || errno == EINTR) {
			/* a) locked by another process,
			   b) timeouted */
			return 0;
		}
		i_error("flock(%s) locking failed for file %s: %m",
			lock_type == F_UNLCK ? "unlock" :
			lock_type == F_RDLCK ? "read-lock" : "write-lock",
			path);
		return -1;
#endif
	}
	case FILE_LOCK_METHOD_DOTLOCK:
		/* we shouldn't get here */
		i_unreached();
	}

	/* APPLE */
	multiclient_did_lock(fd, path, lock_type, client_id);

	return 1;
}

int file_wait_lock(int fd, const char *path, int lock_type,
		   enum file_lock_method lock_method,
		   unsigned int timeout_secs,
		   struct file_lock **lock_r)
{
	/* APPLE */
	return file_wait_lock_multiclient(fd, path, lock_type, lock_method,
					  timeout_secs, lock_r, 0);
}

/* APPLE */
int file_wait_lock_multiclient(int fd, const char *path, int lock_type,
			       enum file_lock_method lock_method,
			       unsigned int timeout_secs,
			       struct file_lock **lock_r, uintmax_t client_id)
{
	struct file_lock *lock;
	int ret;

	ret = file_lock_do(fd, path, lock_type, lock_method, timeout_secs,
			   client_id);				/* APPLE */
	if (ret <= 0)
		return ret;

	lock = i_new(struct file_lock, 1);
	lock->fd = fd;
	lock->path = i_strdup(path);
	lock->lock_type = lock_type;
	lock->lock_method = lock_method;
	*lock_r = lock;
	return 1;
}

int file_lock_try_update(struct file_lock *lock, int lock_type)
{
	return file_lock_do(lock->fd, lock->path, lock_type,
			    lock->lock_method, 0,
			    0);				/* APPLE */
}

void file_unlock(struct file_lock **_lock)
{
	/* APPLE */
	file_unlock_multiclient(_lock, 0);
}

/* APPLE */
void file_unlock_multiclient(struct file_lock **_lock, uintmax_t client_id)
{
	struct file_lock *lock = *_lock;

	*_lock = NULL;

	if (file_lock_do(lock->fd, lock->path, F_UNLCK,
			 lock->lock_method, 0, client_id) == 0) {  /* APPLE */
		/* this shouldn't happen */
		i_error("file_unlock(%s) failed: %m", lock->path);
	}

	file_lock_free(&lock);
}

void file_lock_free(struct file_lock **_lock)
{
	struct file_lock *lock = *_lock;

	*_lock = NULL;

	i_free(lock->path);
	i_free(lock);
}


/* APPLE - rest of file - multiclient locks
   Mediate attempts to lock a file multiple times from a single process on
   behalf of different connected clients.  Without this, two different
   connections could acquire conflicting locks on a single file and cause data
   corruption.  With this, attempts by one pid but different client IDs
   to acquire a lock should behave just like attempts by different pids. */

#include "array.h"
#include "hash.h"

#include <sys/stat.h>

#define	MULTICLIENT_DEBUG	0
#if MULTICLIENT_DEBUG
#include "str.h"
#include "backtrace-string.h"
#include <sys/types.h>
#endif

struct multiclient_key {
	dev_t dev;
	ino_t ino;
};

struct multiclient_lock {
	int fd;
	char *path;
	ARRAY_DEFINE(read_locks, uintmax_t);
	uintmax_t write_lock;
#if MULTICLIENT_DEBUG
	ARRAY_DEFINE(read_backtraces, string_t *);
	string_t *write_backtrace;
#endif
};

/* multiclient_key => multiclient_lock */
static struct hash_table *multiclient_locks = NULL;

#define	READ_LOCK_COUNT(lock)	(array_is_created(&(lock)->read_locks) ? \
				 array_count(&(lock)->read_locks) : 0)

static void multiclient_key_init(struct multiclient_key *key, int fd,
				 const char *path)
{
	struct stat stbuf;
	if (fstat(fd, &stbuf) < 0)
		i_panic("multiclient_key_init(%s): fstat(%d) failed: %m",
			path, fd);
	key->dev = stbuf.st_dev;
	key->ino = stbuf.st_ino;
}

static struct multiclient_key *
multiclient_key_clone(struct multiclient_key *key)
{
	struct multiclient_key *newkey;

	newkey = i_new(struct multiclient_key, 1);
	*newkey = *key;

	return newkey;
}

static void multiclient_key_destroy(struct multiclient_key **_key)
{
	struct multiclient_key *key = *_key;

	*_key = NULL;
	i_free(key);
}
	
static unsigned int multiclient_key_hash(const void *p)
{
	const struct multiclient_key *key = p;

	/* no idea if this "algorithm" is any good, but the hash table
	   is usually empty anyway so it doesn't really matter */
	return key->dev | key->ino;
}

static int multiclient_key_cmp(const void *p1, const void *p2)
{
	const struct multiclient_key *key1 = p1;
	const struct multiclient_key *key2 = p2;

	if (key1->dev < key2->dev)
		return -1;
	else if (key1->dev > key2->dev)
		return 1;
	else if (key1->ino < key2->ino)
		return -1;
	else if (key1->ino > key2->ino)
		return 1;
	else
		return 0;
}

static const char *multiclient_key_name(int fd ATTR_UNUSED, const char *path,
					const struct multiclient_key *key ATTR_UNUSED)
{
#if MULTICLIENT_DEBUG
	return t_strdup_printf("fd %d, path %s, dev %ld,%ld, ino %llu",
			       fd, path,
			       (unsigned long) major(key->dev),
			       (unsigned long) minor(key->dev),
			       (unsigned long long) key->ino);
#else
	return path;
#endif
}

static struct multiclient_lock *multiclient_lock_create(int fd,
							const char *path)
{
	struct multiclient_lock *lock;

	lock = i_new(struct multiclient_lock, 1);
	lock->fd = fd;
	lock->path = i_strdup(path);

#if MULTICLIENT_DEBUG
	lock->write_backtrace = str_new(default_pool, 1024);
#endif

	return lock;
}

static void multiclient_lock_destroy(struct multiclient_lock **_lock)
{
	struct multiclient_lock *lock = *_lock;

	*_lock = NULL;
	i_free(lock->path);
	if (array_is_created(&lock->read_locks))
		array_free(&lock->read_locks);
#if MULTICLIENT_DEBUG
	if (array_is_created(&lock->read_backtraces)) {
		string_t **strs;
		unsigned int count, i;
		strs = array_get_modifiable(&lock->read_backtraces, &count);
		for (i = 0; i < count; i++)
			str_free(&strs[i]);
		array_free(&lock->read_backtraces);
	}
	str_free(&lock->write_backtrace);
#endif
	i_free(lock);
}

static bool multiclient_lock(int fd, const char *path, int lock_type,
			     uintmax_t client_id, bool *apply_to_file,
			     bool record)
{
	struct multiclient_key skey;
	struct multiclient_key *key = NULL;
	struct multiclient_lock *lock = NULL;
	bool found;

	if (client_id == 0) {
		/* although, it is a no-no to mix client_id == 0 and
		   client_id != 0 locks on the same file */
		*apply_to_file = TRUE;
		return TRUE;
	}

	if (multiclient_locks == NULL)
		multiclient_locks =
			hash_table_create(default_pool, default_pool,
					  8, multiclient_key_hash,
					  multiclient_key_cmp);

	multiclient_key_init(&skey, fd, path);
	found = hash_table_lookup_full(multiclient_locks, &skey,
				       (void **) &key, (void **) &lock);

	/* these panic messages are intended to be more helpful than assertion
	   failed messages */
	if (found && lock->write_lock == 0 && READ_LOCK_COUNT(lock) == 0)
		i_panic("multiclient_lock(%s): table contains unlocked lock",
			multiclient_key_name(fd, path, &skey));
	if (lock_type == F_UNLCK) {
		const uintmax_t *readers = NULL;
		unsigned int readers_count = 0;

		if (!found)
			i_panic("multiclient_lock(%s): not locked",
				multiclient_key_name(fd, path, &skey));

		if (array_is_created(&lock->read_locks))
			readers = array_get(&lock->read_locks, &readers_count);
		if (lock->write_lock == client_id) {
			if (readers_count > 0) {
#if MULTICLIENT_DEBUG
				i_panic("multiclient_lock(%s): unlocking "
					"write lock with %u reader(s); "
					"first from %s",
					multiclient_key_name(fd, path, key),
					readers_count,
					str_c(*array_idx(&lock->read_backtraces,
							0)));
#else
				i_panic("multiclient_lock(%s): unlocking "
					"write lock with %u reader(s)",
					multiclient_key_name(fd, path, key),
					readers_count);
#endif
			}

			if (record) {
				lock->write_lock = 0;
#if MULTICLIENT_DEBUG
				str_truncate(lock->write_backtrace, 0);
#endif
				hash_table_remove(multiclient_locks, key);
				multiclient_key_destroy(&key);
				multiclient_lock_destroy(&lock);
			}

			*apply_to_file = TRUE;
		} else if (readers_count == 1) {
			if (lock->write_lock != 0) {
#if MULTICLIENT_DEBUG
				i_panic("multiclient_lock(%s): unlocking "
					"read lock with a writer from %s",
					multiclient_key_name(fd, path, key),
					str_c(lock->write_backtrace));
#else
				i_panic("multiclient_lock(%s): unlocking "
					"read lock with a writer",
					multiclient_key_name(fd, path, key));
#endif
			}
			if (readers[0] != client_id)
				i_panic("multiclient_lock(%s): read locked "
					"but not by me",
					multiclient_key_name(fd, path, key));

			if (record) {
				hash_table_remove(multiclient_locks, key);
				multiclient_key_destroy(&key);
				multiclient_lock_destroy(&lock);
			}

			*apply_to_file = TRUE;
		} else {
			unsigned int i;

			if (lock->write_lock != 0)
				i_panic("multiclient_lock(%s): write locked "
					"but not by me",
					multiclient_key_name(fd, path, key));

			for (i = 0; i < readers_count; i++)
				if (readers[i] == client_id)
					break;
			if (i >= readers_count)
				i_panic("multiclient_lock(%s): can't find "
					"my read lock",
					multiclient_key_name(fd, path, key));
			if (record)
				array_delete(&lock->read_locks, i, 1);
#if MULTICLIENT_DEBUG
			if (record) {
				string_t *str =
					*array_idx(&lock->read_backtraces, i);
				str_free(&str);
				array_delete(&lock->read_backtraces, i, 1);
			}
#endif

			*apply_to_file = FALSE;
		}
	} else if (lock_type == F_WRLCK) {
		if (found) {
			const uintmax_t *readers;
			unsigned int readers_count = 0;

			/* upgrade read -> write lock */

			if (lock->write_lock == client_id) {
#if MULTICLIENT_DEBUG
				i_panic("multiclient_lock(%s): write lock "
					"over my own write lock from %s",
					multiclient_key_name(fd, path, key),
					str_c(lock->write_backtrace));
#else
				i_panic("multiclient_lock(%s): write lock "
					"over my own write lock",
					multiclient_key_name(fd, path, key));
#endif
			}
			if (lock->write_lock != 0) {
#if MULTICLIENT_DEBUG
				i_debug("multiclient_lock(%s): prevented "
					"double write-lock",
					multiclient_key_name(fd, path, key));
#endif
				return FALSE;
			}

			readers = array_get(&lock->read_locks, &readers_count);
			if (readers_count > 1 ||
			    readers[0] != client_id) {
#if MULTICLIENT_DEBUG
				i_debug("multiclient_lock(%s): prevented "
					"write lock over read lock",
					multiclient_key_name(fd, path, key));
#endif
				return FALSE;
			}

			if (record) {
				array_delete(&lock->read_locks, 0, 1);
				lock->write_lock = client_id;
			}
#if MULTICLIENT_DEBUG
			if (record) {
				string_t *str =
					*array_idx(&lock->read_backtraces, 0);
				str_free(&str);
				array_delete(&lock->read_backtraces, 0, 1);
				backtrace_append(lock->write_backtrace);
			}
#endif
		} else {
			if (record) {
				key = multiclient_key_clone(&skey);
				lock = multiclient_lock_create(fd, path);
				lock->write_lock = client_id;
#if MULTICLIENT_DEBUG
				backtrace_append(lock->write_backtrace);
#endif
				hash_table_insert(multiclient_locks,
						  key, lock);
			}
		}

		*apply_to_file = TRUE;
	} else if (lock_type == F_RDLCK) {
		if (found) {
			const uintmax_t *readers;
			unsigned int readers_count = 0, i;

			if (lock->write_lock != 0) {
#if MULTICLIENT_DEBUG
				i_debug("multiclient_lock(%s): prevented "
					"read lock over write lock",
					multiclient_key_name(fd, path, key));
#endif
				return FALSE;
			}

			readers = array_get(&lock->read_locks, &readers_count);
			for (i = 0; i < readers_count; i++) {
				if (readers[i] == client_id) {
					/* already have a read lock */
#if MULTICLIENT_DEBUG
					i_panic("multiclient_lock(%s): read "
						"lock over my own read lock "
						"from %s",
						multiclient_key_name(fd, path, key),
						str_c(*array_idx(&lock->read_backtraces, i)));
#else
					i_panic("multiclient_lock(%s): read "
						"lock over my own read lock",
						multiclient_key_name(fd, path, key));
#endif
				}
			}
			if (record)
				array_append(&lock->read_locks,
					     &client_id, 1);
#if MULTICLIENT_DEBUG
			if (record) {
				string_t *str = str_new(default_pool, 1024);
				backtrace_append(str);
				array_append(&lock->read_backtraces, &str, 1);
			}
#endif

			*apply_to_file = FALSE;
		} else {
			if (record) {
				key = multiclient_key_clone(&skey);
				lock = multiclient_lock_create(fd, path);
				i_array_init(&lock->read_locks, 2);
				array_append(&lock->read_locks,
					     &client_id, 1);
				hash_table_insert(multiclient_locks,
						  key, lock);
			}
#if MULTICLIENT_DEBUG
			if (record) {
				string_t *str = str_new(default_pool, 1024);
				backtrace_append(str);
				i_array_init(&lock->read_backtraces, 2);
				array_append(&lock->read_backtraces, &str, 1);
			}
#endif

			*apply_to_file = TRUE;
		}
	} else
		i_unreached();

	return TRUE;
}

static bool multiclient_may_lock(int fd, const char *path, int lock_type,
				 uintmax_t client_id, bool *apply_to_file)
{
	return multiclient_lock(fd, path, lock_type, client_id,
				apply_to_file, FALSE);
}

static void multiclient_did_lock(int fd, const char *path, int lock_type,
				 uintmax_t client_id)
{
	bool apply_to_file;
	bool permit;

	permit = multiclient_lock(fd, path, lock_type, client_id,
				  &apply_to_file, TRUE);
	i_assert(permit && apply_to_file);
}
