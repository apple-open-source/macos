/*
 * tunable.h -- values that a site might want to change
 *
 * For license terms, see the file COPYING in this directory.
 */

/* umask to use when daemon creates files */
#define	DEF_UMASK		022

/* default timeout period in seconds if server connection dies */
#define	CLIENT_TIMEOUT		300

/* maximum consecutive timeouts to accept */
#define MAX_TIMEOUTS		20

/* maximum consecutive lock-busy errors to accept */
#define MAX_LOCKOUTS		5

/* maximum consecutive authentication failures to accept */
#define MAX_AUTHFAILS		10

/* default skipped message warning interval in seconds */
#define WARNING_INTERVAL	3600
