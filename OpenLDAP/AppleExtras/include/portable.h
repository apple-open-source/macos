/* include/portable.h.  Generated automatically by configure.  */
/* include/portable.h.in.  Generated automatically from configure.in by autoheader.  */
/* This work is part of OpenLDAP Software <http://www.openldap.org/>.
 *
 * Copyright 1998-2004 The OpenLDAP Foundation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in the file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */

#ifndef _LDAP_PORTABLE_H
#define _LDAP_PORTABLE_H

/* end of preamble */


/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef gid_t */

/* Define if you don't have vprintf but do have _doprnt.  */
/* #undef HAVE_DOPRNT */

/* Define if your struct stat has st_blksize.  */
#define HAVE_ST_BLKSIZE 1

/* Define if you have the strftime function.  */
#define HAVE_STRFTIME 1

/* Define if you have <sys/wait.h> that is POSIX.1 compatible.  */
#define HAVE_SYS_WAIT_H 1

/* Define if you have the vprintf function.  */
#define HAVE_VPRINTF 1

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* Define to the type of arg1 for select(). */
#define SELECT_TYPE_ARG1 int

/* Define to the type of args 2, 3 and 4 for select(). */
#define SELECT_TYPE_ARG234 (fd_set *)

/* Define to the type of arg5 for select(). */
#define SELECT_TYPE_ARG5 (struct timeval *)

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/* Define if your <sys/time.h> declares struct tm.  */
/* #undef TM_IN_SYS_TIME */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef uid_t */

/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
#ifdef __BIG_ENDIAN__
#define WORDS_BIGENDIAN
#endif

/* define this if needed to get reentrant functions */
#ifndef REENTRANT
#define REENTRANT 1
#endif
#ifndef _REENTRANT
#define _REENTRANT 1
#endif

/* define this if needed to get threadsafe functions */
#ifndef THREADSAFE
#define THREADSAFE 1
#endif
#ifndef _THREADSAFE
#define _THREADSAFE 1
#endif
#ifndef THREAD_SAFE
#define THREAD_SAFE 1
#endif
#ifndef _THREAD_SAFE
#define _THREAD_SAFE 1
#endif

#ifndef _SGI_MP_SOURCE
#define _SGI_MP_SOURCE 1
#endif

/* These are defined in ldap_features.h */
/*
 LDAP_API_FEATURE_X_OPENLDAP_REENTRANT
 LDAP_API_FEATURE_X_OPENLDAP_THREAD_SAFE
 LDAP_API_FEATURE_X_OPENLDAP_V2_KBIND
 LDAP_API_FEATURE_X_OPENLDAP_V2_REFERRALS
*/

/* These are defined in lber_types.h */
/*
 LBER_INT_T
 LBER_LEN_T
 LBER_SOCKET_T
 LBER_TAG_T
*/

/* Define if you have the bcopy function.  */
#define HAVE_BCOPY 1

/* Define if you have the chroot function.  */
#define HAVE_CHROOT 1

/* Define if you have the closesocket function.  */
/* #undef HAVE_CLOSESOCKET */

/* Define if you have the ctime_r function.  */
#define HAVE_CTIME_R 1

/* Define if you have the endgrent function.  */
#define HAVE_ENDGRENT 1

/* Define if you have the endpwent function.  */
#define HAVE_ENDPWENT 1

/* Define if you have the fcntl function.  */
#define HAVE_FCNTL 1

/* Define if you have the flock function.  */
#define HAVE_FLOCK 1

/* Define if you have the fstat function.  */
#define HAVE_FSTAT 1

/* Define if you have the gai_strerror function.  */
#define HAVE_GAI_STRERROR 1

/* Define if you have the getaddrinfo function.  */
#define HAVE_GETADDRINFO 1

/* Define if you have the getdtablesize function.  */
#define HAVE_GETDTABLESIZE 1

/* Define if you have the getgrgid function.  */
#define HAVE_GETGRGID 1

/* Define if you have the gethostbyaddr_r function.  */
/* #undef HAVE_GETHOSTBYADDR_R */

/* Define if you have the gethostbyname_r function.  */
/* #undef HAVE_GETHOSTBYNAME_R */

/* Define if you have the gethostname function.  */
#define HAVE_GETHOSTNAME 1

/* Define if you have the getnameinfo function.  */
#define HAVE_GETNAMEINFO 1

