dnl aclocal.m4 generated automatically by aclocal 1.4

dnl Copyright (C) 1994, 1995-8, 1999 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY, to the extent permitted by law; without
dnl even the implied warranty of MERCHANTABILITY or FITNESS FOR A
dnl PARTICULAR PURPOSE.

# Like AC_CONFIG_HEADER, but automatically create stamp file.

AC_DEFUN(AM_CONFIG_HEADER,
[AC_PREREQ([2.12])
AC_CONFIG_HEADER([$1])
dnl When config.status generates a header, we must update the stamp-h file.
dnl This file resides in the same directory as the config header
dnl that is generated.  We must strip everything past the first ":",
dnl and everything past the last "/".
AC_OUTPUT_COMMANDS(changequote(<<,>>)dnl
ifelse(patsubst(<<$1>>, <<[^ ]>>, <<>>), <<>>,
<<test -z "<<$>>CONFIG_HEADERS" || echo timestamp > patsubst(<<$1>>, <<^\([^:]*/\)?.*>>, <<\1>>)stamp-h<<>>dnl>>,
<<am_indx=1
for am_file in <<$1>>; do
  case " <<$>>CONFIG_HEADERS " in
  *" <<$>>am_file "*<<)>>
    echo timestamp > `echo <<$>>am_file | sed -e 's%:.*%%' -e 's%[^/]*$%%'`stamp-h$am_indx
    ;;
  esac
  am_indx=`expr "<<$>>am_indx" + 1`
done<<>>dnl>>)
changequote([,]))])

# Define a conditional.

AC_DEFUN(AM_CONDITIONAL,
[AC_SUBST($1_TRUE)
AC_SUBST($1_FALSE)
if $2; then
  $1_TRUE=
  $1_FALSE='#'
else
  $1_TRUE='#'
  $1_FALSE=
fi])

# Do all the work for Automake.  This macro actually does too much --
# some checks are only needed if your package does certain things.
# But this isn't really a big deal.

# serial 1

dnl Usage:
dnl AM_INIT_AUTOMAKE(package,version, [no-define])

AC_DEFUN(AM_INIT_AUTOMAKE,
[AC_REQUIRE([AC_PROG_INSTALL])
PACKAGE=[$1]
AC_SUBST(PACKAGE)
VERSION=[$2]
AC_SUBST(VERSION)
dnl test to see if srcdir already configured
if test "`cd $srcdir && pwd`" != "`pwd`" && test -f $srcdir/config.status; then
  AC_MSG_ERROR([source directory already configured; run "make distclean" there first])
fi
ifelse([$3],,
AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE", [Name of package])
AC_DEFINE_UNQUOTED(VERSION, "$VERSION", [Version number of package]))
AC_REQUIRE([AM_SANITY_CHECK])
AC_REQUIRE([AC_ARG_PROGRAM])
dnl FIXME This is truly gross.
missing_dir=`cd $ac_aux_dir && pwd`
AM_MISSING_PROG(ACLOCAL, aclocal, $missing_dir)
AM_MISSING_PROG(AUTOCONF, autoconf, $missing_dir)
AM_MISSING_PROG(AUTOMAKE, automake, $missing_dir)
AM_MISSING_PROG(AUTOHEADER, autoheader, $missing_dir)
AM_MISSING_PROG(MAKEINFO, makeinfo, $missing_dir)
AC_REQUIRE([AC_PROG_MAKE_SET])])

#
# Check to make sure that the build environment is sane.
#

AC_DEFUN(AM_SANITY_CHECK,
[AC_MSG_CHECKING([whether build environment is sane])
# Just in case
sleep 1
echo timestamp > conftestfile
# Do `set' in a subshell so we don't clobber the current shell's
# arguments.  Must try -L first in case configure is actually a
# symlink; some systems play weird games with the mod time of symlinks
# (eg FreeBSD returns the mod time of the symlink's containing
# directory).
if (
   set X `ls -Lt $srcdir/configure conftestfile 2> /dev/null`
   if test "[$]*" = "X"; then
      # -L didn't work.
      set X `ls -t $srcdir/configure conftestfile`
   fi
   if test "[$]*" != "X $srcdir/configure conftestfile" \
      && test "[$]*" != "X conftestfile $srcdir/configure"; then

      # If neither matched, then we have a broken ls.  This can happen
      # if, for instance, CONFIG_SHELL is bash and it inherits a
      # broken ls alias from the environment.  This has actually
      # happened.  Such a system could not be considered "sane".
      AC_MSG_ERROR([ls -t appears to fail.  Make sure there is not a broken
alias in your environment])
   fi

   test "[$]2" = conftestfile
   )
then
   # Ok.
   :
else
   AC_MSG_ERROR([newly created file is older than distributed files!
Check your system clock])
fi
rm -f conftest*
AC_MSG_RESULT(yes)])

