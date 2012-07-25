/* include/config.h.  Generated from config.h.in by configure.  */
/* include/config.h.in.  Generated from configure.in by autoheader.  */

/* Name of C compiler's unused attribute */
#define ATTRIBUTE_UNUSED __attribute__((unused))

/* Include the DCE dummy auth service */
/* #undef AUTH_DUMMY */

/* Include the DCE gss_negotiate auth service */
#define AUTH_GSS_NEGOTIATE 1

/* Include the schannel auth service */
/* #undef AUTH_SCHANNEL */

/* Support for the codeset registry (untested) */
/* #undef BUILD_CODESET */

/* Support for the LDAP nameservice (untested) */
/* #undef BUILD_RPC_NS_LDAP */

/* Compile with debugging support */
#define DEBUG 1

/* Build demo programs */
#define DEMO_PROGS 1

/* Enable DUMPERS for debugging dceidl */
#define DUMPERS 1

/* Enable GSS negotiate auth mechanism */
#define ENABLE_AUTH_GSS_NEGOTIATE

/* Enable schannel auth mechanism */
/* #undef ENABLE_AUTH_SCHANNEL */

/* Enable experimental DCOM support */
/* #undef ENABLE_DCOM */

/* Enable IP address family support */
#define ENABLE_NAF_IP

/* Enable named pipe address family support */
#define ENABLE_NAF_NP

/* Enable NCACN protocol sequence */
#define ENABLE_PROT_NCACN

/* Enable NCADG protocol sequence */
/* #undef ENABLE_PROT_NCADG */

/* Feature test flags */
#define FEATURETEST_CFLAGS $FEATURETEST_CFLAGS

/* Number of arguments to gethostbyname_r */
#define GETHOSTBYNAME_R_ARGS

/* Define to 1 if you have the `backtrace' function. */
#define HAVE_BACKTRACE 1

/* Define to 1 if you have the `backtrace_symbols' function. */
#define HAVE_BACKTRACE_SYMBOLS 1

/* Define to 1 if you have the `backtrace_symbols_fd' function. */
#define HAVE_BACKTRACE_SYMBOLS_FD 1

/* Define to 1 if you have the `catopen' function. */
#define HAVE_CATOPEN 1

/* Define to 1 if you have the <CrashReporterClient.h> header file. */
#define HAVE_CRASHREPORTERCLIENT_H 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the <execinfo.h> header file. */
#define HAVE_EXECINFO_H 1

/* Define to 1 if you have the <features.h> header file. */
/* #undef HAVE_FEATURES_H */

/* Define to 1 if you have the `gethostbyname_r' function. */
/* #undef HAVE_GETHOSTBYNAME_R */

/* Define to 1 if you have the <getopt.h> header file. */
#define HAVE_GETOPT_H 1

/* Define to 1 if you have the `getpeereid' function. */
#define HAVE_GETPEEREID 1

/* Define to 1 if you have the `gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Define to 1 if you have the <gssapi/gssapi_ext.h> header file. */
/* #undef HAVE_GSSAPI_GSSAPI_EXT_H */

/* Define to 1 if you have the <gssapi/gssapi.h> header file. */
#define HAVE_GSSAPI_GSSAPI_H 1

/* Define to 1 if you have the <gssapi/gssapi_krb5.h> header file. */
#define HAVE_GSSAPI_GSSAPI_KRB5_H 1

/* Define if you have the GSS framework */
#define HAVE_GSS_FRAMEWORK 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define if you have the Kerberos framework */
#define HAVE_KERBEROS_FRAMEWORK 1

/* Define to 1 if you have the <krb5.h> header file. */
#define HAVE_KRB5_H 1

/* whether the Likewise lwioclient API is available */
/* #undef HAVE_LIKEWISE_LWIO */

/* whether the Likewise lwmapsecurity API is available */
/* #undef HAVE_LIKEWISE_LWMAPSECURITY */

/* Define to 1 if you have the <lwio/lwio.h> header file. */
/* #undef HAVE_LWIO_LWIO_H */

/* Define to 1 if you have the <lwmapsecurity/lwmapsecurity.h> header file. */
/* #undef HAVE_LWMAPSECURITY_LWMAPSECURITY_H */

/* Define to 1 if you have the <lw/base.h,> header file. */
/* #undef HAVE_LW_BASE_H_ */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Have MoonUnit */
/* #undef HAVE_MOONUNIT */

/* Define to 1 if you have the <moonunit/moonunit.h> header file. */
/* #undef HAVE_MOONUNIT_MOONUNIT_H */

/* Define to 1 if you have the <net/if_arp.h> header file. */
#define HAVE_NET_IF_ARP_H 1

/* Define to 1 if you have the <net/if_dl.h> header file. */
#define HAVE_NET_IF_DL_H 1

/* Define to 1 if you have the <nl_types.h> header file. */
#define HAVE_NL_TYPES_H 1