/* Define if you have the getopt function.  */
#define HAVE_GETOPT 1

/* Define if you have the getpass function.  */
#define HAVE_GETPASS 1

/* Define if you have the getpassphrase function.  */
/* #undef HAVE_GETPASSPHRASE */

/* Define if you have the getpeereid function.  */
#define HAVE_GETPEEREID 1

/* Define if you have the getpwnam function.  */
#define HAVE_GETPWNAM 1

/* Define if you have the getpwuid function.  */
#define HAVE_GETPWUID 1

/* Define if you have the getspnam function.  */
/* #undef HAVE_GETSPNAM */

/* Define if you have the gettimeofday function.  */
#define HAVE_GETTIMEOFDAY 1

/* Define if you have the inet_ntop function.  */
#define HAVE_INET_NTOP 1

/* Define if you have the initgroups function.  */
#define HAVE_INITGROUPS 1

/* Define if you have the lockf function.  */
#define HAVE_LOCKF 1

/* Define if you have the memcpy function.  */
#define HAVE_MEMCPY 1

/* Define if you have the memmove function.  */
#define HAVE_MEMMOVE 1

/* Define if you have the mkstemp function.  */
#define HAVE_MKSTEMP 1

/* Define if you have the mktemp function.  */
#define HAVE_MKTEMP 1

/* Define if you have the pipe function.  */
#define HAVE_PIPE 1

/* Define if you have the pthread_getconcurrency function.  */
#define HAVE_PTHREAD_GETCONCURRENCY 1

/* Define if you have the pthread_kill function.  */
#define HAVE_PTHREAD_KILL 1

/* Define if you have the pthread_kill_other_threads_np function.  */
/* #undef HAVE_PTHREAD_KILL_OTHER_THREADS_NP */

/* Define if you have the pthread_rwlock_destroy function.  */
#define HAVE_PTHREAD_RWLOCK_DESTROY 1

/* Define if you have the pthread_setconcurrency function.  */
#define HAVE_PTHREAD_SETCONCURRENCY 1

/* Define if you have the pthread_yield function.  */
/* #undef HAVE_PTHREAD_YIELD */

/* Define if you have the read function.  */
#define HAVE_READ 1

/* Define if you have the recv function.  */
#define HAVE_RECV 1

/* Define if you have the recvfrom function.  */
#define HAVE_RECVFROM 1

/* Define if you have the sched_yield function.  */
#define HAVE_SCHED_YIELD 1

/* Define if you have the send function.  */
#define HAVE_SEND 1

/* Define if you have the sendmsg function.  */
#define HAVE_SENDMSG 1

/* Define if you have the sendto function.  */
#define HAVE_SENDTO 1

/* Define if you have the setegid function.  */
#define HAVE_SETEGID 1

/* Define if you have the seteuid function.  */
#define HAVE_SETEUID 1

/* Define if you have the setgid function.  */
#define HAVE_SETGID 1

/* Define if you have the setpwfile function.  */
/* #undef HAVE_SETPWFILE */

/* Define if you have the setsid function.  */
#define HAVE_SETSID 1

/* Define if you have the setuid function.  */
#define HAVE_SETUID 1

/* Define if you have the sigaction function.  */
#define HAVE_SIGACTION 1

/* Define if you have the signal function.  */
#define HAVE_SIGNAL 1

/* Define if you have the sigset function.  */
#define HAVE_SIGSET 1

/* Define if you have the snprintf function.  */
#define HAVE_SNPRINTF 1

/* Define if you have the strdup function.  */
#define HAVE_STRDUP 1

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

/* Define if you have the strpbrk function.  */
#define HAVE_STRPBRK 1

/* Define if you have the strrchr function.  */
#define HAVE_STRRCHR 1

/* Define if you have the strsep function.  */
#define HAVE_STRSEP 1

/* Define if you have the strspn function.  */
#define HAVE_STRSPN 1

/* Define if you have the strstr function.  */
#define HAVE_STRSTR 1

/* Define if you have the strtol function.  */
#define HAVE_STRTOL 1

/* Define if you have the strtoll function.  */
#define HAVE_STRTOLL 1

/* Define if you have the strtoq function.  */
#define HAVE_STRTOQ 1

/* Define if you have the strtoul function.  */
#define HAVE_STRTOUL 1

/* Define if you have the strtouq function.  */
#define HAVE_STRTOUQ 1