dnl AM_MISSING_PROG(NAME, PROGRAM, DIRECTORY)
dnl The program must properly implement --version.
AC_DEFUN(AM_MISSING_PROG,
[AC_MSG_CHECKING(for working $2)
# Run test in a subshell; some versions of sh will print an error if
# an executable is not found, even if stderr is redirected.
# Redirect stdin to placate older versions of autoconf.  Sigh.
if ($2 --version) < /dev/null > /dev/null 2>&1; then
   $1=$2
   AC_MSG_RESULT(found)
else
   $1="$3/missing $2"
   AC_MSG_RESULT(missing)
fi
AC_SUBST($1)])

dnl init_automake.m4--cmulocal automake setup macro
dnl Rob Earhart

AC_DEFUN(CMU_INIT_AUTOMAKE, [
	AC_REQUIRE([AM_INIT_AUTOMAKE])
	ACLOCAL="$ACLOCAL -I \$(top_srcdir)/cmulocal"
	])

dnl
dnl $Id: aclocal.m4,v 1.2 2002/05/23 18:58:36 snsimon Exp $
dnl

dnl
dnl Test for __attribute__
dnl

AC_DEFUN(CMU_C___ATTRIBUTE__, [
AC_MSG_CHECKING(for __attribute__)
AC_CACHE_VAL(ac_cv___attribute__, [
AC_TRY_COMPILE([
#include <stdlib.h>
],
[
static void foo(void) __attribute__ ((noreturn));

static void
foo(void)
{
  exit(1);
}
],
ac_cv___attribute__=yes,
ac_cv___attribute__=no)])
if test "$ac_cv___attribute__" = "yes"; then
  AC_DEFINE(HAVE___ATTRIBUTE__, 1, [define if your compiler has __attribute__])
fi
AC_MSG_RESULT($ac_cv___attribute__)
])


dnl bsd_sockets.m4--which socket libraries do we need? 
dnl Derrick Brashear
dnl from Zephyr

dnl Hacked on by Rob Earhart to not just toss stuff in LIBS
dnl It now puts everything required for sockets into LIB_SOCKET

AC_DEFUN(CMU_SOCKETS, [
	save_LIBS="$LIBS"
	LIB_SOCKET=""
	AC_CHECK_FUNC(connect, :,
		AC_CHECK_LIB(nsl, gethostbyname,
			     LIB_SOCKET="-lnsl $LIB_SOCKET")
		AC_CHECK_LIB(socket, connect,
			     LIB_SOCKET="-lsocket $LIB_SOCKET")
	)
	LIBS="$LIB_SOCKET $save_LIBS"
	AC_CHECK_FUNC(res_search, :,
                AC_CHECK_LIB(resolv, res_search,
                              LIB_SOCKET="-lresolv $LIB_SOCKET") 
        )
	LIBS="$LIB_SOCKET $save_LIBS"
	AC_CHECK_FUNCS(dn_expand dns_lookup)
	LIBS="$save_LIBS"
	AC_SUBST(LIB_SOCKET)
	])

dnl checking for kerberos 4 libraries (and DES)

AC_DEFUN(SASL_DES_CHK, [
AC_ARG_WITH(des, [  --with-des=DIR          with DES (look in DIR) [yes] ],
	with_des=$withval,
	with_des=yes)

LIB_DES=""
if test "$with_des" != no; then
  if test -d $with_des; then
    CPPFLAGS="$CPPFLAGS -I${with_des}/include"
    LDFLAGS="$LDFLAGS -L${with_des}/lib"
  fi

  dnl check for openssl installing -lcrypto, then make vanilla check
  AC_CHECK_LIB(crypto, des_cbc_encrypt,
      AC_CHECK_HEADER(openssl/des.h, [AC_DEFINE(WITH_SSL_DES)
                                     LIB_DES="-lcrypto";
                                     with_des=yes],
                     with_des=no),
      with_des=no, $LIB_RSAREF)

  if test "$with_des" = no; then
    AC_CHECK_LIB(des, des_cbc_encrypt, [LIB_DES="-ldes";
                                        with_des=yes], with_des=no)
  fi

  if test "$with_des" = no; then
     AC_CHECK_LIB(des524, des_cbc_encrypt, [LIB_DES="-ldes524";
                                       with_des=yes], with_des=no)
  fi

  if test "$with_des" = no; then
    dnl if openssl is around, we might be able to use that for des

    dnl if openssl has been compiled with the rsaref2 libraries,
    dnl we need to include the rsaref libraries in the crypto check
    LIB_RSAREF=""
    AC_CHECK_LIB(rsaref, RSAPublicEncrypt,
                 LIB_RSAREF="-lRSAglue -lrsaref"; cmu_have_rsaref=yes,
                 cmu_have_rsaref=no)

    AC_CHECK_LIB(crypto, des_cbc_encrypt, 
	AC_CHECK_HEADER(openssl/des.h, [AC_DEFINE(WITH_SSL_DES)
					LIB_DES="-lcrypto";
					with_des=yes],
			with_des=no), 
        with_des=no, $LIB_RSAREF)
  fi
fi

if test "$with_des" != no; then
  AC_DEFINE(WITH_DES)
fi

AC_SUBST(LIB_DES)
])

