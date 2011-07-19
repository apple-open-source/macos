#include <TargetConditionals.h>

#define __IPSEC_BUILD__ 1

/* If printf doesn't support %zu. */
#undef BROKEN_PRINTF

/* Enable admin port */
#define ENABLE_ADMINPORT 1

/* Enable VPN control port */
#define ENABLE_VPNCONTROL_PORT 1

/* Enable dead peer detection */
#define ENABLE_DPD 1

/* IKE fragmentation support */
#define ENABLE_FRAG 1

/* Hybrid authentication support */
#define ENABLE_HYBRID 1

/* Enable NAT-Traversal */
#define ENABLE_NATT 1

/* Enable NAT-Traversal draft 02 */
#define ENABLE_NATT_02 1

/* Enable NAT-Traversal draft 03 */
#define ENABLE_NATT_03 1

/* Enable NAT-Traversal draft 04 */
#define ENABLE_NATT_04 1

/* Enable NAT-Traversal draft 05 */
#define ENABLE_NATT_05 1

/* Enable NAT-Traversal draft 06 */
#define ENABLE_NATT_06 1

/* Enable NAT-Traversal draft 07 */
#define ENABLE_NATT_07 1

/* Enable NAT-Traversal draft 08 */
#define ENABLE_NATT_08 1

/* Enable NAT-Traversal APPLE version */
#define ENABLE_NATT_APPLE 1

/* Enable NAT-Traversal RFC version */
#define ENABLE_NATT_RFC 1

/* Enable samode-unspec */
#undef ENABLE_SAMODE_UNSPECIFIED

/* Enable statictics */
/* #define ENABLE_STATS 1*/	  /* causes too many logs to syslog */

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you don't have `vprintf' but do have `_doprnt.' */
#undef HAVE_DOPRNT

/* Have __func__ macro */
#define HAVE_FUNC_MACRO 1

/* Define to 1 if you have the `gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Enable GSS API */
/* %%%%%%% change this back when conflict fixed */
#undef HAVE_GSSAPI

/* Have iconv using const */
#define HAVE_ICONV_2ND_CONST 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Have ipsec_policy_t */
#undef HAVE_IPSEC_POLICY_T

/* Hybrid authentication uses PAM */
//#define HAVE_LIBPAM 1
#undef HAVE_LIBPAM

/* Hybrid authentication uses RADIUS */
#undef HAVE_LIBRADIUS

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if keychain is used */
#if TARGET_OS_EMBEDDED
#undef HAVE_KEYCHAIN
#else
#define HAVE_KEYCHAIN 1
#endif

/* Define to 1 if keychain is used */
#if TARGET_OS_EMBEDDED
#undef HAVE_SECURITY_FRAMEWORK
#else
#define HAVE_SECURITY_FRAMEWORK 1
#endif


/* Define to 1 if Open Dir available */
#if TARGET_OS_EMBEDDED
#undef HAVE_OPENDIR
#else
#define HAVE_OPENDIR 1
#endif

#if TARGET_OS_EMBEDDED
#undef HAVE_LIBLDAP
#else
#define HAVE_LIBLDAP 1
#endif

#define HAVE_NETINET6_IPSEC 1

#define HAVE_GETIFADDRS 1

#if TARGET_OS_EMBEDDED
#undef HAVE_OPENSSL
#else
#define HAVE_OPENSSL 1
#endif

#ifdef HAVE_OPENSSL
/* Define to 1 if you have the <openssl/aes.h> header file. */
#define HAVE_OPENSSL_AES_H 1

/* Define to 1 if you have the <openssl/engine.h> header file. */
#define HAVE_OPENSSL_ENGINE_H 1

/* Define to 1 if you have the <openssl/idea.h> header file. */
#undef HAVE_OPENSSL_IDEA_H

/* Define to 1 if you have the <openssl/rc5.h> header file. */
#define HAVE_OPENSSL_RC5_H 1
#endif

/* Define to 1 if you have the `pam_start' function. */
#if TARGET_OS_EMBEDDED
#undef HAVE_PAM_START
#else
#define HAVE_PAM_START 1
#endif

/* Are PF_KEY policy priorities supported? */
#undef HAVE_PFKEY_POLICY_PRIORITY

/* Have forward policy */
#undef HAVE_POLICY_FWD

/* Define to 1 if you have the `rad_create_request' function. */
#undef HAVE_RAD_CREATE_REQUEST

/* Is readline available? */
#undef HAVE_READLINE

/* Define to 1 if you have the `select' function. */
#define HAVE_SELECT 1

/* sha2 is defined in sha.h */
#define HAVE_SHA2_IN_SHA_H 1

/* Define to 1 if you have the <shadow.h> header file. */
#undef HAVE_SHADOW_H

/* Define to 1 if you have the `socket' function. */
#define HAVE_SOCKET 1

/* Define to 1 if you have the <stdarg.h> header file. */
#define HAVE_STDARG_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the `strerror' function. */
#define HAVE_STRERROR 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strlcat' function. */
#define HAVE_STRLCAT 1

/* Define to 1 if you have the `strlcpy' function. */
#define HAVE_STRLCPY 1

/* Define to 1 if you have the `strtol' function. */
#define HAVE_STRTOL 1

/* Define to 1 if you have the `strtoul' function. */
#define HAVE_STRTOUL 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have <sys/wait.h> that is POSIX.1 compatible. */
#define HAVE_SYS_WAIT_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the <varargs.h> header file. */
#define HAVE_VARARGS_H 1

/* Define to 1 if you have the `vprintf' function. */
#define HAVE_VPRINTF 1

/* Support IPv6 */
#define INET6 1

/* Use advanced IPv6 API */
#define INET6_ADVAPI 1
#define __APPLE_USE_RFC_3542 1

/* Name of package */
#undef PACKAGE

/* Define to the address where bug reports for this package should be sent. */
#undef PACKAGE_BUGREPORT

/* Define to the full name of this package. */
#undef PACKAGE_NAME

/* Define to the full name and version of this package. */
#undef PACKAGE_STRING

/* Define to the one symbol short name of this package. */
#undef PACKAGE_TARNAME

/* Define to the version of this package. */
#undef PACKAGE_VERSION

/* Define as the return type of signal handlers (`int' or `void'). */
#define RETSIGTYPE int

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Define to 1 if your <sys/time.h> declares `struct tm'. */
#define TM_IN_SYS_TIME 1

/* A 'va_copy' style function */
#undef VA_COPY

/* Version number of package */
#undef VERSION

/* SHA2 support */
#define WITH_SHA2 1

/* Define to 1 if `lex' declares `yytext' as a `char *' by default, not a
   `char[]'. */
#define YYTEXT_POINTER 1

/* Define to empty if `const' does not conform to ANSI C. */
#undef const

/* Define to `int' if <sys/types.h> does not define. */
#undef pid_t

/* Define to `unsigned' if <sys/types.h> does not define. */
#undef size_t

#define USE_SYSTEMCONFIGURATION_PRIVATE_HEADERS 1
