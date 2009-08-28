#include <sys/types.h>
#if defined(_MSC_VER) || defined(__MINGW32_VERSION)
typedef unsigned char u_char;
#endif
#define TESTVAL 4
extern double foo(int, long, const char*);

/*
 * See netdns.c for copyright notice.
 */



/*
 * Defines for handling compressed domain names
 */
#define INDIR_MASK 0xc0

/* Note: MAXDNAME is the size of a DNAME in PRESENTATION FORMAT.
 *  Each character in the label may need 4 characters in presentation format
 * think \002.\003\004.example.com
 * Hmmm 1010 is just a bit oversized 
 */

#define MAXDNAME 1010

int netdns_dn_expand( u_char *msg,  u_char *eomorig,
	       u_char *comp_dn,  u_char *exp_dn,
	       int length);