/* Define if you have the sysconf function.  */
#define HAVE_SYSCONF 1

/* Define if you have the thr_getconcurrency function.  */
/* #undef HAVE_THR_GETCONCURRENCY */

/* Define if you have the thr_setconcurrency function.  */
/* #undef HAVE_THR_SETCONCURRENCY */

/* Define if you have the thr_yield function.  */
/* #undef HAVE_THR_YIELD */

/* Define if you have the usleep function.  */
#define HAVE_USLEEP 1

/* Define if you have the vsnprintf function.  */
#define HAVE_VSNPRINTF 1

/* Define if you have the wait4 function.  */
#define HAVE_WAIT4 1

/* Define if you have the waitpid function.  */
#define HAVE_WAITPID 1

/* Define if you have the write function.  */
#define HAVE_WRITE 1

/* Define if you have the <arpa/inet.h> header file.  */
#define HAVE_ARPA_INET_H 1

/* Define if you have the <arpa/nameser.h> header file.  */
#define HAVE_ARPA_NAMESER_H 1

/* Define if you have the <assert.h> header file.  */
#define HAVE_ASSERT_H 1

/* Define if you have the <bits/types.h> header file.  */
/* #undef HAVE_BITS_TYPES_H */

/* Define if you have the <conio.h> header file.  */
/* #undef HAVE_CONIO_H */

/* Define if you have the <crypt.h> header file.  */
/* #undef HAVE_CRYPT_H */

/* Define if you have the <cthreads.h> header file.  */
/* #undef HAVE_CTHREADS_H */

/* Define if you have the <db.h> header file.  */
#define HAVE_DB_H 1

/* Define if you have the <db_185.h> header file.  */
/* #undef HAVE_DB_185_H */

/* Define if you have the <des.h> header file.  */
/* #undef HAVE_DES_H */

/* Define if you have the <direct.h> header file.  */
/* #undef HAVE_DIRECT_H */

/* Define if you have the <dirent.h> header file.  */
#define HAVE_DIRENT_H 1

/* Define if you have the <dlfcn.h> header file.  */
#define HAVE_DLFCN_H 1

/* Define if you have the <errno.h> header file.  */
#define HAVE_ERRNO_H 1

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <filio.h> header file.  */
/* #undef HAVE_FILIO_H */

/* Define if you have the <gdbm.h> header file.  */
/* #undef HAVE_GDBM_H */

/* Define if you have the <getopt.h> header file.  */
#define HAVE_GETOPT_H 1

/* Define if you have the <grp.h> header file.  */
#define HAVE_GRP_H 1

/* Define if you have the <heim_err.h> header file.  */
/* #undef HAVE_HEIM_ERR_H */

/* Define if you have the <io.h> header file.  */
/* #undef HAVE_IO_H */

/* Define if you have the <kerberosIV/des.h> header file.  */
/* #undef HAVE_KERBEROSIV_DES_H */

/* Define if you have the <kerberosIV/krb.h> header file.  */
/* #undef HAVE_KERBEROSIV_KRB_H */

/* Define if you have the <krb-archaeology.h> header file.  */
/* #undef HAVE_KRB_ARCHAEOLOGY_H */

/* Define if you have the <krb.h> header file.  */
/* #undef HAVE_KRB_H */

/* Define if you have the <krb5.h> header file.  */
/* #undef HAVE_KRB5_H */

/* Define if you have the <libutil.h> header file.  */
/* #undef HAVE_LIBUTIL_H */

/* Define if you have the <limits.h> header file.  */
#define HAVE_LIMITS_H 1

/* Define if you have the <locale.h> header file.  */
#define HAVE_LOCALE_H 1

/* Define if you have the <ltdl.h> header file.  */
/* #undef HAVE_LTDL_H */

/* Define if you have the <lwp/lwp.h> header file.  */
/* #undef HAVE_LWP_LWP_H */

/* Define if you have the <mach/cthreads.h> header file.  */
/* #undef HAVE_MACH_CTHREADS_H */

/* Define if you have the <malloc.h> header file.  */
/* #undef HAVE_MALLOC_H */

/* Define if you have the <mdbm.h> header file.  */
/* #undef HAVE_MDBM_H */

/* Define if you have the <memory.h> header file.  */
#define HAVE_MEMORY_H 1

/* Define if you have the <ndbm.h> header file.  */
/* #undef HAVE_NDBM_H */

