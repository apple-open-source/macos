dnl Process this file with autoconf to produce a configure script

#		"We are ugly but we have the music" 
#			-- Leonard Cohen

# http://christpuncher.jerkcity.com/jerkcity1335.html


# This is the configure script for distcc.
# Copyright (C) 2002, 2003 by Martin Pool

# As of 0.6cvs, distcc no longer uses automake, only autoconf.
AC_PREREQ(2.53)
AC_INIT(distcc, 2.0.1, [distcc@lists.samba.org])
AC_CONFIG_HEADERS(src/config.h)
AC_CANONICAL_HOST

# FreeBSD installs its version of libpopt into /usr/local/, but does
# not put that on the default library and header path.
# Solaris doesn't even ship libpopt.  We used to add that path if
# building on *bsd*, but bje points out that will break
# cross-compilation, and it's kind of ugly anyhow.  Anyhow, you can
# either set CPPFLAGS and LDFLAGS when running configure, or use
# --with-extra-foo.

##### defaults

# TODO: Add --docdir, and use that instead of this.  It looks like
# that's actually pretty hard to do in autoconf; it looks like the
# directory names are pretty much ardcoded.

# These are left unexpanded so users can "make install exec_prefix=/foo"
# and all the variables that are supposed to be based on exec_prefix
# by default will actually change.
# Use braces instead of parens because sh, perl, etc. also accept them.
docdir='${datadir}/doc'

# This turns into something like "/usr/local/share/doc"; pkgdocdir is
# "/usr/local/share/doc/distcc"

# TODO: Handle program transform rules by autoconf.

### Checks for configure options

AC_ARG_WITH(included-popt,
	AC_HELP_STRING([--with-included-popt], [use bundled popt library, not from system]))

dnl Checks for programs
AC_PROG_CC
if test x"$GCC" = xyes 
then
    CFLAGS="$CFLAGS -W -Wall -W -Wimplicit \
-Wshadow -Wpointer-arith -Wcast-align -Wwrite-strings \
-Waggregate-return -Wstrict-prototypes -Wmissing-prototypes \
-Wnested-externs"
    AC_MSG_NOTICE([Adding gcc options: $CFLAGS])
fi
AC_ISC_POSIX

# This is needed for our included version of popt.  Kind of silly, but
# I don't want our version too far out of sync.
CFLAGS="$CFLAGS"

CPPFLAGS="$CPPFLAGS -DHAVE_CONFIG_H -D_GNU_SOURCE"

AC_PROG_MAKE_SET
AC_PROG_INSTALL

# TODO: Perhaps try to find a command name that invokes Python2.2, and
# write that in to the makefile for use in running benchmarks, etc.
# Perhaps it's reasonable to count on it always being "python2.2", but
# that will break when we're on to a later version.

AC_CHECK_PROGS(PYTHON, [python2.2 python-2.2 python2.3 python-2.3 python])
AC_ARG_VAR(PYTHON, [Python interpreter to use for running tests])
# NB: Cannot use AC_CONFIG_LIBOBJ_DIR here, because it's not present
# in autoconf 2.53.

AC_C_INLINE

########################################################################
### Checks for header files

# Some of these are needed by popt (or other libraries included in the future).

AC_CHECK_HEADERS([unistd.h stdint.h sys/types.h sys/sendfile.h sys/signal.h])
AC_CHECK_HEADERS([ctype.h sys/resource.h sys/socket.h])
AC_CHECK_HEADERS([netinet/in.h], [], [],
[#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
])
AC_CHECK_HEADERS([arpa/nameser.h], [], [],
[#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
])
AC_CHECK_HEADERS([resolv.h], [], [],
[#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#if HAVE_ARPA_NAMESER_H
# include <arpa/nameser.h>
#endif
])

AC_CHECK_HEADERS([float.h mcheck.h alloca.h])

######################################################################
dnl Checks for types

AC_CHECK_TYPES([sa_family_t, socklen_t, in_port_t, in_addr_t], , ,
	       [
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#if HAVE_ARPA_NAMESER_H
# include <arpa/nameser.h>
#endif
])



########################################################################
### Checks for libraries.

# The following test taken from the cvs sources via Samba:
# If we can't find connect, try looking in -lsocket, -lnsl, and -linet.
# The Irix 5 libc.so has connect and gethostbyname, but Irix 5 also has
# libsocket.so which has a bad implementation of gethostbyname (it
# only looks in /etc/hosts), so we only look for -lsocket if we need
# it.
AC_CHECK_FUNCS(connect)
if test x"$ac_cv_func_connect" = x"no"; then
    case "$LIBS" in
    *-lnsl*) ;;
    *) AC_CHECK_LIB(nsl_s, printf) ;;
    esac
    case "$LIBS" in
    *-lnsl*) ;;
    *) AC_CHECK_LIB(nsl, printf) ;;
    esac
    case "$LIBS" in
    *-lsocket*) ;;
    *) AC_CHECK_LIB(socket, connect) ;;
    esac
    case "$LIBS" in
    *-linet*) ;;
    *) AC_CHECK_LIB(inet, connect) ;;
    esac
    dnl We can't just call AC_CHECK_FUNCS(connect) here, because the value
    dnl has been cached.
    if test x"$ac_cv_lib_socket_connect" = x"yes" || 
       test x"$ac_cv_lib_inet_connect" = x"yes"; then
        # ac_cv_func_connect=yes
        # don't!  it would cause AC_CHECK_FUNC to succeed next time configure is run
        AC_DEFINE(HAVE_CONNECT,1,[Whether the system has connect()])
    fi
