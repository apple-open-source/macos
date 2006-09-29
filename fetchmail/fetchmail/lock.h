#ifndef FM_LOCK_H
#define FM_LOCK_H

/* lock.c: concurrency locking */
void lock_setup(struct runctl *);
void lock_assert(void);
void lock_or_die(void);
void fm_lock_release(void);
int lock_state(void);
void lock_dispose(void);

#endif