/* Define if you have the <ndir.h> header file.  */
/* #undef HAVE_NDIR_H */

/* Define if you have the <netinet/tcp.h> header file.  */
#define HAVE_NETINET_TCP_H 1

/* Define if you have the <openssl/ssl.h> header file.  */
#define HAVE_OPENSSL_SSL_H 1

/* Define if you have the <process.h> header file.  */
/* #undef HAVE_PROCESS_H */

/* Define if you have the <psap.h> header file.  */
/* #undef HAVE_PSAP_H */

/* Define if you have the <pth.h> header file.  */
/* #undef HAVE_PTH_H */

/* Define if you have the <pthread.h> header file.  */
#define HAVE_PTHREAD_H 1

/* Define if you have the <pwd.h> header file.  */
#define HAVE_PWD_H 1

/* Define if you have the <regex.h> header file.  */
#define HAVE_REGEX_H 1

/* Define if you have the <resolv.h> header file.  */
#define HAVE_RESOLV_H 1

/* Define if you have the <sasl.h> header file.  */
#define HAVE_SASL_H 1

/* Define if you have the <sasl/sasl.h> header file.  */
/* #undef HAVE_SASL_SASL_H */

/* Define if you have the <sched.h> header file.  */
#define HAVE_SCHED_H 1

/* Define if you have the <sgtty.h> header file.  */
#define HAVE_SGTTY_H 1

/* Define if you have the <shadow.h> header file.  */
/* #undef HAVE_SHADOW_H */

/* Define if you have the <slp.h> header file.  */
/* #undef HAVE_SLP_H */

/* Define if you have the <sql.h> header file.  */
/* #undef HAVE_SQL_H */

/* Define if you have the <sqlext.h> header file.  */
/* #undef HAVE_SQLEXT_H */

/* Define if you have the <ssl.h> header file.  */
/* #undef HAVE_SSL_H */

/* Define if you have the <stddef.h> header file.  */
#define HAVE_STDDEF_H 1

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <strings.h> header file.  */
#define HAVE_STRINGS_H 1

/* Define if you have the <synch.h> header file.  */
/* #undef HAVE_SYNCH_H */

/* Define if you have the <sys/dir.h> header file.  */
/* #undef HAVE_SYS_DIR_H */

/* Define if you have the <sys/errno.h> header file.  */
#define HAVE_SYS_ERRNO_H 1

/* Define if you have the <sys/file.h> header file.  */
#define HAVE_SYS_FILE_H 1

/* Define if you have the <sys/filio.h> header file.  */
#define HAVE_SYS_FILIO_H 1

/* Define if you have the <sys/ioctl.h> header file.  */
#define HAVE_SYS_IOCTL_H 1

/* Define if you have the <sys/ndir.h> header file.  */
/* #undef HAVE_SYS_NDIR_H */

/* Define if you have the <sys/param.h> header file.  */
#define HAVE_SYS_PARAM_H 1

/* Define if you have the <sys/resource.h> header file.  */
#define HAVE_SYS_RESOURCE_H 1

/* Define if you have the <sys/select.h> header file.  */
#define HAVE_SYS_SELECT_H 1

/* Define if you have the <sys/socket.h> header file.  */
#define HAVE_SYS_SOCKET_H 1

/* Define if you have the <sys/stat.h> header file.  */
#define HAVE_SYS_STAT_H 1

/* Define if you have the <sys/syslog.h> header file.  */
#define HAVE_SYS_SYSLOG_H 1

/* Define if you have the <sys/time.h> header file.  */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <sys/types.h> header file.  */
#define HAVE_SYS_TYPES_H 1

/* Define if you have the <sys/ucred.h> header file.  */
#define HAVE_SYS_UCRED_H 1

/* Define if you have the <sys/uio.h> header file.  */
#define HAVE_SYS_UIO_H 1

/* Define if you have the <sys/un.h> header file.  */
#define HAVE_SYS_UN_H 1

/* Define if you have the <sys/uuid.h> header file.  */
/* #undef HAVE_SYS_UUID_H */

/* Define if you have the <sysexits.h> header file.  */
#define HAVE_SYSEXITS_H 1

/* Define if you have the <syslog.h> header file.  */
#define HAVE_SYSLOG_H 1

/* Define if you have the <tcpd.h> header file.  */
/* #undef HAVE_TCPD_H */

