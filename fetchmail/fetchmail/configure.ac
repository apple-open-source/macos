dnl Autoconfigure input file for fetchmail
#
# Fetchmail automatic configuration support
#
# Eric S. Raymond <esr@thyrsus.com>
# 2004 - 2009 Matthias Andree <matthias.andree@gmx.de>
#
dnl Process this file with autoconf to produce a configure script.
dnl

AC_INIT([fetchmail],[6.3.11],[fetchmail-users@lists.berlios.de])
AC_CONFIG_SRCDIR([fetchmail.h])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_LIBOBJ_DIR([.])

AC_CANONICAL_HOST

dnl automake options are in Makefile.am
AC_PREREQ(2.60)
dnl 2.60 required for AC_USE_SYSTEM_EXTENSIONS
AM_INIT_AUTOMAKE

dnl python is optional
#
# you can pass PYTHON=: in environment or on the command line
# to disable python detection and continue without building/installing
# fetchmail.conf -- be sure to check README.packaging, too!
#
AM_PATH_PYTHON(2.0,,AC_MSG_WARN([Disabling fetchmailconf: python 2.0 or greater not found]))
AM_CONDITIONAL([HAVE_PYTHON], [test "$PYTHON" != :])

AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_CPP			dnl Later checks need this.
AC_PROG_RANLIB
AM_PROG_CC_C_O
AC_USE_SYSTEM_EXTENSIONS