AC_DEFUN(SASL_KERBEROS_V4_CHK, [
  AC_REQUIRE([SASL_DES_CHK])

  AC_ARG_ENABLE(krb4, [  --enable-krb4           enable KERBEROS_V4 authentication [yes] ],
    krb4=$enableval,
    krb4=yes)

  if test "$krb4" != no; then
    dnl In order to compile kerberos4, we need libkrb and libdes.
    dnl (We've already gotten libdes from SASL_DES_CHK)
    dnl we might need -lresolv for kerberos
    AC_CHECK_LIB(resolv,res_search)

    dnl if we were ambitious, we would look more aggressively for the
    dnl krb4 install
    if test -d ${krb4}; then
       AC_CACHE_CHECK(for Kerberos includes, cyrus_krbinclude, [
         for krbhloc in include/kerberosIV include/kerberos include
         do
           if test -f ${krb4}/${krbhloc}/krb.h ; then
             cyrus_krbinclude=${krb4}/${krbhloc}
             break
           fi
         done
         ])

       if test -n "${cyrus_krbinclude}"; then
         CPPFLAGS="$CPPFLAGS -I${cyrus_krbinclude}"
       fi
       LDFLAGS="$LDFLAGS -L$krb4/lib"
    fi

    if test "$with_des" != no; then
      AC_CHECK_HEADER(krb.h,
        AC_CHECK_LIB(com_err, com_err,
	  AC_CHECK_LIB(krb, krb_mk_priv,
                     [COM_ERR="-lcom_err"; SASL_KRB_LIB="-lkrb"],
                     krb4=no, $LIB_DES -lcom_err), 
    	  AC_CHECK_LIB(krb, krb_mk_priv,
                     [COM_ERR=""; SASL_KRB_LIB="-lkrb"],
                     krb4=no, $LIB_DES)))

      if test "$krb4" = no; then
        AC_CHECK_HEADER(krb.h,
	  AC_CHECK_LIB(krb4, krb_mk_priv,
                     [COM_ERR=""; SASL_KRB_LIB="-lkrb4"; krb4=yes],
                     krb4=no, $LIB_DES))
        if test "$krb4" = no; then
          AC_WARN(No Kerberos V4 found)
        fi
      fi
    else
      AC_WARN(No DES library found for Kerberos V4 support)
      krb4=no
    fi
  fi

  if test "$krb4" != no; then
    cmu_save_LIBS="$LIBS"
    LIBS="$LIBS $SASL_KRB_LIB"
    AC_CHECK_FUNCS(krb_get_err_text)
    LIBS="$cmu_save_LIBS"
  fi

  AC_MSG_CHECKING(KERBEROS_V4)
  if test "$krb4" != no; then
    AC_MSG_RESULT(enabled)
    SASL_MECHS="$SASL_MECHS libkerberos4.la"
    SASL_STATIC_OBJS="$SASL_STATIC_OBJS ../plugins/kerberos4.o"
    AC_DEFINE(STATIC_KERBEROS4)
    AC_DEFINE(HAVE_KRB)
    SASL_KRB_LIB="$SASL_KRB_LIB $LIB_DES $COM_ERR"
  else
    AC_MSG_RESULT(disabled)
  fi
  AC_SUBST(SASL_KRB_LIB)
])


dnl sasl2.m4--sasl2 libraries and includes
dnl Rob Siemborski

AC_DEFUN(SASL_GSSAPI_CHK,[
 AC_ARG_WITH(gssapi, [  --with-gssapi=<DIR>	  enable GSSAPI authentication [yes] ],
    gssapi=$withval,
    gssapi=yes)

 if test "$gssapi" != no; then
    if test -d ${gssapi}; then
       CPPFLAGS="$CPPFLAGS -I$gssapi/include"
       LDFLAGS="$LDFLAGS -L$gssapi/lib"
    fi
    AC_CHECK_HEADER(gssapi.h, AC_DEFINE(HAVE_GSSAPI_H),
      AC_CHECK_HEADER(gssapi/gssapi.h,, AC_WARN(Disabling GSSAPI); gssapi=no))
 fi

 if test "$gssapi" != no; then
  dnl We need to find out which gssapi implementation we are
  dnl using. Supported alternatives are: MIT Kerberos 5 and
  dnl Heimdal Kerberos 5 (http://www.pdc.kth.se/heimdal)
  dnl
  dnl The choice is reflected in GSSAPIBASE_LIBS
  dnl we might need libdb
  AC_CHECK_LIB(db, db_open)

  gss_impl="mit";
  AC_CHECK_LIB(resolv,res_search)
  if test -d ${gssapi}; then 
     CPPFLAGS="$CPPFLAGS -I$gssapi/include"
     LDFLAGS="$LDFLAGS -L$gssapi/lib"
  fi

  # the base64_decode check fails because libroken has dependencies
  # FIXME: this is probabally non-optimal as well
  AC_CHECK_LIB(krb5,krb5_vlog,gss_impl="heimdal",,)
  #  AC_CHECK_LIB(roken,base64_decode,gss_impl="heimdal",, $LIB_CRYPT)

  if test -d ${gssapi}; then
     gssapi_dir="${gssapi}/lib"
     GSSAPIBASE_LIBS="-L$gssapi_dir"
     GSSAPIBASE_STATIC_LIBS="-L$gssapi_dir"
  else
     dnl FIXME: This is only used for building cyrus, and then only as
     dnl a real hack.  it needs to be fixed.
     gssapi_dir="/usr/local/lib"
  fi

  if test "$gss_impl" = "mit"; then
     GSSAPIBASE_LIBS="$GSSAPIBASE_LIBS -lgssapi_krb5 -lkrb5 -lk5crypto -lcom_err"
     GSSAPIBASE_STATIC_LIBS="$GSSAPIBASE_LIBS $gssapi_dir/libgssapi_krb5.a $gssapi_dir/libkrb5.a $gssapi_dir/libk5crypto.a $gssapi_dir/libcom_err.a"
  elif test "$gss_impl" = "heimdal"; then
     GSSAPIBASE_LIBS="$GSSAPIBASE_LIBS -lgssapi -lkrb5 -ldes -lasn1 -lroken ${LIB_CRYPT} -lcom_err"
     GSSAPIBASE_STATIC_LIBS="$GSSAPIBASE_STATIC_LIBS $gssapi_dir/libgssapi.a $gssapi_dir/libkrb5.a $gssapi_dir/libdes.a $gssapi_dir/libasn1.a $gssapi_dir/libroken.a $gssapi_dir/libcom_err.a ${LIB_CRYPT}"
  else
     gssapi="no"
     AC_WARN(Disabling GSSAPI)
  fi
 fi

 if test "$ac_cv_header_gssapi_h" = "yes"; then
  AC_EGREP_HEADER(GSS_C_NT_HOSTBASED_SERVICE, gssapi.h,
    AC_DEFINE(HAVE_GSS_C_NT_HOSTBASED_SERVICE))
 elif test "$ac_cv_header_gssapi_gssapi_h"; then
  AC_EGREP_HEADER(GSS_C_NT_HOSTBASED_SERVICE, gssapi/gssapi.h,
    AC_DEFINE(HAVE_GSS_C_NT_HOSTBASED_SERVICE))
 fi

 GSSAPI_LIBS=""
 AC_MSG_CHECKING(GSSAPI)
 if test "$gssapi" != no; then
  AC_MSG_RESULT(with implementation ${gss_impl})
  AC_CHECK_LIB(ndbm,dbm_open,GSSAPIBASE_LIBS="$GSSAPIBASE_LIBS -lndbm")
  AC_CHECK_LIB(resolv,res_search,GSSAPIBASE_LIBS="$GSSAPIBASE_LIBS -lresolv")
  SASL_MECHS="$SASL_MECHS libgssapiv2.la"
  SASL_STATIC_OBJS="$SASL_STATIC_OBJS ../plugins/gssapi.o"

  cmu_save_LIBS="$LIBS"
  LIBS="$LIBS $GSSAPIBASE_LIBS"
  AC_CHECK_FUNCS(gsskrb5_register_acceptor_identity)
  LIBS="$cmu_save_LIBS"
else
  AC_MSG_RESULT(disabled)
fi
AC_SUBST(GSSAPI_LIBS)
AC_SUBST(GSSAPIBASE_LIBS)
])

dnl What we want to do here is setup LIB_SASL with what one would
dnl generally want to have (e.g. if static is requested, make it that,
dnl otherwise make it dynamic.

dnl We also want to creat LIB_DYN_SASL and DYNSASLFLAGS.

dnl Also sets using_static_sasl to "no" "static" or "staticonly"

AC_DEFUN(CMU_SASL2, [
AC_ARG_WITH(sasl,
            [  --with-sasl=DIR         Compile with libsasl2 in <DIR>],
	    with_sasl="$withval",
            with_sasl="yes")

AC_ARG_WITH(staticsasl,
	    [  --with-staticsasl=DIR   Compile with staticly linked libsasl2 in <DIR>],
	    with_staticsasl="$withval";
	    if test $with_staticsasl != "no"; then
		using_static_sasl="static"
	    fi,
	    with_staticsasl="no"; using_static_sasl="no")

	SASLFLAGS=""
	LIB_SASL=""

	cmu_saved_CPPFLAGS=$CPPFLAGS
	cmu_saved_LDFLAGS=$LDFLAGS
	cmu_saved_LIBS=$LIBS

	if test ${with_staticsasl} != "no"; then
	  if test -d ${with_staticsasl}; then
	    ac_cv_sasl_where_lib=${with_staticsasl}/lib
	    ac_cv_sasl_where_inc=${with_staticsasl}/include

	    SASLFLAGS="-I$ac_cv_sasl_where_inc"
	    LIB_SASL="-L$ac_cv_sasl_where_lib"
	    CPPFLAGS="${cmu_saved_CPPFLAGS} -I${ac_cv_sasl_where_inc}"
	    LDFLAGS="${cmu_saved_LDFLAGS} -L${ac_cv_sasl_where_lib}"
	  else
	    with_staticsasl="/usr"
	  fi

	  AC_CHECK_HEADER(sasl/sasl.h,
	    AC_CHECK_HEADER(sasl/saslutil.h,
	     if test -r ${with_staticsasl}/lib/libsasl2.a; then
		ac_cv_found_sasl=yes
		AC_MSG_CHECKING(for static libsasl)
		LIB_SASL="$LIB_SASL ${with_staticsasl}/lib/libsasl2.a"
	     else
	        AC_MSG_CHECKING(for static libsasl)
		AC_ERROR([Could not find ${with_staticsasl}/lib/libsasl2.a])
	     fi
	    ))

	  AC_MSG_RESULT(found)

	  SASL_GSSAPI_CHK

	  LIB_SASL="$LIB_SASL $GSSAPIBASE_STATIC_LIBS"
	fi

	if test -d ${with_sasl}; then
            ac_cv_sasl_where_lib=${with_sasl}/lib
            ac_cv_sasl_where_inc=${with_sasl}/include

	    DYNSASLFLAGS="-I$ac_cv_sasl_where_inc"
	    if test "$ac_cv_sasl_where_lib" != ""; then
		CMU_ADD_LIBPATH_TO($ac_cv_sasl_where_lib, LIB_DYN_SASL)
	    fi
	    LIB_DYN_SASL="$LIB_DYN_SASL -lsasl2"
	    CPPFLAGS="${cmu_saved_CPPFLAGS} -I${ac_cv_sasl_where_inc}"
	    LDFLAGS="${cmu_saved_LDFLAGS} -L${ac_cv_sasl_where_lib}"
	fi

	dnl be sure to check for a SASLv2 specific function
	AC_CHECK_HEADER(sasl/sasl.h,
	    AC_CHECK_HEADER(sasl/saslutil.h,
	      AC_CHECK_LIB(sasl2, prop_get, 
                           ac_cv_found_sasl=yes,
		           ac_cv_found_sasl=no),
	                   ac_cv_found_sasl=no), ac_cv_found_sasl=no)

	if test "$ac_cv_found_sasl" = "yes"; then
	    if test "$ac_cv_sasl_where_lib" != ""; then
	        CMU_ADD_LIBPATH_TO($ac_cv_sasl_where_lib, DYNLIB_SASL)
	    fi
	    DYNLIB_SASL="$DYNLIB_SASL -lsasl2"
	    if test "$using_static_sasl" != "static"; then
		LIB_SASL=$DYNLIB_SASL
		SASLFLAGS=$DYNSASLFLAGS
	    fi
	else
	    DYNLIB_SASL=""
	    DYNSASLFLAGS=""
	    using_static_sasl="staticonly"
	fi

	LIBS="$cmu_saved_LIBS"
	LDFLAGS="$cmu_saved_LDFLAGS"
	CPPFLAGS="$cmu_saved_CPPFLAGS"

	AC_SUBST(LIB_DYN_SASL)
	AC_SUBST(DYNSASLFLAGS)
	AC_SUBST(LIB_SASL)
	AC_SUBST(SASLFLAGS)
	])

AC_DEFUN(CMU_SASL2_REQUIRED,
[AC_REQUIRE([CMU_SASL2])
if test "$ac_cv_found_sasl" != "yes"; then
        AC_ERROR([Cannot continue without libsasl2.
Get it from ftp://ftp.andrew.cmu.edu/pub/cyrus-mail/.])
fi])

AC_DEFUN(CMU_SASL2_CHECKAPOP_REQUIRED, [
	AC_REQUIRE([CMU_SASL2_REQUIRED])

	cmu_saved_LDFLAGS=$LDFLAGS

	LDFLAGS="$LIB_SASL $LDFLAGS"

	AC_CHECK_LIB(sasl2, sasl_checkapop, AC_DEFINE(HAVE_APOP),
		AC_MSG_ERROR([libsasl2 without working sasl_checkapop.  Cannot continue.]))

	LDFLAGS=$cmu_saved_LDFLAGS
])

dnl
dnl Additional macros for configure.in packaged up for easier theft.
dnl tjs@andrew.cmu.edu 6-may-1998
dnl

dnl It would be good if ANDREW_ADD_LIBPATH could detect if something was
dnl already there and not redundantly add it if it is.

dnl add -L(arg), and possibly (runpath switch)(arg), to LDFLAGS
dnl (so the runpath for shared libraries is set).
AC_DEFUN(CMU_ADD_LIBPATH, [
  # this is CMU ADD LIBPATH
  if test "$andrew_runpath_switch" = "none" ; then
	LDFLAGS="-L$1 ${LDFLAGS}"
  else
	LDFLAGS="-L$1 $andrew_runpath_switch$1 ${LDFLAGS}"
  fi
])

dnl add -L(1st arg), and possibly (runpath switch)(1st arg), to (2nd arg)
dnl (so the runpath for shared libraries is set).
AC_DEFUN(CMU_ADD_LIBPATH_TO, [
  # this is CMU ADD LIBPATH TO
  if test "$andrew_runpath_switch" = "none" ; then
	$2="-L$1 ${$2}"
  else
	$2="-L$1 ${$2} $andrew_runpath_switch$1"
  fi
])

dnl runpath initialization
AC_DEFUN(CMU_GUESS_RUNPATH_SWITCH, [
   # CMU GUESS RUNPATH SWITCH
  AC_CACHE_CHECK(for runpath switch, andrew_runpath_switch, [
    # first, try -R
    SAVE_LDFLAGS="${LDFLAGS}"
    LDFLAGS="-R /usr/lib"
    AC_TRY_LINK([],[],[andrew_runpath_switch="-R"], [
  	LDFLAGS="-Wl,-rpath,/usr/lib"
    AC_TRY_LINK([],[],[andrew_runpath_switch="-Wl,-rpath,"],
    [andrew_runpath_switch="none"])
    ])
  LDFLAGS="${SAVE_LDFLAGS}"
  ])])

dnl Check for PLAIN (and therefore crypt)

AC_DEFUN(SASL_CRYPT_CHK,[
 AC_CHECK_FUNC(crypt, cmu_have_crypt=yes,
  AC_CHECK_LIB(crypt, crypt,
	       LIB_CRYPT="-lcrypt"; cmu_have_crypt=yes,
	       cmu_have_crypt=no))
 AC_SUBST(LIB_CRYPT)
])

AC_DEFUN(SASL_PLAIN_CHK,[
AC_REQUIRE([SASL_CRYPT_CHK])

dnl PLAIN
 AC_ARG_ENABLE(plain, [  --enable-plain          enable PLAIN authentication [yes] ],
  plain=$enableval,
  plain=yes)

 PLAIN_LIBS=""
 if test "$plain" != no; then
  dnl In order to compile plain, we need crypt.
  if test "$cmu_have_crypt" = yes; then
    PLAIN_LIBS=$LIB_CRYPT
  fi
 fi
 AC_SUBST(PLAIN_LIBS)

 AC_MSG_CHECKING(PLAIN)
 if test "$plain" != no; then
  AC_MSG_RESULT(enabled)
  SASL_MECHS="$SASL_MECHS libplain.la"
  SASL_STATIC_OBJS="$SASL_STATIC_OBJS ../plugins/plain.o"
  AC_DEFINE(STATIC_PLAIN)
 else
  AC_MSG_RESULT(disabled)
 fi
])

dnl Functions to check what database to use for libsasldb

dnl Berkeley DB specific checks first..

dnl this is unbelievably painful due to confusion over what db-3 should be
dnl named and where the db-3 header file is located.  arg.
AC_DEFUN(BERKELEY_DB_CHK_LIB,
[
	BDB_SAVE_LIBS=$LIBS

	if test -d $with_bdb_lib; then
	    LIBS="$LIBS -L$with_bdb_lib"
	    BDB_LIBADD="-L$with_bdb_lib -R $with_bdb_lib"
	else
	    BDB_LIBADD=""
	fi

        for dbname in db-4.0 db4.0 db-4 db-3.3 db3.3 db-3.2 db3.2 db-3.1 db3.1 db-3 db3 db
          do
            AC_CHECK_LIB($dbname, db_create, SASL_DB_LIB="$BDB_LIBADD -l$dbname";
              dblib="berkeley"; break, dblib="no")
          done
        if test "$dblib" = "no"; then
          AC_CHECK_LIB(db, db_open, SASL_DB_LIB="$BDB_LIBADD -ldb";
            dblib="berkeley"; dbname=db,
            dblib="no")
        fi

	LIBS=$BDB_SAVE_LIBS
])

AC_DEFUN(BERKELEY_DB_CHK,
[
	if test -d $with_bdb_inc; then
	    CPPFLAGS="$CPPFLAGS -I$with_bdb_inc"
	    BDB_INCADD="-I$with_bdb_inc"
	else
	    BDB_INCADD=""
	fi

	dnl FreeBSD puts it in a wierd place
	dnl (but they should use with-bdb-incdir now)
	AC_CHECK_HEADER(db.h,
                       	BERKELEY_DB_CHK_LIB()
			SASL_DB_INC=$BDB_INCADD,
                        dblib="no")
])

dnl Figure out what database type we're using
AC_DEFUN(SASL_DB_CHECK, [
cmu_save_LIBS="$LIBS"
AC_ARG_WITH(dblib, [  --with-dblib=DBLIB      set the DB library to use [berkeley] ],
  dblib=$withval,
  dblib=auto_detect)

AC_ARG_WITH(bdb-libdir,
	[  --with-bdb-libdir=DIR   Berkeley DB lib files are in DIR],
	with_bdb_lib=$withval,
	with_bdb_lib=none)
AC_ARG_WITH(bdb-incdir,
	[  --with-bdb-incdir=DIR   Berkeley DB include files are in DIR],
	with_bdb_inc=$withval,
	with_bdb_inc=none)

SASL_DB_LIB=""

case "$dblib" in
dnl this is unbelievably painful due to confusion over what db-3 should be
dnl named.  arg.
  berkeley)
	BERKELEY_DB_CHK()
	;;
  gdbm)
	AC_ARG_WITH(with-gdbm,[  --with-gdbm=PATH        use gdbm from PATH],
                    with_gdbm="${withval}")

        case "$with_gdbm" in
           ""|yes)
               AC_CHECK_HEADER(gdbm.h,
			AC_CHECK_LIB(gdbm, gdbm_open, SASL_DB_LIB="-lgdbm",
                                           dblib="no"),
			dblib="no")
               ;;
           *)
               if test -d $with_gdbm; then
                 CPPFLAGS="${CPPFLAGS} -I${with_gdbm}/include"
                 LDFLAGS="${LDFLAGS} -L${with_gdbm}/lib"
                 SASL_DB_LIB="-lgdbm" 
               else
                 with_gdbm="no"
               fi
       esac
	;;
  ndbm)
	dnl We want to attempt to use -lndbm if we can, just in case
	dnl there's some version of it installed and overriding libc
	AC_CHECK_HEADER(ndbm.h,
			AC_CHECK_LIB(ndbm, dbm_open, SASL_DB_LIB="-lndbm",
				AC_CHECK_FUNC(dbm_open,,dblib="no")),
				dblib="no")
	;;
  auto_detect)
        dnl How about berkeley db?
	BERKELEY_DB_CHK()
	if test "$dblib" = no; then
	  dnl How about ndbm?
	  AC_CHECK_HEADER(ndbm.h, 
		AC_CHECK_LIB(ndbm, dbm_open,
			     dblib="ndbm"; SASL_DB_LIB="-lndbm",
		   	     dblib="weird"),
		   dblib="no")
	  if test "$dblib" = "weird"; then
	    dnl Is ndbm in the standard library?
            AC_CHECK_FUNC(dbm_open, dblib="ndbm", dblib="no")
	  fi

	  if test "$dblib" = no; then
            dnl Can we use gdbm?
   	    AC_CHECK_HEADER(gdbm.h,
		AC_CHECK_LIB(gdbm, gdbm_open, dblib="gdbm";
					     SASL_DB_LIB="-lgdbm", dblib="no"),
  			     dblib="no")
	  fi
	fi
	;;
  none)
	;;
  no)
	;;
  *)
	AC_MSG_WARN([Bad DB library implementation specified;])
	AC_ERROR([Use either \"berkeley\", \"gdbm\", \"ndbm\" or \"none\"])
	dblib=no
	;;
esac
LIBS="$cmu_save_LIBS"

AC_MSG_CHECKING(DB library to use)
AC_MSG_RESULT($dblib)

SASL_DB_BACKEND="db_${dblib}.lo"
SASL_DB_BACKEND_STATIC="../sasldb/db_${dblib}.o ../sasldb/allockey.o"
SASL_DB_UTILS="saslpasswd2 sasldblistusers2"
SASL_DB_MANS="saslpasswd2.8 sasldblistusers2.8"

case "$dblib" in
  gdbm) 
    SASL_MECHS="$SASL_MECHS libsasldb.la"
    SASL_STATIC_OBJS="$SASL_STATIC_OBJS ../plugins/sasldb.o"
    AC_DEFINE(STATIC_SASLDB)
    AC_DEFINE(SASL_GDBM)
    ;;
  ndbm)
    SASL_MECHS="$SASL_MECHS libsasldb.la"
    SASL_STATIC_OBJS="$SASL_STATIC_OBJS ../plugins/sasldb.o"
    AC_DEFINE(STATIC_SASLDB)
    AC_DEFINE(SASL_NDBM)
    ;;
  berkeley)
    SASL_MECHS="$SASL_MECHS libsasldb.la"
    SASL_STATIC_OBJS="$SASL_STATIC_OBJS ../plugins/sasldb.o"
    AC_DEFINE(STATIC_SASLDB)
    AC_DEFINE(SASL_BERKELEYDB)
    ;;
  *)
    AC_MSG_WARN([Disabling SASL authentication database support])
    SASL_DB_BACKEND="db_none.lo"
    SASL_DB_BACKEND_STATIC="../sasldb/db_none.o"
    SASL_DB_UTILS=""
    SASL_DB_MANS=""
    SASL_DB_LIB=""
    ;;