/* Define if you have the <termios.h> header file.  */
#define HAVE_TERMIOS_H 1

/* Define if you have the <thread.h> header file.  */
/* #undef HAVE_THREAD_H */

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the <winsock.h> header file.  */
/* #undef HAVE_WINSOCK_H */

/* Define if you have the <winsock2.h> header file.  */
/* #undef HAVE_WINSOCK2_H */

/* Define if you have the V3 library (-lV3).  */
/* #undef HAVE_LIBV3 */

/* Define if you have the bind library (-lbind).  */
/* #undef HAVE_LIBBIND */

/* Define if you have the gen library (-lgen).  */
/* #undef HAVE_LIBGEN */

/* Define if you have the inet library (-linet).  */
/* #undef HAVE_LIBINET */

/* Define if you have the net library (-lnet).  */
/* #undef HAVE_LIBNET */

/* Define if you have the nsl library (-lnsl).  */
/* #undef HAVE_LIBNSL */

/* Define if you have the nsl_s library (-lnsl_s).  */
/* #undef HAVE_LIBNSL_S */

/* Define if you have the resolv library (-lresolv).  */
/* #undef HAVE_LIBRESOLV */

/* Define if you have the socket library (-lsocket).  */
/* #undef HAVE_LIBSOCKET */

/* Package */
#define OPENLDAP_PACKAGE "OpenLDAP"

/* Version */
#define OPENLDAP_VERSION "2.2.19"

/* Version */
#define LDAP_VENDOR_VERSION 20219

/* Major */
#define LDAP_VENDOR_VERSION_MAJOR 2

/* Minor */
#define LDAP_VENDOR_VERSION_MINOR 2

/* Patch */
#define LDAP_VENDOR_VERSION_PATCH 19

/* define this if you have mkversion */
#define HAVE_MKVERSION 1

/* defined to be the EXE extension */
#define EXEEXT ""

/* define if you have AIX security lib */
/* #undef HAVE_AIX_SECURITY */

/* define to use both <string.h> and <strings.h> */
/* #undef BOTH_STRINGS_H */

/* define if you have libtool -ltdl */
/* #undef HAVE_LIBLTDL */

/* define if system uses EBCDIC instead of ASCII */
/* #undef HAVE_EBCDIC */

/* Define if TIOCGWINSZ requires sys/ioctl.h */
/* #undef GWINSZ_IN_SYS_IOCTL */

/* define if you have POSIX termios */
#define HAVE_POSIX_TERMIOS 1

/* define if you have winsock */
/* #undef HAVE_WINSOCK */

/* define if you have winsock2 */
/* #undef HAVE_WINSOCK2 */

/* define if you have uuid_to_str() */
/* #undef HAVE_UUID_TO_STR */

/* define if you have res_query() */
#define HAVE_RES_QUERY 1

/* define if you have HEIMDAL Kerberos */
/* #undef HAVE_HEIMDAL_KERBEROS */

/* define if you have Kerberos V */
/* #undef HAVE_KRB5 */

/* define if you have Kerberos V with IV support */
/* #undef HAVE_KRB425 */

/* define if you have Kerberos IV */
/* #undef HAVE_KRB4 */

/* define if you have Kerberos des_debug */
/* #undef HAVE_DES_DEBUG */

/* define if you have Kerberos IV */
/* #undef HAVE_KRB4 */

/* define if you have Kth Kerberos */
/* #undef HAVE_KTH_KERBEROS */

/* define if you have Kerberos */
/* #undef HAVE_KERBEROS */

/* define if you have SSLeay or OpenSSL */
#define HAVE_SSLEAY 1

/* define if you have RSAref */
/* #undef HAVE_RSAREF */

/* define if you have TLS */
#define HAVE_TLS 1

/* define to support LAN Manager passwords */
/* #undef SLAPD_LMHASH */

/* if you have NT Threads */
/* #undef HAVE_NT_THREADS */

/* if you have NT Service Manager */
/* #undef HAVE_NT_SERVICE_MANAGER */

/* if you have NT Event Log */
/* #undef HAVE_NT_EVENT_LOG */

/* define to pthreads API spec revision */
#define HAVE_PTHREADS 10

/* if you have LinuxThreads */
/* #undef HAVE_LINUX_THREADS */

/* Define if you have the sched_yield function. */
#define HAVE_SCHED_YIELD 1

/* define if you have pthread_detach function */
#define HAVE_PTHREAD_DETACH 1

/* define if you have Mach Cthreads */
/* #undef HAVE_MACH_CTHREADS */

/* if you have GNU Pth */
/* #undef HAVE_GNU_PTH */

/* if you have Solaris LWP (thr) package */
/* #undef HAVE_THR */

/* if you have SunOS LWP package */
/* #undef HAVE_LWP */

/* define if select implicitly yields */
#define HAVE_YIELDING_SELECT 1

/* if you have LinuxThreads */
/* #undef HAVE_LINUX_THREADS */

/* define if you have (or want) no threads */
/* #undef NO_THREADS */

/* set to the number of arguments ctime_r() expects */
#define CTIME_R_NARGS 2

/* set to the number of arguments gethostbyname_r() expects */
/* #undef GETHOSTBYNAME_R_NARGS */

/* set to the number of arguments gethostbyaddr_r() expects */
/* #undef GETHOSTBYADDR_R_NARGS */

/* define if Berkeley DB has DB_THREAD support */
#define HAVE_BERKELEY_DB_THREAD 1

/* define this if Berkeley DB is available */
#define HAVE_BERKELEY_DB 1

/* define this to use DBHASH w/ LDBM backend */
/* #undef LDBM_USE_DBHASH */

/* define this to use DBBTREE w/ LDBM backend */
#define LDBM_USE_DBBTREE 1

/* define if MDBM is available */
/* #undef HAVE_MDBM */

/* define if GNU DBM is available */
/* #undef HAVE_GDBM */

/* define if NDBM is available */
/* #undef HAVE_NDBM */

/* define if LDAP libs are dynamic */
/* #undef LDAP_LIBS_DYNAMIC */

/* define if you have -lwrap */
/* #undef HAVE_TCPD */

/* define if you have Cyrus SASL */
#define HAVE_CYRUS_SASL 1

/* define if your SASL library has sasl_version() */
#define HAVE_SASL_VERSION 1

/* set to urandom device */
#define URANDOM_DEVICE "/dev/urandom"

/* define if you actually have FreeBSD fetch(3) */
/* #undef HAVE_FETCH */

/* define if crypt(3) is available */
#define HAVE_CRYPT 1

/* define if setproctitle(3) is available */
/* #undef HAVE_SETPROCTITLE */

/* define if you have -lslp */
/* #undef HAVE_SLP */

/* define if you have 'long long' */
#define HAVE_LONG_LONG 1

/* Define to `int' if <sys/types.h> does not define. */
/* #undef mode_t */

/* Define to `long' if <sys/types.h> does not define. */
/* #undef off_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef pid_t */

/* Define if system has ptrdiff_t type */
#define HAVE_PTRDIFF_T 1

/* Define to `unsigned' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define to `signed int' if <sys/types.h> does not define. */
/* #undef ssize_t */

/* Define to `char *' if <sys/types.h> does not define. */
/* #undef caddr_t */

/* define to int if socklen_t is not available */
/* #undef socklen_t */

/* define to atomic type if sig_atomic_t is not available */
/* #undef sig_atomic_t */

/* define if struct passwd has pw_gecos */
#define HAVE_PW_GECOS 1

/* define if struct passwd has pw_passwd */
#define HAVE_PW_PASSWD 1

/* define if toupper() requires islower() */
/* #undef C_UPPER_LOWER */

/* define as empty if volatile is not supported */
/* #undef volatile */

/* define if cross compiling */
/* #undef CROSS_COMPILING */

/* The number of bytes in type short */
#define SIZEOF_SHORT 2

/* The number of bytes in type int */
#define SIZEOF_INT 4

/* The number of bytes in type long */
#define SIZEOF_LONG 4

/* The number of bytes in type wchar_t */
#define SIZEOF_WCHAR_T 4

/* define to you inet_aton(3) is available */
#define HAVE_INET_ATON 1

/* if you have spawnlp() */
/* #undef HAVE_SPAWNLP */

/* define to snprintf routine */
/* #undef snprintf */

/* define to vsnprintf routine */
/* #undef vsnprintf */

/* define if struct msghdr has msg_accrights */
/* #undef HAVE_MSGHDR_MSG_ACCRIGHTS */

/* define to snprintf routine */
/* #undef snprintf */

/* define to snprintf routine */
/* #undef vsnprintf */

/* define if sys_errlist is not declared in stdio.h or errno.h */
/* #undef DECL_SYS_ERRLIST */

/* define if you actually have sys_errlist in your libs */
#define HAVE_SYS_ERRLIST 1

/* define if you have libtool -ltdl */
/* #undef HAVE_LIBLTDL */

/* define this to add SLAPI code */
/* #undef LDAP_SLAPI */

/* define this to add debugging code */
#define LDAP_DEBUG 1

/* define this to add syslog code */
#define LDAP_SYSLOG 1

/* define this for LDAP process title support */
#define LDAP_PROCTITLE 1

/* define to support PF_LOCAL */
#define LDAP_PF_LOCAL 1

/* define to support PF_INET6 */
#define LDAP_PF_INET6 1

/* define to support cleartext passwords */
/* #undef SLAPD_CLEARTEXT */

/* define to support crypt(3) passwords */
#define SLAPD_CRYPT 1

/* define to support SASL passwords */
/* #undef SLAPD_SPASSWD */

/* define to support multimaster replication */
/* #undef SLAPD_MULTIMASTER */

/* define to support reverse lookups */
/* #undef SLAPD_RLOOKUPS */

/* define to support per-object ACIs */
/* #undef SLAPD_ACI_ENABLED */

/* define to support modules */
/* #undef SLAPD_MODULES */

/* statically linked module */
#define SLAPD_MOD_STATIC 1

/* dynamically linked module */
#define SLAPD_MOD_DYNAMIC 2

/* define to support BDB backend */
#define SLAPD_BDB SLAPD_MOD_STATIC

/* define to support DNS SRV backend */
/* #undef SLAPD_DNSSRV */

/* define to support HDB backend */
/* #undef SLAPD_HDB */

/* define to support LDAP backend */
/* #undef SLAPD_LDAP */

/* define to support LDBM backend */
#define SLAPD_LDBM SLAPD_MOD_STATIC

/* define to support LDAP Metadirectory backend */
/* #undef SLAPD_META */

/* define to support cn=Monitor backend */
#define SLAPD_MONITOR SLAPD_MOD_STATIC

/* define to support NULL backend */
/* #undef SLAPD_NULL */

/* define to support NetInfo backend */
#define SLAPD_NETINFO SLAPD_MOD_STATIC

/* define to support PASSWD backend */
/* #undef SLAPD_PASSWD */

/* define to support PERL backend */
/* #undef SLAPD_PERL */

/* define to support SHELL backend */
/* #undef SLAPD_SHELL */

/* define to support SQL backend */
/* #undef SLAPD_SQL */

/* define for Dynamic Group overlay */
/* #undef SLAPD_OVER_DYNGROUP */

/* define for Proxy Cache overlay */
/* #undef SLAPD_OVER_PROXYCACHE */

/* define to enable rewriting in back-ldap and back-meta */
#define ENABLE_REWRITE 1

/* define to enable slapi library */
/* #undef ENABLE_SLAPI */


/* begin of postamble */

#ifdef _WIN32
	/* don't suck in all of the win32 api */
#	define WIN32_LEAN_AND_MEAN 1
#endif

#ifndef LDAP_NEEDS_PROTOTYPES
/* force LDAP_P to always include prototypes */
#define LDAP_NEEDS_PROTOTYPES 1
#endif

#ifndef LDAP_REL_ENG
#if (LDAP_VENDOR_VERSION == 000000) && !defined(LDAP_DEVEL)
#define LDAP_DEVEL
#endif
#if defined(LDAP_DEVEL) && !defined(LDAP_TEST)
#define LDAP_TEST
#endif
#endif

#ifdef HAVE_STDDEF_H
#	include <stddef.h>
#endif

#ifdef HAVE_EBCDIC 
/* ASCII/EBCDIC converting replacements for stdio funcs
 * vsnprintf and snprintf are used too, but they are already
 * checked by the configure script
 */
#define fputs ber_pvt_fputs
#define fgets ber_pvt_fgets
#define printf ber_pvt_printf
#define fprintf ber_pvt_fprintf
#define vfprintf ber_pvt_vfprintf
#define vsprintf ber_pvt_vsprintf
#endif

#include "ac/fdset.h"

#include "ldap_cdefs.h"
#include "ldap_features.h"

#include "ac/assert.h"
#include "ac/localize.h"

#endif /* _LDAP_PORTABLE_H */