fi

AC_CHECK_LIB(resolv, hstrerror, , , [-lnsl -lsocket])
AC_CHECK_LIB(resolv, inet_aton, , , [-lnsl -lsocket])

if test x"$with_included_popt" != x"yes"  && test x"$with_included_popt" != xno
then
    # If not explicitly requested, guess
    AC_CHECK_LIB(popt, poptGetContext, , [with_included_popt=yes])
    AC_CHECK_LIB(popt, poptStrerror, , [with_included_popt=yes])
fi
AC_CACHE_SAVE

AC_MSG_CHECKING([whether to use included libpopt])
if test x"$with_included_popt" = x"yes"
then
    AC_MSG_RESULT($srcdir/popt)
    BUILD_POPT='$(popt_OBJS)'
    CPPFLAGS="$CPPFLAGS -I$srcdir/popt"
else
    AC_MSG_RESULT(no)
fi



########################################################################
# Checks for library functions, using libraries discovered above
CPPFLAGS="$CPPFLAGS -I$srcdir/src"

AC_CHECK_FUNCS([sendfile setsid flock lockf hstrerror strerror setuid setreuid])
AC_CHECK_FUNCS([getuid geteuid mcheck wait4 wait3 waitpid setgroups getcwd])
AC_CHECK_FUNCS([snprintf vsnprintf vasprintf asprintf getcwd getwd])
AC_CHECK_FUNCS([getrusage strsignal])

AC_CHECK_DECLS([snprintf, vsnprintf, vasprintf, asprintf])


dnl
dnl Test if the preprocessor understand vararg macros
dnl
AC_MSG_CHECKING([for vararg macro support])
AC_TRY_COMPILE([#define func(a, b...) do { } while (0)],
[func("a", "b", "c")],
[AC_MSG_RESULT(yes)
 AC_DEFINE(HAVE_VARARG_MACROS, , [Define if your cpp has vararg macros])],
[AC_MSG_RESULT(no)])


AC_CACHE_CHECK([for C99 vsnprintf],rsync_cv_HAVE_C99_VSNPRINTF,[
AC_TRY_RUN([
#include <sys/types.h>
#include <stdarg.h>
void foo(const char *format, ...) { 
       va_list ap;
       int len;
       char buf[5];

       va_start(ap, format);
       len = vsnprintf(0, 0, format, ap);
       va_end(ap);
       if (len != 5) exit(1);

       if (snprintf(buf, 3, "hello") != 5 || strcmp(buf, "he") != 0) exit(1);

       exit(0);
}
main() { foo("hello"); }
],
rsync_cv_HAVE_C99_VSNPRINTF=yes,rsync_cv_HAVE_C99_VSNPRINTF=no,rsync_cv_HAVE_C99_VSNPRINTF=cross)])
if test x"$rsync_cv_HAVE_C99_VSNPRINTF" = x"yes"; then
    AC_DEFINE(HAVE_C99_VSNPRINTF, 1, [define if vsnprintf is C99 compliant])
fi


AC_CACHE_CHECK([for working socketpair],rsync_cv_HAVE_SOCKETPAIR,[
AC_TRY_RUN([
#include <sys/types.h>
#include <sys/socket.h>

main() {
       int fd[2];
       exit((socketpair(AF_UNIX, SOCK_STREAM, 0, fd) != -1) ? 0 : 1);
}],
rsync_cv_HAVE_SOCKETPAIR=yes,rsync_cv_HAVE_SOCKETPAIR=no,rsync_cv_HAVE_SOCKETPAIR=cross)])
if test x"$rsync_cv_HAVE_SOCKETPAIR" = x"yes"; then
    AC_DEFINE(HAVE_SOCKETPAIR, 1, [define if you have a working socketpair])
fi




dnl Checks for structures

dnl ##### Output
AC_SUBST(docdir)
AC_SUBST(CFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(BUILD_POPT)
dnl AC_DEFINE_UNQUOTED(PACKAGE, $PACKAGE, [Package name])
dnl AC_DEFINE_UNQUOTED(VERSION, $VERSION, [Package version])
AC_DEFINE_UNQUOTED(GNU_HOST, ["$host"], [Your gnu-style host triple])
AC_OUTPUT([Makefile])


##### Finalization

# Display success, installation paths, and GPL licence statement.
${MAKE-make} showpaths
cat <<EOF

    $PACKAGE_NAME $PACKAGE_VERSION configured

    Copyright (C) 2002, 2003 by Martin Pool <mbp@samba.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.  Unauthorized redistribution is 
    prohibited by law.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


After installing distcc, please complete the survey in the file "survey.txt".

EOF