esac
AC_SUBST(SASL_DB_UTILS)
AC_SUBST(SASL_DB_MANS)
AC_SUBST(SASL_DB_BACKEND)
AC_SUBST(SASL_DB_BACKEND_STATIC)
AC_SUBST(SASL_DB_INC)
AC_SUBST(SASL_DB_LIB)
])

dnl Figure out what database path we're using
AC_DEFUN(SASL_DB_PATH_CHECK, [
AC_ARG_WITH(dbpath, [  --with-dbpath=PATH      set the DB path to use [/etc/sasldb2] ],
  dbpath=$withval,
  dbpath=/etc/sasldb2)
AC_MSG_CHECKING(DB path to use)
AC_MSG_RESULT($dbpath)
AC_DEFINE_UNQUOTED(SASL_DB_PATH, "$dbpath")])

dnl See whether we can use IPv6 related functions
dnl contributed by Hajimu UMEMOTO

AC_DEFUN(IPv6_CHECK_FUNC, [
changequote(, )dnl
ac_tr_lib=HAVE_`echo $1 | sed -e 's/[^a-zA-Z0-9_]/_/g' \
  -e 'y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/'`
changequote([, ])dnl
AC_CHECK_FUNC($1, [dnl
  AC_DEFINE_UNQUOTED($ac_tr_lib)
  ac_cv_lib_socket_$1=no
  ac_cv_lib_inet6_$1=no
], [dnl
  AC_CHECK_LIB(socket, $1, [dnl
    AC_DEFINE_UNQUOTED($ac_tr_lib)
    LIBS="$LIBS -lsocket"
    ac_cv_lib_inet6_$1=no
  ], [dnl
    AC_MSG_CHECKING([whether your system has IPv6 directory])
    AC_CACHE_VAL(ipv6_cv_dir, [dnl
      for ipv6_cv_dir in /usr/local/v6 /usr/inet6 no; do
	if test $ipv6_cv_dir = no -o -d $ipv6_cv_dir; then
	  break
	fi
      done])dnl
    AC_MSG_RESULT($ipv6_cv_dir)
    if test $ipv6_cv_dir = no; then
      ac_cv_lib_inet6_$1=no
    else
      if test x$ipv6_libinet6 = x; then
	ipv6_libinet6=no
	SAVELDFLAGS="$LDFLAGS"
	LDFLAGS="$LDFLAGS -L$ipv6_cv_dir/lib"
      fi
      AC_CHECK_LIB(inet6, $1, [dnl
	AC_DEFINE_UNQUOTED($ac_tr_lib)
	if test $ipv6_libinet6 = no; then
	  ipv6_libinet6=yes
	  LIBS="$LIBS -linet6"
	fi],)dnl
      if test $ipv6_libinet6 = no; then
	LDFLAGS="$SAVELDFLAGS"
      fi
    fi])dnl
])dnl
if test $ac_cv_func_$1 = yes -o $ac_cv_lib_socket_$1 = yes \
     -o $ac_cv_lib_inet6_$1 = yes
then
  ipv6_cv_$1=yes
  ifelse([$2], , :, [$2])
else
  ipv6_cv_$1=no
  ifelse([$3], , :, [$3])
fi])


