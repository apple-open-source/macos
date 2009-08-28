# $NetBSD: configure.ac,v 1.18 2008/09/29 00:55:57 lukem Exp $
#
# Process this file with autoconf to produce a configure script.

AC_INIT([tnftpd], [20080929], [lukem@NetBSD.org])
AC_PREREQ([2.61])
AC_REVISION([$Revision: 1.18 $])

AS_SHELL_SANITIZE

AC_CONFIG_SRCDIR([tnftpd.h])
AC_CONFIG_AUX_DIR(build-aux)
AC_CONFIG_HEADERS([config.h])

#
# Arguments for which features are included.
#
AC_CANONICAL_TARGET
AC_ARG_PROGRAM
AC_ARG_ENABLE([ipv6],
              [AS_HELP_STRING([--enable-ipv6],
                              [enable IPv6 support (if your OS supports it)
                               [default=enabled]])],
              [opt_ipv6=$enableval],
              [opt_ipv6=yes])
AC_ARG_ENABLE([builtinls],
              [AS_HELP_STRING([--enable-builtinls],
                              [enable built-in /bin/ls
                               [default=enabled]])],
              [opt_builtinls=$enableval],
              [opt_builtinls=yes])
AC_ARG_WITH([pam],
            [AS_HELP_STRING([--with-pam],
                            [enable support for Pluggable Authentication
                             Modules (PAM) authentication
                             [default=auto]])],
            [],
            [with_pam=auto])
AC_ARG_WITH([skey],
            [AS_HELP_STRING([--with-skey],
                            [enable support for S/Key authentication
                             (not compatible with --with-pam)
                             [default=no]])],
            [],
            [with_skey=no])

#
# Autoheader templates symbols.
#
AH_TEMPLATE([HAVE_DIRFD_AS_MACRO],
            [Define if dirfd() is a macro.])
AH_TEMPLATE([HAVE_POLL],
            [Define if we have poll() and it is not emulated.])
AH_TEMPLATE([HAVE_PRINTF_QD],
            [Define if *printf() uses %qd to print `long long'
             (otherwise uses %lld).])
AH_TEMPLATE([HAVE_PRINTF_LONG_LONG],
            [Define if `long long' is supported and
             *printf() supports %lld or %qd to print them.])
AH_TEMPLATE([NO_INTERNAL_LS],
            [Define if not using in-built /bin/ls code.])
AH_TEMPLATE([USE_INET6],
            [Define if using IPv6 support.])
AH_TEMPLATE([USE_PAM],
            [Define if using PAM authentication.])
AH_TEMPLATE([USE_SKEY],
            [Define if using S/Key authentication.])

#
# Checks for programs.
#
AC_PROG_MAKE_SET
AC_PROG_CC
AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_MKDIR_P
AC_PROG_YACC
AC_CHECK_TOOL([AR], [ar], [:])
AC_CHECK_TOOL([RANLIB], [ranlib], [:])

#
# Checks for tool features.
#
AS_CASE([$target_os],
        [darwin*],
        [# Mac OS X linker needs -search_paths_first so our private libraries
         # are used before system libraries.
         #
         old_LDFLAGS=$LDFLAGS
         LDFLAGS="$LDFLAGS -Wl,-search_paths_first"
         AC_MSG_CHECKING([whether ld accepts -search_paths_first])
         AC_LINK_IFELSE([AC_LANG_PROGRAM([], [[int i;]])],
                        [AC_MSG_RESULT([yes])],
                        [AC_MSG_RESULT([no])
                         LDFLAGS=$old_LDFLAGS])],
        )
