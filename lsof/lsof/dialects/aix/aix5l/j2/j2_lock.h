/*
 * Temporary <j2/j2_lock.h> for AIX 5 L lsof until it appears in an AIX 5L
 * release.
 *
 * V. Abell <abe@purdue.edu>
 * Purdue University Computing Center
 */

#if	!defined(LSOF_J2_LOCK_H)
#define	LSOF_J2_LOCK_H
typedef	long		event_t;
#define	MUTEXLOCK_T	Simple_lock
#define	RDWRLOCK_T	Complex_lock
#endif	/* !defined(LSOF_J2_LOCK_H) */
