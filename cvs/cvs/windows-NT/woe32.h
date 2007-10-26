/*
 * woe32.h
 * - utility functions for cvs under win32
 *
 */

#ifndef WOE32_H
#define WOE32_H

#include <timespec.h>

/* #define SYSTEM_CLEANUP woe32_cleanup */
void woe32_cleanup (void);

/* #define fd_select woe32_fd_select */
int woe32_fd_select (	int nfds,
						struct fd_set * readfds,
						struct fd_set * writefds,
						struct fd_set * errorfds,
						struct timeval * timeout);

char *woe32_getlogin (void);

char *woe32_home_dir (void);

/* #define SYSTEM_INITIALIZE(pargc,pargv) woe32_init_winsock() */
void woe32_init_winsock (void);

/* #define nanosleep woe32_nanosleep */
int woe32_nanosleep (const struct timespec *requested_delay,
                           struct timespec *remaining_delay);

char * woe32_shell (void);

#endif /* WOE32_H */