AC_PATH_PROG([PATH_YACC], [$YACC], [])
AS_IF([test "$PATH_YACC" = ""],
      [AC_MSG_FAILURE([can't find yacc parser generator "$YACC"])])

#
# Checks for libraries.
#
AC_SEARCH_LIBS([crypt], [crypt])
AC_SEARCH_LIBS([pidfile], [util])
AC_SEARCH_LIBS([setproctitle], [util])

AC_SEARCH_LIBS([gethostbyname], [nsl])
AC_SEARCH_LIBS([socket],
               [socket],
               [],
               [AC_CHECK_LIB([socket],
                             [socket],
                             [LIBS="-lsocket -lnsl $LIBS"],
                             [],
                             [-lnsl])])

#
# Checks for header files.
#
accheck_includes='
#include <stdio.h>
#if defined(HAVE_SYS_TYPES_H)
# include <sys/types.h>
#endif
#if defined(HAVE_SYS_STAT_H)
# include <sys/stat.h>
#endif
#if defined(STDC_HEADERS)
# include <stdarg.h>
# include <stdlib.h>
# include <string.h>
#endif
#if defined(HAVE_PWD_H)
# include <pwd.h>
#endif
#if defined(HAVE_FNMATCH_H)
# include <fnmatch.h>
#endif
#if defined(HAVE_UNISTD_H)
# include <unistd.h>
#endif
#if defined(HAVE_POLL_H)
# include <poll.h>
#elif defined(HAVE_SYS_POLL_H)
# include <sys/poll.h>
#endif
#if defined(HAVE_SYS_SOCKET_H)
# include <sys/socket.h>
#endif
#if defined(HAVE_NETINET_IN_H)
# include <netinet/in.h>
#endif
#if defined(HAVE_NETINET_IN_SYSTM_H)
# include <netinet/in_systm.h>
#endif
#if defined(HAVE_NETDB_H)
# include <netdb.h>
#endif
#if defined(HAVE_ARPA_INET_H)
# include <arpa/inet.h>
#endif
#if defined(HAVE_DIRENT_H)
# include <dirent.h>
#else
# define dirent direct
# if defined(HAVE_SYS_NDIR_H)
#  include <sys/ndir.h>
# endif
# if defined(HAVE_SYS_DIR_H)
#  include <sys/dir.h>
# endif
# if defined(HAVE_NDIR_H)
#  include <ndir.h>
# endif
#endif
' # accheck_includes

AC_CHECK_HEADERS([sys/types.h sys/ioctl.h sys/mman.h sys/param.h \
                  sys/stat.h sys/socket.h sys/syslimits.h \
                  sys/sysmacros.h sys/time.h sys/wait.h],
                 [], [], [$accheck_includes])
AC_HEADER_DIRENT
AC_HEADER_RESOLV
AC_HEADER_STAT
AC_HEADER_STDC
AC_HEADER_TIME
AC_HEADER_TIOCGWINSZ
AC_CHECK_HEADERS([sys/resource.h \
                  arpa/inet.h arpa/nameser.h arpa/telnet.h err.h \
                  fcntl.h fnmatch.h grp.h libutil.h limits.h locale.h \
                  netinet/in.h netinet/in_systm.h netinet/ip.h \
                  paths.h poll.h pwd.h sys/poll.h \
                  setjmp.h signal.h stddef.h syslog.h termios.h \
                  unistd.h util.h utime.h utmp.h],
                  [], [], [$accheck_includes])

#
# Checks for typedefs, structures, and compiler characteristics.
#
AC_CHECK_DECLS([AI_NUMERICHOST, FNM_CASEFOLD,
                crypt, fclose, h_errno, pclose, optarg, optind, optreset],
               [], [], [$accheck_includes])
AC_TYPE_LONG_DOUBLE
AC_TYPE_LONG_LONG_INT
AC_TYPE_UINT32_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIGNAL
AC_TYPE_SIZE_T
AC_STRUCT_TM
AC_CHECK_MEMBERS([struct sockaddr.sa_len, struct sockaddr_in.sin_len,
                  DIR.dd_fd, DIR.__dd_fd, struct dirent.d_namlen,
                  struct passwd.pw_change, struct passwd.pw_expire],
                 [], [], [$accheck_includes])
AC_CHECK_TYPES([in_port_t, sa_family_t, socklen_t, struct addrinfo],
               [], [], [$accheck_includes])
AC_SYS_LARGEFILE

# If IPv6 is enabled, check for necessary items.
#
AS_IF([test "$opt_ipv6" = yes],
      [AC_CHECK_DECLS([AF_INET6, NS_IN6ADDRSZ], [], [opt_ipv6=no],
[$accheck_includes
#if defined(HAVE_ARPA_NAMESER_H)
#include <arpa/nameser.h>
#endif
])
       AC_CHECK_TYPES([struct sockaddr_in6], [], [opt_ipv6=no],
                      [$accheck_includes])
       AC_MSG_CHECKING([required functionality for IPv6])
       AS_IF([test "$opt_ipv6" = yes],
             [AC_MSG_RESULT([yes])
              AC_DEFINE([USE_INET6], [1])],
             [AC_MSG_RESULT([no])])
       ])

#
# Checks for library functions.
#
AC_FUNC_CLOSEDIR_VOID
AC_FUNC_FORK
AC_FUNC_FSEEKO
AC_FUNC_STRCOLL
AC_REPLACE_FUNCS([daemon err fgetln fparseln \
                  getaddrinfo getgrouplist getnameinfo getusershell \
                  inet_net_pton inet_ntop inet_pton mkstemp \
                  sl_init snprintf strdup strerror strsuftollx \
                  strlcat strlcpy strsep usleep])
AC_CHECK_FUNCS([dirfd getcwd gethostbyaddr gethostbyname gethostbyname2 \
                gethostname getrusage getspnam gettimeofday \
                isascii madvise memmove memset mmap pidfile realpath \
                select setlogin setproctitle setegid seteuid \
                setresgid setresuid strcasecmp strtol strtoul vfork])
AS_IF([test "$ac_cv_func_dirfd" = no],
      [AC_MSG_CHECKING([whether dirfd() is a macro])
       AC_EGREP_CPP([have_dirfd_as_macro], [
$accheck_includes
#if defined(dirfd)
have_dirfd_as_macro
#endif],
                    [AC_MSG_RESULT([yes])
                     AC_DEFINE([HAVE_DIRFD_AS_MACRO], [1])],
                    [AC_MSG_RESULT([no])])])

# Always replace glob(); the vendor's may not be secure.
#
AC_LIBOBJ([glob])

# Replace fnmatch() if FNM_CASEFOLD isn't supported.
#
AS_IF([test "$ac_cv_have_decl_FNM_CASEFOLD" != yes], [AC_LIBOBJ([fnmatch])])

# If getusershell() is available, check for declaration.
#
AS_IF([test "$ac_cv_func_getusershell" = yes], [AC_CHECK_DECLS([getusershell])])

# We assume that if sprintf() supports %lld or %qd,
# then all of *printf() does. If not, disable long long
# support because we don't know how to display it.
#
AS_IF([test "$ac_cv_type_long_long_int" = yes],
      [AC_MSG_CHECKING([*printf() support for %lld])
       can_printf_longlong=no
       AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <stdio.h>
int main() {
        char buf[100];
        sprintf(buf, "%lld", 4294967300LL);
        return (strcmp(buf, "4294967300"));
}
]])],
                     [AC_MSG_RESULT([yes])
                      can_printf_longlong=yes],
                     [AC_MSG_RESULT([no])],
                     [AC_MSG_RESULT([unknown - cross-compiling])])
        AS_IF([test "$can_printf_longlong" != yes],
              [AC_MSG_CHECKING([*printf() support for %qd])
               AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <stdio.h>
int main() {
        char buf[100];
        sprintf(buf, "%qd", 4294967300LL);
        return (strcmp(buf, "4294967300"));
}
]])],
                             [AC_MSG_RESULT([yes])
                              can_printf_longlong=yes
                              AC_DEFINE([HAVE_PRINTF_QD], [1])],
                             [AC_MSG_RESULT([no])],
                             [AC_MSG_RESULT([unknown - cross-compiling])])])
        AC_MSG_CHECKING([whether *printf() can print long long ints])
        AS_IF([test "$can_printf_longlong" = yes],
              [AC_MSG_RESULT([yes])
               AC_DEFINE([HAVE_PRINTF_LONG_LONG], [1])
               AC_REPLACE_FUNCS([strtoll])],
              [AC_MSG_RESULT([no])])])