AC_ISC_POSIX
dnl AC_ISC_POSIX: - XXX FIXME: remove in fetchmail 6.4.
dnl This macro adds `-lcposix' to output variable `LIBS' if necessary
dnl for Posix facilities.  Sun dropped support for the obsolete
dnl INTERACTIVE Systems Corporation Unix on 2006-07-23.  New programs
dnl need not use this macro.  It is implemented as
dnl `AC_SEARCH_LIBS([strerror], [cposix])'

AC_HEADER_STDC
AC_HEADER_TIME
AC_TYPE_SIZE_T
AC_TYPE_PID_T
AC_TYPE_SIGNAL
AC_CHECK_HEADERS([unistd.h termios.h termio.h sgtty.h stdarg.h \
	sys/itimer.h fcntl.h sys/fcntl.h memory.h sys/wait.h \
	arpa/inet.h arpa/nameser.h netinet/in.h net/socket.h netdb.h \
	sys/select.h sys/socket.h sys/time.h langinfo.h])
if test _$ac_cv_header_stdarg_h != _yes ; then
AC_MSG_WARN([stdarg.h is not defined. Unsupported configuration, proceed at your own risk.])
fi
AC_CHECK_TYPE(u_int32_t,,
	AC_DEFINE(u_int32_t,unsigned int,
		[Define to unsigned int if <sys/types.h> does not define.]),
	[AC_INCLUDES_DEFAULT
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif])
AC_CHECK_HEADERS([resolv.h],,,[
#include <sys/types.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
])

AC_C_CONST			dnl getopt needs this.

AM_PROG_LEX
AC_PROG_MAKE_SET
AC_PROG_YACC

# Check for OS special cases
case $host_os in
darwin*)
    AC_MSG_NOTICE(found Darwin - Adding -DBIND_8_COMPAT to CFLAGS)
    CPPFLAGS="$CPPFLAGS -DBIND_8_COMPAT"
    ;;
# Check for FreeBSD special case: more libs needed
freebsd*)
    AC_MSG_NOTICE(found FreeBSD - Adding -lmd -lkvm -lcom_err to standard libraries)
    LIBS="$LIBS -lmd -lkvm -lcom_err"
    ;;
# Check for LynxOS special case: -lbsd needed (at least on 2.3.0) and -s
# not working.
lynxos*)
    AC_MSG_NOTICE(found LynxOS - Adding -lbsd to standard libraries)
    LIBS="$LIBS -lbsd"
    LDFLAGS=`echo $LDFLAGS | sed "s/-s //"`
    AC_MSG_NOTICE(found LynxOS - Prepending standard include path to gcc flags)
    CPPFLAGS="$CPPFLAGS -I/usr/include"
    ;;
# Check for Rhapsody special case: it doesn't like -s
rhapsody*)
    AC_MSG_NOTICE(found Rhapsody - Removing -s load flag)
    LDFLAGS=`echo $LDFLAGS | sed "s/-s //"`
    ;;
esac

AC_CACHE_SAVE

dnl i18n
AM_GNU_GETTEXT([external], [need-ngettext])
AM_GNU_GETTEXT_VERSION([0.14.6])
dnl end i18n

# Under sysV68, socket and friends are provided by the C library.
# -linet does not provide socket, but causes multiple definition
# errors at link-time.  It is thus better to only use the C library.
# So don't add -linet to the link list unless it's necessary
AC_CHECK_FUNC(socket,
    AC_MSG_RESULT(using libc's socket),
    AC_CHECK_LIB(socket,socket)
    AC_CHECK_LIB(inet,socket))

# The condition in this test copes with the presence of inet_addr in libc6.
AC_CHECK_FUNC(inet_addr,
    AC_MSG_RESULT(using libc's inet_addr),
    AC_CHECK_LIB(nsl,inet_addr))

dnl Port hack for Interactive UNIX System V/386 Release 3.2
AC_CHECK_LIB(cposix, strchr,
		[DEFS="$DEFS -D_SYSV3"
		LIBS="$LIBS -lcposix"])

dnl Port hack for Sparc/NetBSD-1.5
dnl
dnl NB: this has been disabled as it causes the unconditional
dnl addition of libintl to the build, which is both undesired
dnl and breaks on Solaris/Blastwave.org machines.
dnl
dnl AC_CHECK_LIB(intl, gettext,
dnl 		[LIBS="$LIBS -lintl"])

AC_REPLACE_FUNCS([strstr strcasecmp memmove stpcpy strlcpy strlcat])

AC_CHECK_FUNC(MD5Init, [],
	      [AC_LIBSOURCE(md5c.c)
               EXTRAOBJ="$EXTRAOBJ md5c.\$(OBJEXT)"])

AC_CHECK_FUNC(getopt_long, [],
              [AC_LIBSOURCES([getopt.c, getopt1.c])
	       EXTRAOBJ="$EXTRAOBJ getopt.\$(OBJEXT) getopt1.\$(OBJEXT)"])

AC_FUNC_VPRINTF

AC_SUBST(EXTRAOBJ)

AC_CHECK_FUNCS(tcsetattr stty setsid geteuid seteuid dnl
  strerror syslog snprintf vprintf vsnprintf vsyslog dnl
  atexit inet_aton strftime setrlimit socketpair dnl
  sigaction strdup setlocale)

AC_CHECK_DECLS(strerror)
dnl INET6 is used by KAME/getnameinfo
AC_CACHE_CHECK(for AF_INET6/PF_INET6,ac_cv_inet6,
AC_COMPILE_IFELSE([
  AC_LANG_PROGRAM([[
    #ifdef HAVE_SYS_TYPES_H
    #include <sys/types.h>
    #endif
    #ifdef HAVE_SYS_SOCKET_H
    #include <sys/socket.h>
    #endif
  ]],[[
    int foo = AF_INET6;
    int bar = PF_INET6;
  ]])],
  ac_cv_inet6=yes , ac_cv_inet6=no
))
if test "x$ac_cv_inet6" = xyes
then
    AC_DEFINE(INET6,1,Define to 1 if your system defines AF_INET6 and PF_INET6.)
fi

# Under Red Hat 4.0 (and many other Linuxes) -lresolv is seriously flaky
# and breaks gethostbyname(2).  It's better to use the bind stuff in the C
# library.  So don't add -lresolv to the link list unless it's necessary
# (It will be necessary when using GNU libc6).
old_LIBS="$LIBS"
for lib in '' -lresolv; do
    if test -z "$lib"; then
       AC_MSG_CHECKING([for res_search in libc])
    else
       AC_MSG_CHECKING([for res_search in $lib])
    fi
    LIBS="$old_LIBS $lib"
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <sys/types.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif
extern int res_search();
]], [[res_search(0, 0, 0, 0, 0); dn_skipname(0,0);]])],
    [AC_MSG_RESULT([found])
     AC_DEFINE(HAVE_RES_SEARCH, [1],
	       [Define to 1 if you have the 'res_search' and 'dn_skipname' functions.])
     break], [AC_MSG_RESULT([not found])])
     LIBS=$old_LIBS
done

dnl Check for libcrypt -- it may live in libc or libcrypt, as on IRIX
AC_CHECK_FUNC(crypt, , AC_CHECK_LIB(crypt,crypt))

dnl Check for RSA refersence library in case we're using SSL
AC_CHECK_LIB(rsaref, RSAPublicDecrypt)

dnl AC_FUNC_SETVBUF_REVERSED

dnl Check for usable void pointer type
AC_MSG_CHECKING(use of void pointer type)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[char *p;
    void *xmalloc();
    p = (char *) xmalloc(1);
   ]])],[AC_DEFINE(HAVE_VOIDPOINTER,1,[Define if your C compiler allows void * as a function result]) AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)])

dnl Check for ANSI volatile
AC_C_VOLATILE

dnl Check out the wait reality.  We have to assume sys/wait.h is present.
AC_CHECK_FUNCS(waitpid wait3)
AC_MSG_CHECKING(for union wait);
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
#include <sys/wait.h>]], [[union wait status; int pid; pid = wait (&status);
#ifdef WEXITSTATUS
/* Some POSIXoid systems have both the new-style macros and the old
   union wait type, and they do not work together.  If union wait
   conflicts with WEXITSTATUS et al, we don't want to use it at all.  */
if (WEXITSTATUS (status) != 0) pid = -1;
#endif
#ifdef HAVE_WAITPID
/* Make sure union wait works with waitpid.  */
pid = waitpid (-1, &status, 0);
#endif
]])],[AC_DEFINE(HAVE_UNION_WAIT,1,Define if 'union wait' is the type of the first arg to wait functions.) AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)])

AC_MSG_CHECKING(sys_siglist declaration in signal.h or unistd.h)
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <signal.h>
/* NetBSD declares sys_siglist in <unistd.h>.  */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif]], [[char *msg = *(sys_siglist + 1);]])],[AC_DEFINE(SYS_SIGLIST_DECLARED,1,[Define if 'sys_siglist' is declared by <signal.h>.]) AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)])

# Find the right directory to put the root-mode PID file in
for dir in "/var/run" "/etc"
do
	if test -d $dir 
	then
		break;
	fi
done
AC_MSG_RESULT(root-mode pid file will go in $dir)
AC_DEFINE_UNQUOTED(PID_DIR, "$dir", directory for PID lock files)

# We may have a fallback MDA available in case the socket open to the 
# local SMTP listener fails.  Best to use procmail for this, as we know
# it won't try delivering through local SMTP and cause a mail loop.
# Sendmail without the -t option to use the message headers will work too,
# not just for sendmail itself but for workalikes like exim.
#
# Note1: A disadvantage of using procmail is that local alias expansion
# according to /etc/aliases won't get done if we fall back.  This doesn't
# matter in single-drop mode.
#
# Note2: it would be a very bad idea to use any MDA that doesn't return
# a refuse-to-deliver status on disk- or process-table-full
# conditions; mail could get lost that way.  Sendmail and all of the MDAs
# like exim that might be lurking under a sendmail alias) do the right 
# thing in this circumstance.  Matthias Andree warns that procmail does
# not.  
#
# Note3: Defalt value of fallback is now off.  Matthias writes:
#
# 1. there is no way to predict when the fallback is used. With some MTAs
#    (such as those limiting load average), outer circumstances can cause
#    the fallback to kick in.
# 
# 2. the fallback changes fetchmail behaviour in unpredictable ways. It's
#    not only about alias expansion, .forwards to special filters won't
#    work, mail may end up in a different place (users claim "mail loss"
#    for that).
# 
# 3. The claim procmail did the right thing with its exit codes is plain
#    wrong. I've seen procmail exit with code 1 when it should have exited
#    with code 75, like, configuration errors. Procmail is a dangerous
#    beast and is best replaced by maildrop.
# 
# 4. if multiple choices exist (like procmail and maildrop), fetchmail
#    cannot tell which one it should choose. Say, your MTA is configured
#    to use maildrop to deliver to user's mailboxes, if fetchmail then
#    chooses procmail, this is plain wrong.
#

AC_PATH_PROG(procmail, procmail, "", $PATH:/usr/sbin)
AC_PATH_PROG(sendmail, sendmail, "", $PATH:/usr/sbin:/usr/lib)
AC_PATH_PROG(maildrop, maildrop, "", $PATH:/usr/local/bin)

###	use option --disable-fallback to disable fallback MDA
###	use option --enable-fallback=procmail or 
###		   --enable-fallback=sendmail to select
AC_ARG_ENABLE(fallback,
	[  --enable-fallback=procmail    enable procmail as fallback
  --enable-fallback=sendmail    enable /usr/sbin/sendmail as fallback
  --enable-fallback=maildrop    enable maildrop as fallback
  --enable-fallback=no          disable fallback],,[enable_fallback=no])

case "$enable_fallback" in
	sendmail)	if test -z "$sendmail" ; then 
				AC_MSG_ERROR([Sendmail selected as fallback, but not found])
				#not reached
			fi
			AC_DEFINE_UNQUOTED(FALLBACK_MDA, "$sendmail -i %T", Fallback MTA to use if defined)
			AC_MSG_NOTICE(Will use $sendmail as fallback MDA.)
			;;
	procmail)	if test -z "$procmail" ; then
				AC_MSG_ERROR([procmail selected as fallback, but not found])
				#not reached
			fi
			AC_DEFINE_UNQUOTED(FALLBACK_MDA, "$procmail -d %T")
			AC_MSG_NOTICE(Will use $procmail as fallback MDA.)
			;;
	maildrop)	if test -z "$maildrop" ; then
				AC_MSG_ERROR([maildrop selected as fallback, but not found])
				#not reached
			fi
			AC_DEFINE_UNQUOTED(FALLBACK_MDA, "$maildrop -d %T")
			AC_MSG_NOTICE(Will use $maildrop as fallback MDA.)
			;;
	no|unset)	AC_MSG_NOTICE(Will not use a fallback MDA.)
			;;
	auto|yes|set)	if test -n "$sendmail" ; then
				AC_DEFINE_UNQUOTED(FALLBACK_MDA,"$sendmail -i %T")
				AC_MSG_NOTICE(Will use $sendmail as fallback MDA.)
			else
				AC_MSG_WARN(No fallback MDA available.  procmail and maildrop are not eligible)
				AC_MSG_WARN(for automatic fallback MDA configuration for reliability reasons.)
			fi
			;;
	*)		AC_MSG_ERROR([unkown value for --enable-fallback given: $enable_fallback])
			#notreached
			;;
esac

AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)

###	use option --enable-POP2 to compile in the POP2 support
AC_ARG_ENABLE(POP2,
	[  --enable-POP2           compile in POP2 protocol support (obsolete)],
	[with_POP2=$enableval],
	[with_POP2=no])
test "$with_POP2" = "yes" && AC_DEFINE(POP2_ENABLE,1,Define if you want POP2 support compiled in)
AM_CONDITIONAL(POP2_ENABLE, test "$with_POP2" = yes)

###	use option --disable-POP3 to omit the POP3 support
AC_ARG_ENABLE(POP3,
	[  --disable-POP3          don't compile in POP3 protocol support],
	[with_POP3=$enableval],
	[with_POP3=yes])
test "$with_POP3" = "yes" && AC_DEFINE(POP3_ENABLE,1,Define if you want POP3 support compiled in)
AM_CONDITIONAL(POP3_ENABLE, test "$with_POP3" = yes)

###	use option --disable-IMAP to omit the IMAP support
AC_ARG_ENABLE(IMAP,
	[  --disable-IMAP          don't compile in IMAP protocol support],
	[with_IMAP=$enableval],
	[with_IMAP=yes])
test "$with_IMAP" = "yes" && AC_DEFINE(IMAP_ENABLE,1,Define if you want IMAP support compiled in)
AM_CONDITIONAL(IMAP_ENABLE, test "$with_IMAP" = yes)

###	use option --disable-ETRN to omit the ETRN support
AC_ARG_ENABLE(ETRN,
	[  --disable-ETRN          don't compile in ETRN protocol support],
	[with_ETRN=$enableval],
	[with_ETRN=yes])
test "$with_ETRN" = "yes" && AC_DEFINE(ETRN_ENABLE,1,Define if you want ETRN support compiled in.)
AM_CONDITIONAL(ETRN_ENABLE, test "$with_ETRN" = yes)

###	use option --disable-ODMR to omit the ODMR support
AC_ARG_ENABLE(ODMR,
	[  --disable-ODMR          don't compile in ODMR protocol support],
	[with_ODMR=$enableval],
	[with_ODMR=yes])
test "$with_ODMR" = "yes" && AC_DEFINE(ODMR_ENABLE,1,Define if you want ODMR support compiled in)
AM_CONDITIONAL(ODMR_ENABLE, test "$with_ODMR" = yes)

###	use option --enable-RPA to compile in the RPA support
AC_ARG_ENABLE(RPA,
	[  --enable-RPA            compile in RPA protocol support],
	[with_RPA=$enableval],
	[with_RPA=no])
test "$with_RPA" = "yes" && AC_DEFINE(RPA_ENABLE,1,Define if you want RPA support compiled in)
AM_CONDITIONAL(RPA_ENABLE, test "$with_RPA" = yes)

###	use option --enable-NTLM to compile in the NTLM support
AC_ARG_ENABLE(NTLM,
	[  --enable-NTLM           compile in NTLM authentication support],
	[with_NTLM=$enableval],
	[with_NTLM=no])
test "$with_NTLM" = "yes" && AC_DEFINE(NTLM_ENABLE,1,Define if you want NTLM authentication)

###	use option --enable-SDPS to compile in the SDPS support
AC_ARG_ENABLE(SDPS,
	[  --enable-SDPS           compile in SDPS protocol support],
	[with_SDPS=$enableval],
	[with_SDPS=no])
if test "$with_SDPS" = yes ; then
   if test "$with_POP3" != yes ; then
       AC_MSG_WARN([SDPS cannot be enabled with POP3 disabled. Disabling SDPS.])
       with_SDPS=no
   else
       AC_DEFINE(SDPS_ENABLE,1,Define if you want SDPS support compiled in)
   fi
fi
if test "$with_POP3" != yes && test "$with_POP2" != yes \
    && test "$with_IMAP"  != yes ; then
    AC_MSG_ERROR([You must enable at least one of POP2, POP3 and IMAP.])
fi

AC_CACHE_SAVE

###	use option --enable-opie to compile in the OPIE support
AC_ARG_ENABLE(opie,
	[  --enable-opie           support OTP through the OPIE library],
	[ AC_CHECK_HEADER(opie.h,, [AC_MSG_ERROR(cannot find <opie.h>, which is required for OPIE support.)])
          AC_CHECK_LIB(opie,opiegenerator,, [AC_MSG_ERROR(cannot find libopie, which is required for OPIE support.)])
          with_opie=$enableval],
	[with_opie=no])
test "$with_opie" = "yes" && AC_DEFINE(OPIE_ENABLE,1,Define if you want OPIE support compiled in)

dnl Mostly stolen from gnulib's getaddrinfo.m4
AC_SEARCH_LIBS(getaddrinfo, [nsl socket])
AC_CACHE_CHECK([for getaddrinfo],[fm_cv_getaddrinfo],[
  AC_TRY_LINK([
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
  ], [getaddrinfo(0, 0, 0, 0);],
    [ fm_cv_getaddrinfo=yes],
    [ fm_cv_getaddrinfo=no ])
])

if test x"$fm_cv_getaddrinfo" = "xyes"; then  
     AC_DEFINE(HAVE_GETADDRINFO, 1,
      [Define to 1 if you have the getaddrinfo function.])
fi

AC_CACHE_CHECK([for getnameinfo],[fm_cv_getnameinfo],[
  AC_TRY_LINK([
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifndef NULL
#define NULL ((void *)0)
#endif
  ], [getnameinfo(NULL,0, NULL,0, NULL, 0, 0);],
    [ fm_cv_getnameinfo=yes],
    [ fm_cv_getnameinfo=no ])
])
if test $fm_cv_getnameinfo = yes ; then
    AC_DEFINE(HAVE_GETNAMEINFO,1,[Define to 1 if your system has getnameinfo()])
fi

AM_CONDITIONAL(NEED_GETADDRINFO, test "$fm_cv_getaddrinfo" != yes)
AM_CONDITIONAL(NEED_GETNAMEINFO, test "$fm_cv_getnameinfo"   != yes)

AC_CHECK_FUNCS(inet_ntop)
dnl Check if getaddrinfo is async-signal-safe - most implementations aren't
if test "$fm_cv_getaddrinfo" = yes ; then
    AC_MSG_CHECKING(if your getaddrinfo is async-signal-safe)
    gai_ts=no
    dnl we have getaddrinfo() - check if the OS is known to have a async-signal-safe implementation
    case $host_os in
	darwin9*)				gai_ts=yes ;;
	linux*)					gai_ts=yes ;;
	freebsd5.5|freebsd6*|freebsd7*)		gai_ts=yes ;;
	solaris2.8|solaris2.9|solaris2.10)	gai_ts=yes ;;
    esac
    AC_MSG_RESULT($gai_ts)
    if test $gai_ts = yes ; then
	AC_DEFINE(GETADDRINFO_ASYNCSAFE, 1, [define to 1 if you know your getaddrinfo function is async-signal-safe])
    fi
fi

# This version of the Kerberos 4 and 5 options addresses the follwing issues:
# 
# * Build correctly under Heimdal kerberos if it is compiled with db2 and
#   OpenSSL support (Debian's is)
# * Build the kerberos.c stuff (KPOP) only for kerberosIV, to avoid breakage.
#   I don't know if this is 100% correct, but now at least IMAP and POP3
#   behave the same way regarding kerberosV.
# * Build without any fuss for both kerberosIV and V at the same time.
# * Move all the kerberos header mess to kerberos.h, and #include that
#   in driver.c and kerberos.c.
# 
# Tested using the Heimdal Kerberos V libs, Kungliga Tekniska Högskolan (the
# Royal Institute of Technology in Stockholm, Sweden)'s kerberos IV libs, and
# the MIT reference implementation of KerberosV (all as packaged in Debian).

###    use option --with-kerberos5=DIR to point at a Kerberos 5 directory
### make sure --with-ssl is run before --with-kerberos* !
AC_ARG_WITH(kerberos5,
       [  --with-kerberos5=DIR    point fetchmail compilation at a Kerberos 5 directory],
[
if test "$with_kerberos5" != "no"
then
# Check for a OpenBSD special case
if test "$with_kerberos5" = "yes" && ( test `uname` = "OpenBSD" )
then
  AS_MESSAGE(checking kerberosV for OpenBSD...)
  AC_DEFINE(HEIMDAL,1,Define if you have HEIMDAL kerberos 5)
  AC_DEFINE(KERBEROS_V5,1,Define if you have Kerberos V5)
  CFLAGS="$CFLAGS -I/usr/include/kerberosV"
  LIBS="$LIBS -lasn1 -lkrb5 -lcom_err -lkafs"
elif krb5-config 2> /dev/null >/dev/null ; then
  krb5_prefix=`krb5-config --prefix krb5`
  AC_MSG_RESULT([krb5-config points to kerberosV under $krb5_prefix])
  if test -f ${krb5_prefix}/include/et/com_err.h && \
   ! test -f ${krb5_prefix}/include/com_err.h  ; then
    CFLAGS="$CFLAGS -I${krb5_prefix}/include/et"
  fi
  unset krb5_prefix
  CFLAGS="$CFLAGS `krb5-config --cflags krb5`"
  LIBS="$LIBS `krb5-config --libs krb5`"
  AC_DEFINE(KERBEROS_V5)
else
  if test "$with_kerberos5" != "yes" 
  then
      LDFLAGS="$LDFLAGS -L${with_kerberos5}/lib"
      searchdirs="$with_kerberos5"
  else
      searchdirs="/usr/kerberos /usr/local/krb5 /usr/athena /usr"
  fi
  with_kerberos5=
  for dir in $searchdirs
  do AC_MSG_CHECKING([for Kerberos V in $dir])
     if test -f "$dir/include/krb5.h" || test -f "$dir/include/krb5/krb5.h"
     then
	if test -d "$dir/include/krb5" ; then CPPFLAGS="$CPPFLAGS -I$dir/include/krb5" ; fi
        ac_krblibs=
        if test -f "$dir/include/roken.h" || test -f "$dir/include/krb5/roken.h"
        then
           ac_krblibs="-lasn1 -lroken -lcom_err"
           AC_MSG_RESULT([Heimdal found])
          dnl Attempt to detect if we need to -ldb2 to link Heimdal
          dnl we assume we do if it is available
           AC_CHECK_LIB(db2, db_open, ac_krblibs="$ac_krblibs -ldb2", [],
                ${LDFLAGS})
           AC_CHECK_LIB(des, des_string_to_key, libk5crypto=-ldes,
              AC_CHECK_LIB(ssl, MD5_Init, [],
                AC_MSG_ERROR([DES libraries not found. Try adding --with-ssl to enable OpenSSL support]),
                ${LDFLAGS} ${ac_krblibs}),
           ${LDFLAGS} ${ac_krblibs})
           AC_DEFINE(HEIMDAL)
        else
          AC_MSG_RESULT([found])
          ac_krblibs="-lcom_err"
           AC_CHECK_LIB(crypto,
             krb5_des_string_to_key,
             libk5crypto=-lcrypto,
                AC_CHECK_LIB(k5crypto,
                   krb5_des_string_to_key,
                   libk5crypto=-lk5crypto,
                      AC_MSG_ERROR([Kerberos 5 DES libraries not found]),
                   ${LDFLAGS} ${ac_krblibs}),
                 ${LDFLAGS} ${ac_krblibs})
        fi
        AC_CHECK_LIB(krb5, krb5_init_context,[],
           continue,
           ${LDFLAGS} ${ac_krblibs})
        AC_DEFINE(KERBEROS_V5)
        test "$dir" != "/usr" && CFLAGS="$CFLAGS -I$dir/include"
        LDFLAGS="$LDFLAGS -L$dir/lib"
       LIBS="$LIBS $ac_krblibs $libk5crypto"
        with_kerberos5=$dir
        break
     else
        AC_MSG_RESULT([not found])
     fi
  done
  if test -z "$with_kerberos5" ; then
     AC_MSG_ERROR([Kerberos 5 libraries not found])
  fi
fi
fi
]) dnl --with-kerberos5=DIR

###    use option --with-kerberos=DIR to point at a Kerberos 4 directory
KERBEROS_V4=0
AC_ARG_WITH(kerberos,
       [  --with-kerberos=DIR     point fetchmail compilation at a Kerberos 4 directory],
[
if test "$with_kerberos" != "no"
then
    AC_MSG_WARN([Kerberos IV support is obsolete. Use --with-kerberos5 if possible.])
# Check for a NetBSD/OpenBSD special case
if test "$with_kerberos" = "yes" && ( test `uname` = "NetBSD" || test `uname` = "OpenBSD" )
then
  AS_MESSAGE(checking kerberosIV for `uname`...)
  KERBEROS_V4=1
  CFLAGS="$CFLAGS -I/usr/include/kerberosIV"
  case `uname` in
      NetBSD)  LIBS="$LIBS -lkrb -ldes -lroken -lcom_err" ;;
      OpenBSD) LIBS="$LIBS -lkrb -ldes" ;;
  esac
elif krb4-config 2> /dev/null >/dev/null ; then
  krb4_prefix=`krb4-config --prefix`
  AC_MSG_RESULT([krb4-config points to kerberosIV under $krb4_prefix])
  unset krb4_prefix
  CFLAGS="$CFLAGS `krb4-config --cflags`"
  LIBS="$LIBS `krb4-config --libs`"
  KERBEROS_V4=1
elif krb5-config 2> /dev/null >/dev/null ; then
  krb4_prefix=`krb5-config --prefix krb4`
  AC_MSG_RESULT([krb5-config points to kerberosIV under $krb4_prefix])
  if test -f ${krb4_prefix}/include/kerberosIV/krb.h ; then
    AC_DEFINE(KERBEROS_V4_V5,1,Define if you have Kerberos V4 headers under a kerberosIV directory)
  fi
  unset krb4_prefix
  CFLAGS="$CFLAGS `krb5-config --cflags krb4`"
  LIBS="$LIBS `krb5-config --libs krb4`"
  KERBEROS_V4=1
else
  #we need to detect when we're building under a kerberosV compatibility
  #layer, btw...
  if test "$with_kerberos" != "yes" ; then
     searchdirs="$with_kerberos"
  else
     searchdirs="$with_kerberos5 /usr/kerberos /usr/kerberosIV /usr/athena /usr"
  fi
  with_kerberos=
  ac_saveLDFLAGS="$LDFLAGS"
  for dir in $searchdirs
  do
     AC_MSG_CHECKING([for Kerberos IV in $dir])
     if test -f "$dir/include/krb.h" || test -f "$dir/include/krb4.h" \
            || test -f "$dir/include/kerberosIV/krb.h"
     then
        AC_MSG_RESULT([found])
     else
        AC_MSG_RESULT([not found])
       continue
     fi
     #Find libs
     if test -f "$with_kerberos5/roken.h" ; then
       AC_CHECK_LIB(45, krb_mk_req)
     fi
     LDFLAGS="-L$dir/lib $ac_saveLDFLAGS"
     if test `uname` = "FreeBSD"; then
        AC_SEARCH_LIBS(_ossl_old_des_string_to_key, [des425 des crypto], [], continue)
     else
        AC_SEARCH_LIBS(des_string_to_key, [crypto], [], continue)
     fi
     AC_SEARCH_LIBS(krb_realmofhost, [krb4 krb], [], continue)
     with_kerberos="$dir"
     if test -f "$dir/include/kerberosIV/krb.h" ; then
        dir="$dir/include/kerberosIV"
     else
       dir="$dir/include"
     fi
     KERBEROS_V4=1
     test -f "$with_kerberos5/roken.h" && AC_DEFINE(HEIMDAL)
     test "$dir" != "/usr/include" && CFLAGS="$CFLAGS -I$dir"
     LDFLAGS="$LDFLAGS -L$with_kerberos/lib"
     break
  done
  if test -z "$with_kerberos" ; then
     AC_MSG_ERROR([Kerberos 4 libraries not found])
  fi
  LDFLAGS="$ac_saveLDFLAGS"
fi
fi
]) dnl --with-kerberos=DIR
if test "$KERBEROS_V4" = 1 ; then
    AC_DEFINE(KERBEROS_V4,1,Define if you have Kerberos V4)
fi
AM_CONDITIONAL(KERBEROS_V4_ENABLE, test "$KERBEROS_V4" = 1)

###	use option --with-ssl to compile in the SSL support
AC_ARG_WITH(ssl,
	[  --with-ssl=[DIR]        enable SSL support using libraries in DIR],
	[with_ssl=$withval],
	[with_ssl=no])
test "$with_ssl" != "no" && AC_DEFINE(SSL_ENABLE,1,Define if you want SSL support compiled in)

if test "$with_ssl" = "yes"
then
    #	User didn't specify an SSL location.  Let's look at some common
    #	directories where SSL has been found in the past and try and auto
    #	configure for SSL.  OpenSSL determination will be made later.
    #	This will screw up if an OpenSSL install is located in a later
    #	directory than an older SSLeay install, but the user should fix that
    #	anyways and he can override on the configure line.
    #	Just testing for directories is not sufficient, /usr exists on
    #	all systems!
    for ac_dir in \
      /usr/local/ssl \
      /usr/local \
      /usr/ssl \
      /usr \
      /local/ssl \
      /opt/ssl \
      /opt/csw \
      ; \
    do
        if test -r "$ac_dir/include/openssl/ssl.h" ; then
            with_ssl=$ac_dir
            break;
        fi
    done
fi

if test -n "$with_ssl" -a "$with_ssl" != "no"
then
  # With the autoconfigure above, the only time this is going to be
  # true is going to be when we could not find the headers.  If they
  # are not in system standard locations, we are going to be broken.
  if test "$with_ssl" = "yes"
  then
    # Let's just define the standard location for the SSLeay root
    with_ssl="/usr/local/ssl"
  fi
  if test -r $with_ssl/include/openssl/ssl.h
  then
    ###	ssl.h found under openssl.  Use openssl configuration preferentially,
    AC_MSG_NOTICE(Enabling OpenSSL support in $with_ssl.)
    test "$with_ssl" != "/usr" && CFLAGS="$CFLAGS -I$with_ssl/include"
    ### In Red Hat 9, this file includes a reference to <krb5.h>, so we
    ### force the Kerberos direcory onto the include path so it will build.
    CFLAGS="$CFLAGS -I/usr/kerberos/include"
    ###	OpenBSD comes with ssl headers
  else
    AC_MSG_ERROR([SSL support enabled, but OpenSSL not found])
  fi
  LDFLAGS="$LDFLAGS -L$with_ssl/lib"
  LIBS="$LIBS -lssl -lcrypto"
  dnl check if -ldl is needed
  AC_MSG_CHECKING([for additional library dependencies of SSL])
  found=0
  save_LIBS="$LIBS"
  for i in "" "-ldl" ; do
      LIBS="$LDFLAGS $save_LIBS $i"
      AC_LINK_IFELSE([AC_LANG_PROGRAM(,[SSL_library_init()])],[found=1; break])
  done
  if test $found = 0 ; then
      AC_MSG_RESULT([error])
      AC_MSG_ERROR([cannot link with SSL - check config.log])
  fi
  LIBS="$save_LIBS $i"
  if test "$i" = "" ; then i="(none)" ; fi
  AC_MSG_RESULT($i)
  dnl XXX FIXME: use pkg-config if available!
  AC_DEFINE(SSL_ENABLE)
else
  AC_MSG_NOTICE(Disabling SSL support.)
fi

###	use option --with-socks=DIR to point at SOCKS library
AC_ARG_WITH(socks,
	[  --with-socks[=DIR]      add built-in SOCKS firewall access],
[
if test "$with_socks" != no
then
    if test "$with_socks" != yes
    then
        LDFLAGS="$LDFLAGS -L$with_socks"
    else
        AC_CHECK_LIB(socks, Rconnect,,
               AC_MSG_ERROR([could not find libsocks which is needed for built-in SOCKS support]))
    fi
    AC_DEFINE(HAVE_SOCKS,1,Define if you want built-in SOCKS support)
    CFLAGS="$CFLAGS -Dconnect=Rconnect -Dgetsockname=Rgetsockname -Dbind=Rbind -Daccept=Raccept -Dlisten=Rlisten -Dselect=Rselect"
    LIBS="-lsocks $LIBS"
fi])

###     use option --with-socks5=DIR to point at SOCKS library
AC_ARG_WITH(socks5,
        [  --with-socks5[=DIR]     add built-in SOCKS5 firewall access],
[
if test "$with_socks5" != no
then
  if test "$with_socks5" != yes
  then
    LDFLAGS="$LDFLAGS -L$with_socks5"
  fi
  ac_savedLDFLAGS="$LDFLAGS"
  LDFLAGS="$LDFLAGS $LDFLAGS"
  AC_CHECK_LIB(socks5, SOCKSconnect,,
  	AC_MSG_ERROR([could not find libsocks5 which is needed for built-in SOCKS5 support]))
  AC_DEFINE(HAVE_SOCKS)
  CFLAGS="$CFLAGS -Dconnect=SOCKSconnect -Dgetsockname=SOCKSgetsockname -Dbind=SOCKSbind -Daccept=SOCKSaccept -Dlisten=SOCKSlisten -Dselect=SOCKSselect -Drecvfrom=SOCKSrecvfrom -Dsendto=SOCKSsendto -Drecv=SOCKSrecv -Dsend=SOCKSsend -Dread=SOCKSread -Dwrite=SOCKSwrite -Drresvport=SOCKSrresvport -Dshutdown=SOCKSshutdown -Dlisten=SOCKSlisten -Dclose=SOCKSclose -Ddup=SOCKSdup -Ddup2=SOCKSdup2 -Dgethostbyname=SOCKSgethostbyname"
  AC_CHECK_FUNC(SOCKSfclose, [CFLAGS="$CFLAGS -Dfclose=SOCKSfclose"])
  LDFLAGS="$ac_savedLDFLAGS"
fi])

ACX_WHICH_GETHOSTBYNAME_R

###	use option --with-hesiod=DIR to point at a HESIOD directory
AC_ma_SEARCH_PACKAGE(hesiod, hesiod_getmailhost, /usr/athena /usr /usr/local,hesiod, hesiod.h)

###	use option --with-gssapi=DIR to compile in GSSAPI support
AC_ARG_WITH(gssapi,
	[  --with-gssapi[=DIR]     compile in GSSAPI support using libraries in DIR],
[
if test "$with_gssapi" != "no"
then
  if test "$with_gssapi" = "yes" -a -n "$with_kerberos5"
  then
    with_gssapi=$with_kerberos5
  fi

  if test "$with_gssapi" != "yes" -a -n "$with_gssapi"
  then
    CFLAGS="$CFLAGS -I$with_gssapi/include"
    LDFLAGS="$LDFLAGS -L$with_gssapi/lib"
  fi

  dnl obtain gssapi default libs
  if krb5-config --libs gssapi 2>/dev/null >/dev/null ; then
      LIBS="$LIBS `krb5-config --libs gssapi`"
  else
    AC_CHECK_LIB(gss, gss_check_version, LIBS="$LIBS -lgss", nogss=t)
    if test -n "$nogss"
    then
      # -lgss not found
      if test "$with_gssapi" = yes ; then
	  with_gssapi=/usr
      fi
      if test -f "$with_gssapi/include/roken.h" -o -f "$with_gssapi/include/krb5/roken.h"
      then
         # have roken.h, assume Heimdal
	 case `uname` in
	     NetBSD) LIBS="$LIBS -lkrb5" ;;
	     *)      LIBS="$LIBS -lkrb5 -lasn1 -ldes -lroken -ldb -ldl" ;;
	 esac
         AC_CHECK_LIB(gssapi, gss_init_sec_context,LIBS="$LIBS -lgssapi",
                      AC_MSG_ERROR([could not find libgssapi which is needed for GSSAPI support]), )
         AC_DEFINE(HEIMDAL)
      else
         # no roken.h, assume MIT
         AC_CHECK_LIB(gssapi_krb5, gss_init_sec_context,LIBS="$LIBS -lgssapi_krb5",
                      AC_MSG_ERROR([could not find libgssapi_krb5 which is needed for GSSAPI support]), -lkrb5)
      fi
    fi
  fi
  AC_DEFINE(GSSAPI,1,Define if you want GSSAPI authentication)
  if test "$with_gssapi" != yes ; then
    CPPFLAGS="$CPPFLAGS-I$with_gssapi/include"
  fi
  AC_CHECK_HEADERS(gss.h gssapi.h gssapi/gssapi.h gssapi/gssapi_generic.h)
  if test "$ac_cv_header_gssapi_h" = "yes"; then
    AC_EGREP_HEADER(GSS_C_NT_HOSTBASED_SERVICE, gssapi.h, AC_DEFINE(HAVE_GSS_C_NT_HOSTBASED_SERVICE,1,Define if you have MIT kerberos))
  else
    AC_EGREP_HEADER(GSS_C_NT_HOSTBASED_SERVICE, gssapi/gssapi.h, AC_DEFINE(HAVE_GSS_C_NT_HOSTBASED_SERVICE))
  fi
fi])

dnl ,------------------------------------------------------------------
dnl Check if we need TRIO
needtrio=0
if test "$FORCE_TRIO" = "yes" ; then
    needtrio=1
    ac_cv_func_vsnprintf=no
    ac_cv_func_snprintf=no
fi
if test "x$ac_cv_func_snprintf" != "xyes" ; then
    AC_DEFINE(snprintf, trio_snprintf,
	      [Define to trio_snprintf if your system lacks snprintf])
    needtrio=1
fi
if test "x$ac_cv_func_vsnprintf" != "xyes" ; then
    AC_DEFINE(vsnprintf, trio_vsnprintf,
	      [Define to trio_vsnprintf if your system lacks vsnprintf])
    needtrio=1
fi
AM_CONDITIONAL(NEED_TRIO, test "$needtrio" = 1)

dnl TRIO IEEE compiler option for Alpha
dnl
if test "$needtrio" = 1 ; then
    AC_MSG_CHECKING(for IEEE compilation options)
    AC_CACHE_VAL(ac_cv_ieee_option, [
    AC_TRY_COMPILE(,[
    #if !(defined(__alpha) && (defined(__DECC) || defined(__DECCXX) || (defined(__osf__) && defined(__LANGUAGE_C__))) && (defined(VMS) || defined(__VMS)))
    # error "Option needed"
    #endif
    ],ac_cv_ieee_option="/IEEE_MODE=UNDERFLOW_TO_ZERO/FLOAT=IEEE",
    AC_TRY_COMPILE(,[
    #if !(defined(__alpha) && (defined(__DECC) || defined(__DECCXX) || (defined(__osf__) && defined(__LANGUAGE_C__))) && !(defined(VMS) || defined(__VMS)) && !defined(_CFE))
    # error "Option needed"
    #endif
    ],ac_cv_ieee_option="-ieee",
    AC_TRY_COMPILE(,[
    #if !(defined(__alpha) && (defined(__GNUC__) && (defined(__osf__) || defined(__linux__))))
    # error "Option needed"
    #endif
    ],ac_cv_ieee_option="-mieee",
    ac_cv_ieee_option="none"
    )
    )
    )
    ])
    AC_MSG_RESULT($ac_cv_ieee_option)
    if test $ac_cv_ieee_option != none; then
      CFLAGS="${CFLAGS} ${ac_cv_ieee_option}"
    fi
fi
dnl ----------------------------------------------------------------'

AC_CONFIG_FILES([Makefile po/Makefile.in genlsm.sh])
AC_OUTPUT

dnl Local Variables:
dnl comment-start: "dnl "
dnl comment-end: ""
dnl comment-start-skip: "\\bdnl\\b\\s *"
dnl compile-command: "make configure config.h.in"
dnl End:
