/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* why not HAVE_P_S? */
/* #undef CALL_PTHREAD_SETCONCURRENCY */

/* Define to one of `_getb67', `GETB67', `getb67' for Cray-2 and Cray-YMP
   systems. This function is required for `alloca.c' support on those systems.
   */
/* #undef CRAY_STACKSEG_END */

/* Define to 1 if using `alloca.c'. */
/* #undef C_ALLOCA */

/* Enable debugging code? */
#define DEBUG 1

/* Directory separator character, usually / or \\ */
#define DIR_SEP '/'

/* number of args to el_init() */
#define EL_INIT_ARGS 4

/* nls support in libopts */
/* #undef ENABLE_NLS */

/* successful termination */
/* #undef EX_OK */

/* internal software error */
/* #undef EX_SOFTWARE */

/* fopen(3) accepts a 'b' in the mode flag */
#define FOPEN_BINARY_FLAG "b"

/* fopen(3) accepts a 't' in the mode flag */
#define FOPEN_TEXT_FLAG "t"

/* What is getsockname()'s socklen type? */
#define GETSOCKNAME_SOCKLEN_TYPE socklen_t

/* Define to 1 if you have `alloca', as a function or macro. */
#define HAVE_ALLOCA 1

/* Define to 1 if you have <alloca.h> and it should be used (not on Ultrix).
   */
#define HAVE_ALLOCA_H 1

/* Define to 1 if you have the <arpa/nameser.h> header file. */
#define HAVE_ARPA_NAMESER_H 1

/* Define to 1 if you have the `canonicalize_file_name' function. */
/* #undef HAVE_CANONICALIZE_FILE_NAME */

/* Define to 1 if you have the `chmod' function. */
#define HAVE_CHMOD 1

/* Define to 1 if you have the `clock_getres' function. */
/* #undef HAVE_CLOCK_GETRES */

/* Define to 1 if you have the `clock_gettime' function. */
/* #undef HAVE_CLOCK_GETTIME */

/* Define to 1 if you have the `clock_settime' function. */
/* #undef HAVE_CLOCK_SETTIME */

/* Define to 1 if you have the <cthreads.h> header file. */
/* #undef HAVE_CTHREADS_H */

/* Define to 1 if you have the declaration of `strerror_r', and to 0 if you
   don't. */
#define HAVE_DECL_STRERROR_R 1

/* Define this if /dev/zero is readable device */
#define HAVE_DEV_ZERO 1

/* Define to 1 if you have the <dirent.h> header file, and it defines `DIR'.
   */
#define HAVE_DIRENT_H 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you don't have `vprintf' but do have `_doprnt.' */
/* #undef HAVE_DOPRNT */

/* Can we drop root privileges? */
/* #undef HAVE_DROPROOT */

/* Define to 1 if you have the <errno.h> header file. */
#define HAVE_ERRNO_H 1

/* Define to 1 if you have the `EVP_MD_do_all_sorted' function. */
/* #undef HAVE_EVP_MD_DO_ALL_SORTED */

/* Define to 1 if you have the `fchmod' function. */
#define HAVE_FCHMOD 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the `fork' function. */
#define HAVE_FORK 1

/* Define to 1 if you have the `fstat' function. */
#define HAVE_FSTAT 1

/* Define to 1 if you have the `getclock' function. */
/* #undef HAVE_GETCLOCK */

/* Define to 1 if you have the `getdtablesize' function. */
#define HAVE_GETDTABLESIZE 1

/* Define to 1 if you have the `getifaddrs' function. */
#define HAVE_GETIFADDRS 1

/* if you have GNU Pth */
/* #undef HAVE_GNU_PTH */

/* Define to 1 if you have the <histedit.h> header file. */
#define HAVE_HISTEDIT_H 1

/* Define to 1 if you have the <history.h> header file. */
/* #undef HAVE_HISTORY_H */

/* Define to 1 if you have the `if_nametoindex' function. */
#define HAVE_IF_NAMETOINDEX 1

/* inline keyword or macro available */
#define HAVE_INLINE 1

/* Define to 1 if the system has the type `int16_t'. */
#define HAVE_INT16_T 1

/* Define to 1 if the system has the type `int32'. */
/* #undef HAVE_INT32 */

/* int32 type in DNS headers, not others. */
/* #undef HAVE_INT32_ONLY_WITH_DNS */

/* Define to 1 if the system has the type `int32_t'. */
#define HAVE_INT32_T 1

/* Define to 1 if the system has the type `int8_t'. */
#define HAVE_INT8_T 1

/* Define to 1 if the system has the type `intmax_t'. */
/* #undef HAVE_INTMAX_T */

/* Define to 1 if the system has the type `intptr_t'. */
#define HAVE_INTPTR_T 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `gen' library (-lgen). */
/* #undef HAVE_LIBGEN */

/* Define to 1 if you have the <libgen.h> header file. */
#define HAVE_LIBGEN_H 1

/* Define to 1 if you have the `intl' library (-lintl). */
/* #undef HAVE_LIBINTL */

/* Define to 1 if you have the <libintl.h> header file. */
/* #undef HAVE_LIBINTL_H */

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* using Linux pthread? */
/* #undef HAVE_LINUXTHREADS */

/* Do we have Linux capabilities? */
/* #undef HAVE_LINUX_CAPABILITIES */

/* Define to 1 if you have the <linux/if_addr.h> header file. */
/* #undef HAVE_LINUX_IF_ADDR_H */

/* if you have LinuxThreads */
/* #undef HAVE_LINUX_THREADS */

/* Define to 1 if you have the `localeconv' function. */
/* #undef HAVE_LOCALECONV */

/* Define to 1 if you have the <locale.h> header file. */
/* #undef HAVE_LOCALE_H */

/* Define to 1 if the system has the type `long double'. */
/* #undef HAVE_LONG_DOUBLE */

/* Define to 1 if the system has the type `long long'. */
#define HAVE_LONG_LONG 1

/* Define to 1 if the system has the type `long long int'. */
/* #undef HAVE_LONG_LONG_INT */

/* if you have SunOS LWP package */
/* #undef HAVE_LWP */

/* Define to 1 if you have the <lwp/lwp.h> header file. */
/* #undef HAVE_LWP_LWP_H */

/* define if you have Mach Cthreads */
/* #undef HAVE_MACH_CTHREADS */

/* Define to 1 if you have the <mach/cthreads.h> header file. */
/* #undef HAVE_MACH_CTHREADS_H */

/* Define to 1 if you have the `MD5Init' function. */
/* #undef HAVE_MD5INIT */

/* Define to 1 if you have the <md5.h> header file. */
/* #undef HAVE_MD5_H */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `mmap' function. */
#define HAVE_MMAP 1

/* Define to 1 if you have the <ndir.h> header file, and it defines `DIR'. */
/* #undef HAVE_NDIR_H */

/* Define to 1 if you have the <netdb.h> header file. */
#define HAVE_NETDB_H 1

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* Define to 1 if you have the <netinet/in_system.h> header file. */
/* #undef HAVE_NETINET_IN_SYSTEM_H */

/* Define to 1 if you have the <netinet/in_systm.h> header file. */
#define HAVE_NETINET_IN_SYSTM_H 1

/* Define to 1 if you have the <netinet/in_var.h> header file. */
#define HAVE_NETINET_IN_VAR_H 1

/* Define to 1 if you have the <netinet/ip.h> header file. */
#define HAVE_NETINET_IP_H 1

/* Define to 1 if you have the <net/if.h> header file. */
#define HAVE_NET_IF_H 1

/* Define to 1 if you have the <net/if_var.h> header file. */
#define HAVE_NET_IF_VAR_H 1

/* sntp does not care about 'nice' */
#define HAVE_NO_NICE 1

/* if you have NT Event Log */
/* #undef HAVE_NT_EVENT_LOG */

/* if you have NT Service Manager */
/* #undef HAVE_NT_SERVICE_MANAGER */

/* if you have NT Threads */
/* #undef HAVE_NT_THREADS */

/* Define this if pathfind(3) works */
/* #undef HAVE_PATHFIND */

/* Define to 1 if the system has the type `pid_t'. */
#define HAVE_PID_T 1

/* Define to 1 if you have the <priv.h> header file. */
/* #undef HAVE_PRIV_H */

/* Define if you have POSIX threads libraries and header files. */
/* #undef HAVE_PTHREAD */

/* define to pthreads API spec revision */
#define HAVE_PTHREADS 10

/* Define to 1 if you have the `pthread_attr_getstacksize' function. */
/* #undef HAVE_PTHREAD_ATTR_GETSTACKSIZE */

/* Define to 1 if you have the `pthread_attr_setstacksize' function. */
/* #undef HAVE_PTHREAD_ATTR_SETSTACKSIZE */

/* define if you have pthread_detach function */
#define HAVE_PTHREAD_DETACH 1

/* Define to 1 if you have the `pthread_getconcurrency' function. */
#define HAVE_PTHREAD_GETCONCURRENCY 1

/* Define to 1 if you have the <pthread.h> header file. */
#define HAVE_PTHREAD_H 1

/* Define to 1 if you have the `pthread_kill' function. */
#define HAVE_PTHREAD_KILL 1

/* Define to 1 if you have the `pthread_kill_other_threads_np' function. */
/* #undef HAVE_PTHREAD_KILL_OTHER_THREADS_NP */

/* define if you have pthread_rwlock_destroy function */
#define HAVE_PTHREAD_RWLOCK_DESTROY 1

/* Define to 1 if you have the `pthread_setconcurrency' function. */
#define HAVE_PTHREAD_SETCONCURRENCY 1

/* Define to 1 if you have the `pthread_yield' function. */
/* #undef HAVE_PTHREAD_YIELD */

/* Define to 1 if you have the <pth.h> header file. */
/* #undef HAVE_PTH_H */

/* Define to 1 if the system has the type `ptrdiff_t'. */
#define HAVE_PTRDIFF_T 1

/* Define to 1 if you have the <readline.h> header file. */
/* #undef HAVE_READLINE_H */

/* Define if your readline library has \`add_history' */
#define HAVE_READLINE_HISTORY 1

/* Define to 1 if you have the <readline/history.h> header file. */
#define HAVE_READLINE_HISTORY_H 1

/* Define to 1 if you have the <readline/readline.h> header file. */
#define HAVE_READLINE_READLINE_H 1

/* Define this if we have a functional realpath(3C) */
#define HAVE_REALPATH 1

/* Define to 1 if you have the `recvmsg' function. */
#define HAVE_RECVMSG 1

/* Define to 1 if you have the <resolv.h> header file. */
#define HAVE_RESOLV_H 1

/* Define to 1 if you have the `res_init' function. */
#define HAVE_RES_INIT 1

/* Define to 1 if you have the <runetype.h> header file. */
#define HAVE_RUNETYPE_H 1

/* Define to 1 if you have the <sched.h> header file. */
#define HAVE_SCHED_H 1

/* Define to 1 if you have the `sched_yield' function. */
#define HAVE_SCHED_YIELD 1

/* Define to 1 if you have the <semaphore.h> header file. */
#define HAVE_SEMAPHORE_H 1

/* Define to 1 if you have the `sem_timedwait' function. */
/* #undef HAVE_SEM_TIMEDWAIT */

/* Define to 1 if you have the <setjmp.h> header file. */
#define HAVE_SETJMP_H 1

/* Define to 1 if you have the `settimeofday' function. */
#define HAVE_SETTIMEOFDAY 1

/* Define to 1 if you have the `sigaction' function. */
#define HAVE_SIGACTION 1

/* Can we use SIGIO for tcp and udp IO? */
#define HAVE_SIGNALED_IO 1

/* Define to 1 if you have the `sigset' function. */
#define HAVE_SIGSET 1

/* Define to 1 if you have the `sigvec' function. */
#define HAVE_SIGVEC 1

/* sigwait() available? */
/* #undef HAVE_SIGWAIT */

/* Define to 1 if the system has the type `size_t'. */
#define HAVE_SIZE_T 1

/* Define to 1 if you have the `snprintf' function. */
#define HAVE_SNPRINTF 1

/* Define to 1 if you have the `socket' function. */
#define HAVE_SOCKET 1

/* Define to 1 if you have the `socketpair' function. */
#define HAVE_SOCKETPAIR 1

/* Are Solaris privileges available? */
/* #undef HAVE_SOLARIS_PRIVS */

/* Define to 1 if you have the <stdarg.h> header file. */
#define HAVE_STDARG_H 1

/* Define to 1 if stdbool.h conforms to C99. */
#define HAVE_STDBOOL_H 1

/* Define to 1 if you have the <stddef.h> header file. */
/* #undef HAVE_STDDEF_H */

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `stime' function. */
/* #undef HAVE_STIME */

/* Define to 1 if you have the `strchr' function. */
#define HAVE_STRCHR 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the `strerror_r' function. */
#define HAVE_STRERROR_R 1

/* Define this if strftime() works */
#define HAVE_STRFTIME 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strlcat' function. */
#define HAVE_STRLCAT 1

/* Define to 1 if you have the `strlcpy' function. */
#define HAVE_STRLCPY 1

/* Define to 1 if you have the `strrchr' function. */
#define HAVE_STRRCHR 1

/* Define to 1 if you have the `strsignal' function. */
#define HAVE_STRSIGNAL 1

/* Define to 1 if `decimal_point' is a member of `struct lconv'. */
/* #undef HAVE_STRUCT_LCONV_DECIMAL_POINT */

/* Define to 1 if `thousands_sep' is a member of `struct lconv'. */
/* #undef HAVE_STRUCT_LCONV_THOUSANDS_SEP */

/* Do we have struct ntptimeval? */
/* #undef HAVE_STRUCT_NTPTIMEVAL */

/* Does a system header define struct sockaddr_storage? */
#define HAVE_STRUCT_SOCKADDR_STORAGE 1

/* struct timespec declared? */
#define HAVE_STRUCT_TIMESPEC 1

/* Define to 1 if you have the <synch.h> header file. */
/* #undef HAVE_SYNCH_H */

/* Define to 1 if you have the `sysconf' function. */
#define HAVE_SYSCONF 1

/* Define to 1 if you have the <sysexits.h> header file. */
#define HAVE_SYSEXITS_H 1

/* */
#define HAVE_SYSLOG_FACILITYNAMES 1

/* Define to 1 if you have the <syslog.h> header file. */
#define HAVE_SYSLOG_H 1

/* Define to 1 if you have the <sys/capability.h> header file. */
/* #undef HAVE_SYS_CAPABILITY_H */

/* Define to 1 if you have the <sys/clockctl.h> header file. */
/* #undef HAVE_SYS_CLOCKCTL_H */

/* Define to 1 if you have the <sys/dir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_DIR_H */

/* Define to 1 if you have the <sys/limits.h> header file. */
/* #undef HAVE_SYS_LIMITS_H */

/* Define to 1 if you have the <sys/mman.h> header file. */
#define HAVE_SYS_MMAN_H 1

/* Define to 1 if you have the <sys/ndir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_NDIR_H */

/* Define to 1 if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1

/* Define to 1 if you have the <sys/poll.h> header file. */
#define HAVE_SYS_POLL_H 1

/* Define to 1 if you have the <sys/prctl.h> header file. */
/* #undef HAVE_SYS_PRCTL_H */

/* Define to 1 if you have the <sys/procset.h> header file. */
/* #undef HAVE_SYS_PROCSET_H */

/* Define to 1 if you have the <sys/select.h> header file. */
#define HAVE_SYS_SELECT_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/stropts.h> header file. */
/* #undef HAVE_SYS_STROPTS_H */

/* Define to 1 if you have the <sys/sysctl.h> header file. */
#define HAVE_SYS_SYSCTL_H 1

/* Define to 1 if you have the <sys/timepps.h> header file. */
/* #undef HAVE_SYS_TIMEPPS_H */

/* Define to 1 if you have the <sys/timers.h> header file. */
/* #undef HAVE_SYS_TIMERS_H */

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/un.h> header file. */
#define HAVE_SYS_UN_H 1

/* Define to 1 if you have the <sys/wait.h> header file. */
#define HAVE_SYS_WAIT_H 1

/* Define to 1 if the system has the type `s_char'. */
/* #undef HAVE_S_CHAR */

/* sntp does not care about TTY stuff */
#define HAVE_TERMIOS 1

/* if you have Solaris LWP (thr) package */
/* #undef HAVE_THR */

/* Define to 1 if you have the <thread.h> header file. */
/* #undef HAVE_THREAD_H */

/* Define to 1 if you have the `thr_getconcurrency' function. */
/* #undef HAVE_THR_GETCONCURRENCY */

/* Define to 1 if you have the `thr_setconcurrency' function. */
/* #undef HAVE_THR_SETCONCURRENCY */

/* Define to 1 if you have the `thr_yield' function. */
/* #undef HAVE_THR_YIELD */

/* Define to 1 if you have the `timegm' function. */
#define HAVE_TIMEGM 1

/* Define to 1 if you have the <time.h> header file. */
#define HAVE_TIME_H 1

/* Define to 1 if the system has the type `uint16_t'. */
#define HAVE_UINT16_T 1

/* Define to 1 if the system has the type `uint32_t'. */
#define HAVE_UINT32_T 1

/* Define to 1 if the system has the type `uint8_t'. */
#define HAVE_UINT8_T 1

/* Define to 1 if the system has the type `uintmax_t'. */
/* #undef HAVE_UINTMAX_T */

/* Define to 1 if the system has the type `uintptr_t'. */
#define HAVE_UINTPTR_T 1

/* Define to 1 if the system has the type `uint_t'. */
/* #undef HAVE_UINT_T */

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* deviant sigwait? */
/* #undef HAVE_UNIXWARE_SIGWAIT */

/* Define to 1 if the system has the type `unsigned long long int'. */
#define HAVE_UNSIGNED_LONG_LONG_INT 1

/* Define to 1 if you have the <utime.h> header file. */
#define HAVE_UTIME_H 1

/* Define to 1 if the system has the type `u_int32'. */
/* #undef HAVE_U_INT32 */

/* u_int32 type in DNS headers, not others. */
/* #undef HAVE_U_INT32_ONLY_WITH_DNS */

/* Define to 1 if you have the <values.h> header file. */
/* #undef HAVE_VALUES_H */

/* Define to 1 if you have the <varargs.h> header file. */
/* #undef HAVE_VARARGS_H */

/* Define to 1 if you have the `vfork' function. */
#define HAVE_VFORK 1

/* Define to 1 if you have the <vfork.h> header file. */
/* #undef HAVE_VFORK_H */

/* Define to 1 if you have the `vprintf' function. */
#define HAVE_VPRINTF 1

/* Define if C99-compliant `vsnprintf' is available. */
#define HAVE_VSNPRINTF 1

/* Define to 1 if you have the <wchar.h> header file. */
#define HAVE_WCHAR_H 1

/* Define to 1 if the system has the type `wchar_t'. */
#define HAVE_WCHAR_T 1

/* Define to 1 if the system has the type `wint_t'. */
#define HAVE_WINT_T 1

/* Define to 1 if `fork' works. */
#define HAVE_WORKING_FORK 1

/* Define to 1 if `vfork' works. */
#define HAVE_WORKING_VFORK 1

/* define if select implicitly yields */
#define HAVE_YIELDING_SELECT 1

/* Define to 1 if the system has the type `_Bool'. */
#define HAVE__BOOL 1

/* defined if C compiler supports __attribute__((...)) */
#define HAVE___ATTRIBUTE__ /**/


	/* define away __attribute__() if unsupported */
	#ifndef HAVE___ATTRIBUTE__
	# define __attribute__(x) /* empty */
	#endif
	#define ISC_PLATFORM_NORETURN_PRE
	#define ISC_PLATFORM_NORETURN_POST __attribute__((__noreturn__))
    


/* Define to 1 if you have the `__res_init' function. */
/* #undef HAVE___RES_INIT */

/* Does struct sockaddr_storage have __ss_family? */
/* #undef HAVE___SS_FAMILY_IN_SS */


	    /* Handle sockaddr_storage.__ss_family */
	    #ifdef HAVE___SS_FAMILY_IN_SS
	    # define ss_family __ss_family
	    #endif /* HAVE___SS_FAMILY_IN_SS */
	
    

/* Define to provide `rpl_snprintf' function. */
/* #undef HW_WANT_RPL_SNPRINTF */

/* Define to provide `rpl_vsnprintf' function. */
/* #undef HW_WANT_RPL_VSNPRINTF */

/* Enclose PTHREAD_ONCE_INIT in extra braces? */
/* #undef ISC_PLATFORM_BRACEPTHREADONCEINIT */

/* Do we need to fix in6isaddr? */
/* #undef ISC_PLATFORM_FIXIN6ISADDR */

/* ISC: do we have if_nametoindex()? */
#define ISC_PLATFORM_HAVEIFNAMETOINDEX 1

/* have struct if_laddrconf? */
/* #undef ISC_PLATFORM_HAVEIF_LADDRCONF */

/* have struct if_laddrreq? */
/* #undef ISC_PLATFORM_HAVEIF_LADDRREQ */

/* have struct in6_pktinfo? */
#define ISC_PLATFORM_HAVEIN6PKTINFO 1

/* have IPv6? */
#define ISC_PLATFORM_HAVEIPV6 1

/* struct sockaddr has sa_len? */
#define ISC_PLATFORM_HAVESALEN 1

/* sin6_scope_id? */
#define ISC_PLATFORM_HAVESCOPEID 1

/* missing in6addr_any? */
/* #undef ISC_PLATFORM_NEEDIN6ADDRANY */

/* Do we need netinet6/in6.h? */
/* #undef ISC_PLATFORM_NEEDNETINET6IN6H */

/* ISC: provide inet_ntop() */
/* #undef ISC_PLATFORM_NEEDNTOP */

/* Declare in_port_t? */
/* #undef ISC_PLATFORM_NEEDPORTT */

/* ISC: provide inet_pton() */
/* #undef ISC_PLATFORM_NEEDPTON */

/* enable libisc thread support? */
/* #undef ISC_PLATFORM_USETHREADS */

/* define to 1 if library is thread safe */
#define LDAP_API_FEATURE_X_OPENLDAP_THREAD_SAFE 1

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#define LT_OBJDIR ".libs/"

/* Does the target support multicast IP? */
#define MCAST 1

/* pthread_init() required? */
/* #undef NEED_PTHREAD_INIT */

/* use PTHREAD_SCOPE_SYSTEM? */
/* #undef NEED_PTHREAD_SCOPE_SYSTEM */

/* Do we need an s_char typedef? */
#define NEED_S_CHAR_TYPEDEF 1

/* Define this if optional arguments are disallowed */
/* #undef NO_OPTIONAL_OPT_ARGS */

/* Should we avoid #warning on option name collisions? */
/* #undef NO_OPTION_NAME_WARNINGS */

/* define if you have (or want) no threads */
/* #undef NO_THREADS */

/* Use OpenSSL? */
/* #undef OPENSSL */

/* Name of package */
#define PACKAGE "sntp"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "sntp"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "sntp 4.2.8p10"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "sntp"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "4.2.8p10"

/* define to a working POSIX compliant shell */
#define POSIX_SHELL "/bin/bash"

/* enable thread safety */
#define REENTRANT 1

/* name of regex header file */
#define REGEX_HEADER <regex.h>

/* define if sched_yield yields the entire process */
/* #undef REPLACE_BROKEN_YIELD */

/* The size of `char *', as computed by sizeof. */
#define SIZEOF_CHAR_P 8

/* The size of `int', as computed by sizeof. */
#define SIZEOF_INT 4

/* The size of `long', as computed by sizeof. */
#define SIZEOF_LONG 8

/* The size of `long long', as computed by sizeof. */
#define SIZEOF_LONG_LONG 8

/* The size of `pthread_t', as computed by sizeof. */
/* #undef SIZEOF_PTHREAD_T */

/* The size of `short', as computed by sizeof. */
#define SIZEOF_SHORT 2

/* The size of `signed char', as computed by sizeof. */
#define SIZEOF_SIGNED_CHAR 1

/* The size of `time_t', as computed by sizeof. */
#define SIZEOF_TIME_T 8

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at runtime.
	STACK_DIRECTION > 0 => grows toward higher addresses
	STACK_DIRECTION < 0 => grows toward lower addresses
	STACK_DIRECTION = 0 => direction of growth unknown */
/* #undef STACK_DIRECTION */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if strerror_r returns char *. */
/* #undef STRERROR_R_CHAR_P */

/* canonical system (cpu-vendor-os) of where we should run */
#define STR_SYSTEM ""

/* Does Xettimeofday take 1 arg? */
/* #undef SYSV_TIMEOFDAY */

/* enable thread safety */
#define THREADSAFE 1

/* enable thread safety */
#define THREAD_SAFE 1

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Provide a typedef for uintptr_t? */
#ifndef HAVE_UINTPTR_T
typedef unsigned int	uintptr_t;
#define HAVE_UINTPTR_T 1
#endif

/* What type to use for setsockopt */
#define TYPEOF_IP_MULTICAST_LOOP u_char

/* OK to use snprintb()? */
/* #undef USE_SNPRINTB */

/* Can we use SIGPOLL for tty IO? */
/* #undef USE_TTY_SIGPOLL */

/* Can we use SIGPOLL for UDP? */
/* #undef USE_UDP_SIGPOLL */

/* Version number of package */
#define VERSION "4.2.8p10"

/* vsnprintf expands "%m" to strerror(errno) */
/* #undef VSNPRINTF_PERCENT_M */

/* configure --enable-ipv6 */
#define WANT_IPV6 1

/* Define this if a working libregex can be found */
#define WITH_LIBREGEX 1

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif

/* Define to 1 if on MINIX. */
/* #undef _MINIX */

/* Define to 2 if the system does not provide POSIX.1 features except with
   this defined. */
/* #undef _POSIX_1_SOURCE */

/* Define to 1 if you need to in order for `stat' and other things to work. */
/* #undef _POSIX_SOURCE */

/* enable thread safety */
#define _REENTRANT 1

/* enable thread safety */
#define _SGI_MP_SOURCE 1

/* enable thread safety */
#define _THREADSAFE 1

/* enable thread safety */
#define _THREAD_SAFE 1

/* Define to 500 only on HP-UX. */
/* #undef _XOPEN_SOURCE */

/* Are we _special_? */
#define __APPLE_USE_RFC_3542 1

/* Define to 1 if type `char' is unsigned and you are not using gcc.  */
#ifndef __CHAR_UNSIGNED__
/* # undef __CHAR_UNSIGNED__ */
#endif

/* Enable extensions on AIX 3, Interix.  */
#ifndef _ALL_SOURCE
# define _ALL_SOURCE 1
#endif
/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif
/* Enable threading extensions on Solaris.  */
#ifndef _POSIX_PTHREAD_SEMANTICS
# define _POSIX_PTHREAD_SEMANTICS 1
#endif
/* Enable extensions on HP NonStop.  */
#ifndef _TANDEM_SOURCE
# define _TANDEM_SOURCE 1
#endif
/* Enable general extensions on Solaris.  */
#ifndef __EXTENSIONS__
# define __EXTENSIONS__ 1
#endif


/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `int' if <sys/types.h> doesn't define. */
/* #undef gid_t */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif

/* Define to the widest signed integer type if <stdint.h> and <inttypes.h> do
   not define. */
/* #undef intmax_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef pid_t */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

	
	    #if !defined(_KERNEL) && !defined(PARSESTREAM)
	    /*
	     * stdio.h must be included after _GNU_SOURCE is defined
	     * but before #define snprintf rpl_snprintf
	     */
	    # include <stdio.h>	
	    #endif
	

/* Define to rpl_snprintf if the replacement function should be used. */
/* #undef snprintf */

/* Define to `int' if <sys/types.h> doesn't define. */
/* #undef uid_t */

/* Define to the widest unsigned integer type if <stdint.h> and <inttypes.h>
   do not define. */
/* #undef uintmax_t */

/* Define to the type of an unsigned integer type wide enough to hold a
   pointer, if such a type exists, and if the system does not define it. */
/* #undef uintptr_t */

/* Define as `fork' if `vfork' does not work. */
/* #undef vfork */

/* Define to empty if the keyword `volatile' does not work. Warning: valid
   code using `volatile' can become incorrect without. Disable with care. */
/* #undef volatile */

/* Define to rpl_vsnprintf if the replacement function should be used. */
/* #undef vsnprintf */


#ifndef MPINFOU_PREDECLARED
# define MPINFOU_PREDECLARED
typedef union mpinfou {
	struct pdk_mpinfo *pdkptr;
	struct mpinfo *pikptr;
} mpinfou_t;
#endif



	#if !defined(_KERNEL) && !defined(PARSESTREAM)
	# if defined(HW_WANT_RPL_VSNPRINTF)
	#  if defined(__cplusplus)
	extern "C" {
	# endif
	# include <stdarg.h>
	int rpl_vsnprintf(char *, size_t, const char *, va_list);
	# if defined(__cplusplus)
	}
	#  endif
	# endif
	# if defined(HW_WANT_RPL_SNPRINTF)
	#  if defined(__cplusplus)
	extern "C" {
	#  endif
	int rpl_snprintf(char *, size_t, const char *, ...);
	#  if defined(__cplusplus)
	}
	#  endif
	# endif
	#endif	/* !defined(_KERNEL) && !defined(PARSESTREAM) */
	
