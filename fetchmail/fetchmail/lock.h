/** \file lock.h function declarations (API) for lock.c */

#ifndef FM_LOCK_H
#define FM_LOCK_H

/* from lock.c: concurrency locking */

/** Set up the global \a lockfile variable (but do not lock yet). */
void fm_lock_setup(struct runctl *);

/** Assert that we already possess a lock. */
void fm_lock_assert(void);

/** Obtain a lock or exit the program with PS_EXCLUDE. This function is
 * idempotent, that means it can be run even if we already have the
 * lock. Note that fm_lock_setup() has to be run beforehand; this
 * function does not check that condition. */
void fm_lock_or_die(void);

/** Release the lock (unlink the lockfile). */
void fm_lock_release(void);

/** Check the state of the lock file. If there is an error opening or
 * reading the lockfile, exit with PS_EXCLUDE. If a stale lock file
 * cannot be unlinked, complain and try to truncate it to 0 size. If
 * truncation fails, complain and exit with PS_EXCLUDE.  \return
 * -  0 if no lock is set
 * - >0 if a fetchmail is running, but not in daemon mode
 * - <0 if a fetchmail is running in daemon mode.
 */
int  fm_lock_state(void);

/** If atexit(3) is available on the system this software is compiled on,
 * register an exit handler to dipose of the lock on process exit. */
void fm_lock_dispose(void);

#endif