/* Define to 1 if you have the `pthread_atfork' function. */
#define HAVE_PTHREAD_ATFORK 1

/* Define to 1 if you have the `pthread_delay_np' function. */
/* #undef HAVE_PTHREAD_DELAY_NP */

/* Define to 1 if you have the <pthread_ext.h> header file. */
/* #undef HAVE_PTHREAD_EXT_H */

/* Define to 1 if you have the `pthread_getunique_np' function. */
/* #undef HAVE_PTHREAD_GETUNIQUE_NP */

/* Define to 1 if you have the `pthread_get_expiration_np' function. */
/* #undef HAVE_PTHREAD_GET_EXPIRATION_NP */

/* Define to 1 if you have the <pthread.h> header file. */
#define HAVE_PTHREAD_H 1

/* Define to 1 if you have the `pthread_ismultithreaded_np' function. */
/* #undef HAVE_PTHREAD_ISMULTITHREADED_NP */

/* Define to 1 if you have the `pthread_kill_other_threads_np' function. */
/* #undef HAVE_PTHREAD_KILL_OTHER_THREADS_NP */

/* Define to 1 if you have the `pthread_lock_global_np' function. */
/* #undef HAVE_PTHREAD_LOCK_GLOBAL_NP */

/* Define to 1 if you have the `pthread_mutexattr_getkind_np' function. */
/* #undef HAVE_PTHREAD_MUTEXATTR_GETKIND_NP */

/* Define to 1 if you have the `pthread_mutexattr_setkind_np' function. */
/* #undef HAVE_PTHREAD_MUTEXATTR_SETKIND_NP */

/* Define to 1 if you have the `pthread_setugid_np' function. */
#define HAVE_PTHREAD_SETUGID_NP 1

/* Define to 1 if you have the `pthread_unlock_global_np' function. */
/* #undef HAVE_PTHREAD_UNLOCK_GLOBAL_NP */

/* Define to 1 if you have the `pthread_yield' function. */
/* #undef HAVE_PTHREAD_YIELD */

/* Define to 1 if you have the `scandir' function. */
#define HAVE_SCANDIR 1

/* Define to 1 if you have the `sched_yield' function. */
#define HAVE_SCHED_YIELD 1

/* Define to 1 if you have the `setpgrp' function. */
#define HAVE_SETPGRP 1

/* whether the SMBClient.framework API is available */
#define HAVE_SMBCLIENT_FRAMEWORK 1

/* Define to 1 if you have the `socket' function. */
#define HAVE_SOCKET 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strlcat' function. */
#define HAVE_STRLCAT 1

/* Define to 1 if you have the `strlcpy' function. */
#define HAVE_STRLCPY 1

/* Define to 1 if you have the <sys/cdefs.h> header file. */
#define HAVE_SYS_CDEFS_H 1

/* Define to 1 if you have the <sys/fd_set.h> header file. */
/* #undef HAVE_SYS_FD_SET_H */

/* Define to 1 if you have the <sys/kauth.h> header file. */
#define HAVE_SYS_KAUTH_H 1

/* Define to 1 if you have the <sys/sockio.h> header file. */
#define HAVE_SYS_SOCKIO_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/syscall.h> header file. */
#define HAVE_SYS_SYSCALL_H 1

/* Define to 1 if you have the <sys/sysctl.h> header file. */
#define HAVE_SYS_SYSCTL_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/un.h> header file. */
#define HAVE_SYS_UN_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the <uuid/uuid.h> header file. */
#define HAVE_UUID_UUID_H 1

/* Define to 1 if you have the `vasprintf' function. */
#define HAVE_VASPRINTF 1

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#define LT_OBJDIR ".libs/"

/* IP network address family support */
#define NAF_IP 1

/* Named Pipes network address family support */
#define NAF_NP 1

/* AF_LOCAL network address family support (untested) */
/* #undef NAF_UXD */

/* Name of package */
#define PACKAGE "dcerpc"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "DCE RPC"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "DCE RPC 1.1.0.7"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "dce-rpc"

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.1.0.7"

/* Support connection based transports */
#define PROT_NCACN 1

/* Support connectionless transports */
/* #undef PROT_NCADG */

/* named pipes directory */
#define RPC_C_NP_DIR "/var/rpc/ncacn_np"

/* unix domain sockets directory */
#define RPC_C_UXD_DIR "/var/rpc/ncalrpc"

/* Apple SDKROOT we are building against */
#define SDKROOT "/"

/* Number of arguments to setgrp() */
#define SETPGRP_ARGS 0

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Include loopback interface support */
#define USE_LOOPBACK 1

/* Version number of package */
#define VERSION "1.1.0.7"

/* Tell libc that we want BSD things */
#define _BSD 1

/* Ask for gnu extensions */
#define _GNU_SOURCE 1

/* Ask for re-entrant api */
#define _REENTRANT 1

/* Ask for extension api */
#define __EXTENSIONS__ 1

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif
