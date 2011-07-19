/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Directory separator character, usually / or \\ */
#define DIR_SEP '/'

/* fopen(3) accepts a 'b' in the mode flag */
#define FOPEN_BINARY_FLAG "b"

/* fopen(3) accepts a 't' in the mode flag */
#define FOPEN_TEXT_FLAG "t"

/* What is getsockname()'s socklen type? */
#define GETSOCKNAME_SOCKLEN_TYPE socklen_t

/* Define to 1 if you have the `atexit' function. */
#define HAVE_ATEXIT 1

/* Define to 1 if you have the `canonicalize_file_name' function. */
/* #undef HAVE_CANONICALIZE_FILE_NAME */

/* Define this if /dev/zero is readable device */
#define HAVE_DEV_ZERO 1

/* Define to 1 if you have the <dirent.h> header file, and it defines `DIR'.
   */
#define HAVE_DIRENT_H 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you don't have `vprintf' but do have `_doprnt.' */
/* #undef HAVE_DOPRNT */

/* Define to 1 if you have the <errno.h> header file. */
#define HAVE_ERRNO_H 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the `fork' function. */
#define HAVE_FORK 1

/* inline keyword or macro available */
#define HAVE_INLINE 1

/* Define to 1 if the system has the type `int16_t'. */
#define HAVE_INT16_T 1

/* Define to 1 if the system has the type `int32_t'. */
#define HAVE_INT32_T 1

/* Define to 1 if the system has the type `int8_t'. */
#define HAVE_INT8_T 1

/* Define to 1 if the system has the type `intptr_t'. */
#define HAVE_INTPTR_T 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `gen' library (-lgen). */
/* #undef HAVE_LIBGEN */

/* Define to 1 if you have the <libgen.h> header file. */
#define HAVE_LIBGEN_H 1

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `memset' function. */
#define HAVE_MEMSET 1

/* Define to 1 if you have the `mmap' function. */
#define HAVE_MMAP 1

/* Define to 1 if you have the <ndir.h> header file, and it defines `DIR'. */
/* #undef HAVE_NDIR_H */

/* Define to 1 if you have the <netdb.h> header file. */
#define HAVE_NETDB_H 1

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* sntp does not care about 'nice' */
#define HAVE_NO_NICE 1

/* Define this if pathfind(3) works */
/* #undef HAVE_PATHFIND */

/* Define to 1 if the system has the type `pid_t'. */
#define HAVE_PID_T 1

/* Define this if we have a functional realpath(3C) */
#define HAVE_REALPATH 1

/* Define to 1 if you have the <runetype.h> header file. */
#define HAVE_RUNETYPE_H 1

/* Define to 1 if you have the <setjmp.h> header file. */
#define HAVE_SETJMP_H 1

/* Define to 1 if the system has the type `size_t'. */
#define HAVE_SIZE_T 1

/* Define to 1 if you have the `snprintf' function. */
#define HAVE_SNPRINTF 1

/* Define to 1 if you have the `socket' function. */
#define HAVE_SOCKET 1

/* Define to 1 if you have the <stdarg.h> header file. */
#define HAVE_STDARG_H 1

/* Define to 1 if stdbool.h conforms to C99. */
#define HAVE_STDBOOL_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strchr' function. */
#define HAVE_STRCHR 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define this if strftime() works */
#define HAVE_STRFTIME 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strrchr' function. */
#define HAVE_STRRCHR 1

/* Define to 1 if you have the `strsignal' function. */
#define HAVE_STRSIGNAL 1

/* Does a system header define struct sockaddr_storage? */
#define HAVE_STRUCT_SOCKADDR_STORAGE 1

/* Define to 1 if you have the <sysexits.h> header file. */
#define HAVE_SYSEXITS_H 1

/* Define to 1 if you have the <syslog.h> header file. */
#define HAVE_SYSLOG_H 1

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

/* Define to 1 if the system has the type `uint16_t'. */
#define HAVE_UINT16_T 1

/* Define to 1 if the system has the type `uint32_t'. */
#define HAVE_UINT32_T 1

/* Define to 1 if the system has the type `uint8_t'. */
#define HAVE_UINT8_T 1

/* Define to 1 if the system has the type `uintptr_t'. */
#define HAVE_UINTPTR_T 1

/* Define to 1 if the system has the type `uint_t'. */
/* #undef HAVE_UINT_T */

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the <utime.h> header file. */
#define HAVE_UTIME_H 1

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

/* Define to 1 if the system has the type `_Bool'. */
#define HAVE__BOOL 1

/* Does struct sockaddr_storage have __ss_family? */
/* #undef HAVE___SS_FAMILY_IN_SS */


	/* Handle sockaddr_storage.__ss_family */
	#ifdef HAVE___SS_FAMILY_IN_SS
	# define ss_family __ss_family
	#endif /* HAVE___SS_FAMILY_IN_SS */
    


/* Does struct sockaddr_storage have __ss_len? */
/* #undef HAVE___SS_LEN_IN_SS */


	/* Handle sockaddr_storage.__ss_len */
	#ifdef HAVE___SS_LEN_IN_SS
	# define ss_len __ss_len
	#endif /* HAVE___SS_LEN_IN_SS */
    


/* Do we need to fix in6isaddr? */
/* #undef ISC_PLATFORM_FIXIN6ISADDR */

/* have struct if_laddrconf? */
/* #undef ISC_PLATFORM_HAVEIF_LADDRCONF */

/* have struct if_laddrreq? */
/* #undef ISC_PLATFORM_HAVEIF_LADDRREQ */

/* have struct in6_pktinfo? */
#define ISC_PLATFORM_HAVEIN6PKTINFO /**/

/* have IPv6? */
#define ISC_PLATFORM_HAVEIPV6 /**/

/* have sin6_scope_id? */
#define ISC_PLATFORM_HAVESCOPEID /**/

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

/* Do we need netinet6/in6.h? */
/* #undef LWRES_PLATFORM_NEEDNETINET6IN6H */

/* Do we need an s_char typedef? */
#define NEED_S_CHAR_TYPEDEF 1

/* Define this if optional arguments are disallowed */
/* #undef NO_OPTIONAL_OPT_ARGS */

/* Name of package */
#define PACKAGE "sntp"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "sntp"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "sntp 4.2.6"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "sntp"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "4.2.6"

/* name of regex header file */
#define REGEX_HEADER <regex.h>

/* The size of `char*', as computed by sizeof. */
#define SIZEOF_CHARP 8

/* The size of `int', as computed by sizeof. */
#define SIZEOF_INT 4

/* The size of `long', as computed by sizeof. */
#define SIZEOF_LONG 8

/* The size of `short', as computed by sizeof. */
#define SIZEOF_SHORT 2

/* The size of `signed char', as computed by sizeof. */
#define SIZEOF_SIGNED_CHAR 1

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* canonical system (cpu-vendor-os) of where we should run */
#define STR_SYSTEM "i686-apple-darwin10.0"

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Version number of package */
#define VERSION "4.2.6"

/* ISC: Want IPv6? */
#define WANT_IPV6 /**/

/* Define this if a working libregex can be found */
#define WITH_LIBREGEX 1

/* Define to 1 if type `char' is unsigned and you are not using gcc.  */
#ifndef __CHAR_UNSIGNED__
/* # undef __CHAR_UNSIGNED__ */
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

/* Define to `int' if <sys/types.h> does not define. */
/* #undef pid_t */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define to `int' if <sys/types.h> doesn't define. */
/* #undef uid_t */

/* Define as `fork' if `vfork' does not work. */
/* #undef vfork */
