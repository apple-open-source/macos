#ifndef _XINETD_LIBPORTABLE
#define _XINETD_LIBPORTABLE 1

#include "config.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#ifndef IPV6_ADDRFORM
#define IPV6_ADDRFORM            1
#endif

#ifndef NI_MAXHOST
#define NI_MAXHOST 1025
#endif

#ifndef HAVE_DIFFTIME
double difftime( time_t, time_t );
#endif

#ifndef HAVE_STRCASECMP
int strcasecmp(const char *s1, const char *s2);
#endif

#ifndef HAVE_STRERROR
char * strerror(int);
#endif

#ifdef NO_INET_ATON
int inet_aton(const char *cp, struct in_addr *addr);
#endif

#ifndef HAVE_GETNAMEINFO
int getnameinfo(const struct sockaddr *sa, size_t salen, char *host,
                size_t hostlen, char *serv, size_t servlen, int flags);
#endif

#ifndef HAVE_INET_NTOP
const char *inet_ntop(int, const void *, char *, size_t);
#endif

#ifndef APPEND
#define APPEND(a, b) APPEND2 (a, b)
#endif

#ifndef APPEND2
#define APPEND2(a, b) a##b
#endif

#ifndef FLOAT_TYPE
#define FLOAT_TYPE double
#endif

#ifndef FUNC_PREFIX
#define FUNC_PREFIX 
#endif

#ifndef HAVE_FCVT
int APPEND(FUNC_PREFIX,fcvt_r)(FLOAT_TYPE value,int ndigit,int *decpt,int *sign,char *buf,size_t len);
char *APPEND(FUNC_PREFIX,fcvt)(FLOAT_TYPE value,int ndigit,int *decpt,int *sign);
#endif
#ifndef HAVE_ECVT
char *APPEND(FUNC_PREFIX,ecvt)(FLOAT_TYPE value,int ndigit,int *decpt,int *sign);
int APPEND(FUNC_PREFIX,ecvt_r)(FLOAT_TYPE value,int ndigit,int *decpt,int *sign,char *buf,size_t len);
#endif
#ifndef HAVE_GCVT
char *APPEND(FUNC_PREFIX,gcvt)(FLOAT_TYPE value,int ndigit,char *buf);
#endif

/* from OpenSSH's fake-socket.h */

#ifndef HAVE_STRUCT_SOCKADDR_STORAGE
# define	_SS_MAXSIZE	128	/* Implementation specific max size */
# define       _SS_PADSIZE     (_SS_MAXSIZE - sizeof (struct sockaddr))

struct sockaddr_storage {
	struct	sockaddr ss_sa;
	char		__ss_pad2[_SS_PADSIZE];
};
# define ss_family ss_sa.sa_family
#endif /* !HAVE_STRUCT_SOCKADDR_STORAGE */

#ifndef IN6_IS_ADDR_LOOPBACK
# define IN6_IS_ADDR_LOOPBACK(a) \
	(((u_int32_t *) (a))[0] == 0 && ((u_int32_t *) (a))[1] == 0 && \
	 ((u_int32_t *) (a))[2] == 0 && ((u_int32_t *) (a))[3] == htonl (1))
#endif /* !IN6_IS_ADDR_LOOPBACK */

#ifndef HAVE_STRUCT_IN6_ADDR
struct in6_addr {
	u_int8_t		s6_addr[16];
};
#endif /* !HAVE_STRUCT_IN6_ADDR */

#ifndef HAVE_STRUCT_SOCKADDR_IN6
struct sockaddr_in6 {
   unsigned short sin6_family;
	u_int16_t sin6_port;
	u_int32_t sin6_flowinfo;
	struct in6_addr sin6_addr;
};
#endif /* !HAVE_STRUCT_SOCKADDR_IN6 */

#ifndef AF_INET6
/* Define it to something that should never appear */
#define AF_INET6 AF_MAX
#endif


#endif /* _XINETD_LIBPORTABLE */
