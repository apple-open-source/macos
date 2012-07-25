/*
 * ntp_md5.h: deal with md5.h headers
 */

#if defined HAVE_MD5_H && defined HAVE_MD5INIT
# include <md5.h>
#elif defined(__APPLE__)
#include <CommonCrypto/CommonDigest.h>
# define MD5_CTX	CC_MD5_CTX
# define MD5Init	CC_MD5_Init
# define MD5Update	CC_MD5_Update
# define MD5Final	CC_MD5_Final
#else
# include "isc/md5.h"
# define MD5_CTX	isc_md5_t
# define MD5Init	isc_md5_init
# define MD5Update	isc_md5_update
# define MD5Final(d,c)	isc_md5_final(c,d)
#endif
