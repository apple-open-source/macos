/* $Xorg: thr_stubs.c,v 1.3 2000/08/17 19:55:21 cpqbld Exp $ */
/*
 * Stub interface to thread routines that Solaris needs but shipped
 * broken/buggy versions in 5.2 and 5.3
 *
 * One workaround is to include this stub routine when you link.
 *
 * These routines don't need to have accurate interfaces -- they will 
 * never be called. They just need to be there in order to be resolved 
 * at link time by non-threaded programs.
 */

extern int errno;

typedef int thread_t;

thread_t thr_self(void) { errno = -1; return 0; }
int thr_create(void) { errno = -1; return -1; }
int mutex_init(void) { errno = -1; return -1; }
int mutex_destroy(void) { errno = -1; return -1; }
int mutex_lock(void) { errno = -1; return -1; }
int mutex_unlock(void) { errno = -1; return -1; }
int cond_init(void) { errno = -1; return -1; }
int cond_destroy(void) { errno = -1; return -1; }
int cond_wait(void) { errno = -1; return -1; }
int cond_signal(void) { errno = -1; return -1; }
int cond_broadcast(void) { errno = -1; return -1; }
