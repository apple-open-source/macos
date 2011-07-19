/* APPLE - Mail SACL cache client
   Based on verify_clnt */

#ifndef _SACL_CACHE_CLNT_H_INCLUDED_
#define _SACL_CACHE_CLNT_H_INCLUDED_

 /*
  * System library.
  */
#include <stdarg.h>

 /*
  * Global library.
  */
#include <deliver_request.h>

 /*
  * Address verification requests.
  */
#define SACL_CACHE_REQ_PUT		"put"
#define SACL_CACHE_REQ_GET		"get"
#define SACL_CACHE_REQ_NO_SACL		"no-sacl"

 /*
  * Request (NOT: address) status codes.
  */
#define SACL_CACHE_STAT_OK		0
#define SACL_CACHE_STAT_FAIL		(-1)
#define SACL_CACHE_STAT_BAD		(-2)

 /*
  * SACL check status codes.
  */
#define SACL_CHECK_STATUS_UNKNOWN	0
#define SACL_CHECK_STATUS_AUTHORIZED	1
#define SACL_CHECK_STATUS_UNAUTHORIZED	2
#define SACL_CHECK_STATUS_NO_SACL	3

 /*
  * Functional interface.
  */
extern int sacl_cache_clnt_put(const char *, int);
extern int sacl_cache_clnt_get(const char *, int *);
extern int sacl_cache_clnt_no_sacl(void);

#endif