dnl See whether we have ss_family in sockaddr_storage
AC_DEFUN(IPv6_CHECK_SS_FAMILY, [
AC_MSG_CHECKING([whether you have ss_family in struct sockaddr_storage])
AC_CACHE_VAL(ipv6_cv_ss_family, [dnl
AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/socket.h>],
	[struct sockaddr_storage ss; int i = ss.ss_family;],
	[ipv6_cv_ss_family=yes], [ipv6_cv_ss_family=no])])dnl
if test $ipv6_cv_ss_family = yes; then
  ifelse([$1], , AC_DEFINE(HAVE_SS_FAMILY), [$1])
else
  ifelse([$2], , :, [$2])
fi
AC_MSG_RESULT($ipv6_cv_ss_family)])


dnl whether you have sa_len in struct sockaddr
AC_DEFUN(IPv6_CHECK_SA_LEN, [
AC_MSG_CHECKING([whether you have sa_len in struct sockaddr])
AC_CACHE_VAL(ipv6_cv_sa_len, [dnl
AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/socket.h>],
	       [struct sockaddr sa; int i = sa.sa_len;],
	       [ipv6_cv_sa_len=yes], [ipv6_cv_sa_len=no])])dnl
if test $ipv6_cv_sa_len = yes; then
  ifelse([$1], , AC_DEFINE(HAVE_SOCKADDR_SA_LEN), [$1])
else
  ifelse([$2], , :, [$2])
fi
AC_MSG_RESULT($ipv6_cv_sa_len)])


dnl See whether sys/socket.h has socklen_t
AC_DEFUN(IPv6_CHECK_SOCKLEN_T, [
AC_MSG_CHECKING(for socklen_t)
AC_CACHE_VAL(ipv6_cv_socklen_t, [dnl
AC_TRY_LINK([#include <sys/types.h>
#include <sys/socket.h>],
	    [socklen_t len = 0;],
	    [ipv6_cv_socklen_t=yes], [ipv6_cv_socklen_t=no])])dnl
if test $ipv6_cv_socklen_t = yes; then
  ifelse([$1], , AC_DEFINE(HAVE_SOCKLEN_T), [$1])
else
  ifelse([$2], , :, [$2])
fi
AC_MSG_RESULT($ipv6_cv_socklen_t)])