# Handle Darwin 7 having a poll() compatibility function,
# and don't use it if it's emulated.
# Be conservative, if we don't find one of poll.h or sys/poll.h,
# don't attempt to use poll().
#
AC_CHECK_TYPES([struct pollfd], [], [], [$accheck_includes])
AC_CHECK_FUNC([poll],
              [AC_CHECK_DECL([_POLL_EMUL_H_],
                             [],
                             [AC_DEFINE([HAVE_POLL], [1])],
                             [$accheck_includes])])

# Replace sl_init() (et al) if it provides the older API.
#
AS_IF([test "$ac_cv_func_sl_init" = yes],
      [AC_MSG_CHECKING([whether sl_add() returns int])
       AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <stringlist.h>]],
                                          [[int f = sl_add((StringList *)0,
                                                           "foo");]])],
                         [AC_MSG_RESULT([yes])],
                         [AC_MSG_RESULT([no - using local version])
                          AC_LIBOBJ([sl_init])],
                         [AC_MSG_RESULT([unknown - cross-compiling])])])

# Replace getaddrinfo() if missing or AI_NUMERICHOST isn't declared.
#
AC_MSG_CHECKING([whether getaddrinfo() is usuable])
AS_IF([test "$ac_cv_have_decl_AI_NUMERICHOST" = yes],
      [AC_MSG_RESULT([yes])],
      [AS_IF([test "$ac_cv_func_getaddrinfo" = yes],
             [AC_LIBOBJ([getaddrinfo])
              AC_MSG_RESULT([no - using local version])],
             [AC_MSG_RESULT([using local version])])])

# Check for PAM.
#
AS_IF([test "$with_pam" != no],
      [AC_CHECK_LIB([pam],
                    [pam_start],
                    [LIBS="-lpam $LIBS"
                     AC_CHECK_HEADERS([security/pam_appl.h],
                                      [],
                                      [AC_CHECK_HEADERS([pam/pam_appl.h],
                                                        [],
                                                        [AC_MSG_FAILURE(
                [--with-pam was given, but pam_appl.h wasn't found])])])
                     AC_DEFINE([USE_PAM], [1])
                     with_pam=yes],
                    [AS_IF([test "$with_pam" != "auto"],
                           [AC_MSG_FAILURE(
                [--with-pam was given, but pam_start() wasn't found])])
                     with_pam=no])])

# Check for S/Key.
#
AS_IF([test "$with_skey" != no],
      AS_IF([test "$with_pam" != no],
            [AC_MSG_ERROR([--with-skey is not compatible with --with-pam])])
      [AC_CHECK_LIB([skey],
                    [skey_haskey],
                    [LIBS="-lskey $LIBS"
                     AC_DEFINE([USE_SKEY], [1])
                     with_skey=yes,
                     AC_CHECK_HEADERS([skey.h])],
                    [AS_IF([test "$with_skey" != auto],
                           [AC_MSG_FAILURE(
                [--with-skey was given, but skey_haskey() wasn't found])])
                     with_skey=no])])

# Tests for items required for inbuilt ls.
#
AS_IF([test "$opt_builtinls" = yes],
      [AC_CHECK_HEADERS([fts.h])
       AC_REPLACE_FUNCS([fts_open strmode user_from_uid])
       AC_MSG_CHECKING([whether fts.h and fts_open() are usable])
       AS_IF([test "$ac_cv_header_fts_h" = yes],
             [AC_MSG_RESULT([yes])],
             [AS_IF([test "$ac_cv_func_fts_open" = yes],
                    [AC_LIBOBJ([fts_open])
                     AC_MSG_RESULT([no - using local version])],
                    [AC_MSG_RESULT([using local version])])])
       LSOBJS="cmp.o ls.o print.o util.o"],
      [AC_DEFINE([NO_INTERNAL_LS], [1])])

# Build libnetbsd if necessary.
#
AS_IF([test -n "$LIBOBJS"],
      [INCLUDES="$INCLUDES -I\${srcdir}/../libnetbsd"
       LDFLAGS="$LDFLAGS -L../libnetbsd"
       LIBS="$LIBS -lnetbsd"
       LIBNETBSD=libnetbsd.a
       LIBDEPENDS="$LIBDEPENDS ../libnetbsd/libnetbsd.a"])

# Display feature results.
#
AC_MSG_NOTICE([ === Configuration results ===])
AC_MSG_NOTICE([Package:               $PACKAGE_STRING])
AC_MSG_NOTICE([IPv6 support:          $opt_ipv6])
AC_MSG_NOTICE([Built-in /bin/ls:      $opt_builtinls])
AC_MSG_NOTICE([PAM authentication:    $with_pam])
AC_MSG_NOTICE([S/Key authentication:  $with_skey])
AC_MSG_NOTICE([ =============================])

#
# Create the Makefiles.
#
AC_SUBST(INCLUDES)
AC_SUBST(LIBNETBSD)
AC_SUBST(LIBDEPENDS)
AC_SUBST(LSOBJS)

AC_CONFIG_FILES([
        Makefile
        libnetbsd/Makefile
        src/Makefile
])

AC_OUTPUT
