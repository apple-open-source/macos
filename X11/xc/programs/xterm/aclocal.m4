dnl
dnl $XFree86: xc/programs/xterm/aclocal.m4,v 3.44 2002/12/27 21:05:20 dickey Exp $
dnl
dnl ---------------------------------------------------------------------------
dnl
dnl Copyright 1997-2001,2002 by Thomas E. Dickey
dnl
dnl                         All Rights Reserved
dnl
dnl Permission to use, copy, modify, and distribute this software and its
dnl documentation for any purpose and without fee is hereby granted,
dnl provided that the above copyright notice appear in all copies and that
dnl both that copyright notice and this permission notice appear in
dnl supporting documentation, and that the name of the above listed
dnl copyright holder(s) not be used in advertising or publicity pertaining
dnl to distribution of the software without specific, written prior
dnl permission.
dnl
dnl THE ABOVE LISTED COPYRIGHT HOLDER(S) DISCLAIM ALL WARRANTIES WITH REGARD
dnl TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
dnl AND FITNESS, IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE
dnl LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
dnl WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
dnl ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
dnl OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
dnl
dnl ---------------------------------------------------------------------------
dnl ---------------------------------------------------------------------------
dnl Copy non-preprocessor flags to $CFLAGS, preprocessor flags to $CPPFLAGS
dnl The second parameter if given makes this macro verbose.
AC_DEFUN([CF_ADD_CFLAGS],
[
cf_new_cflags=
cf_new_cppflags=
for cf_add_cflags in $1
do
	case $cf_add_cflags in #(vi
	-undef|-nostdinc*|-I*|-D*|-U*|-E|-P|-C) #(vi
		case "$CPPFLAGS" in
		*$cf_add_cflags) #(vi
			;;
		*) #(vi
			cf_new_cppflags="$cf_new_cppflags $cf_add_cflags"
			;;
		esac
		;;
	*)
		cf_new_cflags="$cf_new_cflags $cf_add_cflags"
		;;
	esac
done

if test -n "$cf_new_cflags" ; then
	ifelse($2,,,[CF_VERBOSE(add to \$CFLAGS $cf_new_cflags)])
	CFLAGS="$CFLAGS $cf_new_cflags"
fi

if test -n "$cf_new_cppflags" ; then
	ifelse($2,,,[CF_VERBOSE(add to \$CPPFLAGS $cf_new_cppflags)])
	CPPFLAGS="$cf_new_cppflags $CPPFLAGS"
fi

])dnl
dnl ---------------------------------------------------------------------------
dnl This is adapted from the macros 'fp_PROG_CC_STDC' and 'fp_C_PROTOTYPES'
dnl in the sharutils 4.2 distribution.
AC_DEFUN([CF_ANSI_CC_CHECK],
[
AC_CACHE_CHECK(for ${CC-cc} option to accept ANSI C, cf_cv_ansi_cc,[
cf_cv_ansi_cc=no
cf_save_CFLAGS="$CFLAGS"
cf_save_CPPFLAGS="$CPPFLAGS"
# Don't try gcc -ansi; that turns off useful extensions and
# breaks some systems' header files.
# AIX			-qlanglvl=ansi
# Ultrix and OSF/1	-std1
# HP-UX			-Aa -D_HPUX_SOURCE
# SVR4			-Xc
# UnixWare 1.2		(cannot use -Xc, since ANSI/POSIX clashes)
for cf_arg in "-DCC_HAS_PROTOS" \
	"" \
	-qlanglvl=ansi \
	-std1 \
	-Ae \
	"-Aa -D_HPUX_SOURCE" \
	-Xc
do
	CF_ADD_CFLAGS($cf_arg)
	AC_TRY_COMPILE(
[
#ifndef CC_HAS_PROTOS
#if !defined(__STDC__) || (__STDC__ != 1)
choke me
#endif
#endif
],[
	int test (int i, double x);
	struct s1 {int (*f) (int a);};
	struct s2 {int (*f) (double a);};],
	[cf_cv_ansi_cc="$cf_arg"; break])
done
CFLAGS="$cf_save_CFLAGS"
CPPFLAGS="$cf_save_CPPFLAGS"
])

if test "$cf_cv_ansi_cc" != "no"; then
if test ".$cf_cv_ansi_cc" != ".-DCC_HAS_PROTOS"; then
	CF_ADD_CFLAGS($cf_cv_ansi_cc)
else
	AC_DEFINE(CC_HAS_PROTOS)
fi
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Allow user to disable a normally-on option.
AC_DEFUN([CF_ARG_DISABLE],
[CF_ARG_OPTION($1,[$2],[$3],[$4],yes)])dnl
dnl ---------------------------------------------------------------------------
dnl Allow user to enable a normally-off option.
AC_DEFUN([CF_ARG_ENABLE],
[CF_ARG_OPTION($1,[$2],[$3],[$4],no)])dnl
dnl ---------------------------------------------------------------------------
dnl Restricted form of AC_ARG_ENABLE that ensures user doesn't give bogus
dnl values.
dnl
dnl Parameters:
dnl $1 = option name
dnl $2 = help-string
dnl $3 = action to perform if option is not default
dnl $4 = action if perform if option is default
dnl $5 = default option value (either 'yes' or 'no')
AC_DEFUN([CF_ARG_OPTION],
[AC_ARG_ENABLE($1,[$2],[test "$enableval" != ifelse($5,no,yes,no) && enableval=ifelse($5,no,no,yes)
  if test "$enableval" != "$5" ; then
ifelse($3,,[    :]dnl
,[    $3]) ifelse($4,,,[
  else
    $4])
  fi],[enableval=$5 ifelse($4,,,[
  $4
])dnl
  ])])dnl
dnl ---------------------------------------------------------------------------
dnl Check if we're accidentally using a cache from a different machine.
dnl Derive the system name, as a check for reusing the autoconf cache.
dnl
dnl If we've packaged config.guess and config.sub, run that (since it does a
dnl better job than uname).  Normally we'll use AC_CANONICAL_HOST, but allow
dnl an extra parameter that we may override, e.g., for AC_CANONICAL_SYSTEM
dnl which is useful in cross-compiles.
AC_DEFUN([CF_CHECK_CACHE],
[
if test -f $srcdir/config.guess ; then
	ifelse([$1],,[AC_CANONICAL_HOST],[$1])
	system_name="$host_os"
else
	system_name="`(uname -s -r) 2>/dev/null`"
	if test -z "$system_name" ; then
		system_name="`(hostname) 2>/dev/null`"
	fi
fi
test -n "$system_name" && AC_DEFINE_UNQUOTED(SYSTEM_NAME,"$system_name")
AC_CACHE_VAL(cf_cv_system_name,[cf_cv_system_name="$system_name"])

test -z "$system_name" && system_name="$cf_cv_system_name"
test -n "$cf_cv_system_name" && AC_MSG_RESULT(Configuring for $cf_cv_system_name)

if test ".$system_name" != ".$cf_cv_system_name" ; then
	AC_MSG_RESULT(Cached system name ($system_name) does not agree with actual ($cf_cv_system_name))
	AC_ERROR("Please remove config.cache and try again.")
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Conditionally add to $CFLAGS and $CPPFLAGS values which are derived from
dnl a build-configuration such as imake.  These have the pitfall that they
dnl often contain compiler-specific options which we cannot use, mixed with
dnl preprocessor options that we usually can.
AC_DEFUN([CF_CHECK_CFLAGS],
[
CF_VERBOSE(checking additions to CFLAGS)
cf_check_cflags="$CFLAGS"
cf_check_cppflags="$CPPFLAGS"
CF_ADD_CFLAGS($1,yes)
if test "$cf_check_cflags" != "$CFLAGS" ; then
AC_TRY_LINK([#include <stdio.h>],[printf("Hello world");],,
	[CF_VERBOSE(test-compile failed.  Undoing change to \$CFLAGS)
	 if test "$cf_check_cppflags" != "$CPPFLAGS" ; then
		 CF_VERBOSE(but keeping change to \$CPPFLAGS)
	 fi
	 CFLAGS="$cf_check_flags"])
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for data that is usually declared in <stdio.h> or <errno.h>, e.g.,
dnl the 'errno' variable.  Define a DECL_xxx symbol if we must declare it
dnl ourselves.
dnl
dnl $1 = the name to check
AC_DEFUN([CF_CHECK_ERRNO],
[
AC_CACHE_CHECK(if external $1 is declared, cf_cv_dcl_$1,[
    AC_TRY_COMPILE([
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdio.h>
#include <sys/types.h>
#include <errno.h> ],
    [long x = (long) $1],
    [cf_cv_dcl_$1=yes],
    [cf_cv_dcl_$1=no])
])

if test "$cf_cv_dcl_$1" = no ; then
    CF_UPPER(cf_result,decl_$1)
    AC_DEFINE_UNQUOTED($cf_result)
fi

# It's possible (for near-UNIX clones) that the data doesn't exist
CF_CHECK_EXTERN_DATA($1,int)
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for existence of external data in the current set of libraries.  If
dnl we can modify it, it's real enough.
dnl $1 = the name to check
dnl $2 = its type
AC_DEFUN([CF_CHECK_EXTERN_DATA],
[
AC_CACHE_CHECK(if external $1 exists, cf_cv_have_$1,[
    AC_TRY_LINK([
#undef $1
extern $2 $1;
],
    [$1 = 2],
    [cf_cv_have_$1=yes],
    [cf_cv_have_$1=no])
])

if test "$cf_cv_have_$1" = yes ; then
    CF_UPPER(cf_result,have_$1)
    AC_DEFINE_UNQUOTED($cf_result)
fi

])dnl
dnl ---------------------------------------------------------------------------
dnl You can always use "make -n" to see the actual options, but it's hard to
dnl pick out/analyze warning messages when the compile-line is long.
dnl
dnl Sets:
dnl	ECHO_LD - symbol to prefix "cc -o" lines
dnl	RULE_CC - symbol to put before implicit "cc -c" lines (e.g., .c.o)
dnl	SHOW_CC - symbol to put before explicit "cc -c" lines
dnl	ECHO_CC - symbol to put before any "cc" line
dnl
AC_DEFUN([CF_DISABLE_ECHO],[
AC_MSG_CHECKING(if you want to see long compiling messages)
CF_ARG_DISABLE(echo,
	[  --disable-echo          display "compiling" commands],
	[
    ECHO_LD='@echo linking [$]@;'
    RULE_CC='	@echo compiling [$]<'
    SHOW_CC='	@echo compiling [$]@'
    ECHO_CC='@'
],[
    ECHO_LD=''
    RULE_CC='# compiling'
    SHOW_CC='# compiling'
    ECHO_CC=''
])
AC_MSG_RESULT($enableval)
AC_SUBST(ECHO_LD)
AC_SUBST(RULE_CC)
AC_SUBST(SHOW_CC)
AC_SUBST(ECHO_CC)
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if 'errno' is declared in <errno.h>
AC_DEFUN([CF_ERRNO],
[
CF_CHECK_ERRNO(errno)
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for memmove, or a bcopy that can handle overlapping copy.  If neither
dnl is found, add our own version of memmove to the list of objects.
AC_DEFUN([CF_FUNC_MEMMOVE],
[
AC_CHECK_FUNC(memmove,,[
AC_CHECK_FUNC(bcopy,[
	AC_CACHE_CHECK(if bcopy does overlapping moves,cf_cv_good_bcopy,[
		AC_TRY_RUN([
int main() {
	static char data[] = "abcdefghijklmnopqrstuwwxyz";
	char temp[40];
	bcopy(data, temp, sizeof(data));
	bcopy(temp+10, temp, 15);
	bcopy(temp+5, temp+15, 10);
	exit (strcmp(temp, "klmnopqrstuwwxypqrstuwwxyz"));
}
		],
		[cf_cv_good_bcopy=yes],
		[cf_cv_good_bcopy=no],
		[cf_cv_good_bcopy=unknown])
		])
	],[cf_cv_good_bcopy=no])
	if test "$cf_cv_good_bcopy" = yes ; then
		AC_DEFINE(USE_OK_BCOPY)
	else
		AC_DEFINE(USE_MY_MEMMOVE)
	fi
])])dnl
dnl ---------------------------------------------------------------------------
dnl Check for tgetent function in termcap library.  If we cannot find this,
dnl we'll use the $LINES and $COLUMNS environment variables to pass screen
dnl size information to subprocesses.  (We cannot use terminfo's compatibility
dnl function, since it cannot provide the termcap-format data).
AC_DEFUN([CF_FUNC_TGETENT],
[
# compute a reasonable value for $TERM to give tgetent(), since we may be
# running in 'screen', which sets $TERMCAP to a specific entry that is not
# necessarily in /etc/termcap - unsetenv is not portable, so we cannot simply
# discard $TERMCAP.
cf_TERMVAR=vt100
test -n "$TERMCAP" && cf_TERMVAR="$TERM"
test -z "$cf_TERMVAR" && cf_TERMVAR=vt100

AC_CACHE_CHECK(for full tgetent function,cf_cv_lib_tgetent,[
cf_save_LIBS="$LIBS"
cf_cv_lib_tgetent=no
cf_TERMLIB="termcap termlib ncurses curses"
for cf_termlib in '' $cf_TERMLIB ; do
	LIBS="$cf_save_LIBS"
	test -n "$cf_termlib" && LIBS="$LIBS -l$cf_termlib"
	AC_TRY_RUN([
/* terminfo implementations ignore the buffer argument, making it useless for
 * the xterm application, which uses this information to make a new TERMCAP
 * environment variable.
 */
int main()
{
	char buffer[1024];
	buffer[0] = 0;
	tgetent(buffer, "$cf_TERMVAR");
	exit(buffer[0] == 0); }],
	[echo "yes, there is a termcap/tgetent in $cf_termlib" 1>&AC_FD_CC
	 if test -n "$cf_termlib" ; then
	 	cf_cv_lib_tgetent="-l$cf_termlib"
	 else
	 	cf_cv_lib_tgetent=yes
	 fi
	 break],
	[echo "no, there is no termcap/tgetent in $cf_termlib" 1>&AC_FD_CC],
	[echo "cross-compiling, cannot verify if a termcap/tgetent is present in $cf_termlib" 1>&AC_FD_CC])
done
LIBS="$cf_save_LIBS"
])

# If we found a working tgetent(), set LIBS and check for termcap.h.
# (LIBS cannot be set inside AC_CACHE_CHECK; the commands there should
# not have side effects other than setting the cache variable, because
# they are not executed when a cached value exists.)
if test "$cf_cv_lib_tgetent" != no ; then
	test "$cf_cv_lib_tgetent" != yes && LIBS="$LIBS $cf_cv_lib_tgetent"
	AC_DEFINE(USE_TERMCAP)
	AC_TRY_COMPILE([
#include <termcap.h>],[
#ifdef NCURSES_VERSION
make an error
#endif],[AC_DEFINE(HAVE_TERMCAP_H)])
else
        # If we didn't find a tgetent() that supports the buffer
        # argument, look again to see whether we can find even
        # a crippled one.  A crippled tgetent() is still useful to
        # validate values for the TERM environment variable given to
        # child processes.
	AC_CACHE_CHECK(for partial tgetent function,cf_cv_lib_part_tgetent,[
	cf_cv_lib_part_tgetent=no
	for cf_termlib in $cf_TERMLIB ; do
		LIBS="$cf_save_LIBS -l$cf_termlib"
		AC_TRY_LINK([],[tgetent(0, "$cf_TERMVAR")],
			[echo "there is a terminfo/tgetent in $cf_termlib" 1>&AC_FD_CC
			 cf_cv_lib_part_tgetent="-l$cf_termlib"
			 break])
	done
	LIBS="$cf_save_LIBS"
	])

	if test "$cf_cv_lib_part_tgetent" != no ; then
		LIBS="$LIBS $cf_cv_lib_part_tgetent"
		AC_CHECK_HEADERS(termcap.h)

                # If this is linking against ncurses, we'll trigger the
                # ifdef in resize.c that turns the termcap stuff back off.
		AC_DEFINE(USE_TERMINFO)
	fi
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Test for availability of useful gcc __attribute__ directives to quiet
dnl compiler warnings.  Though useful, not all are supported -- and contrary
dnl to documentation, unrecognized directives cause older compilers to barf.
AC_DEFUN([CF_GCC_ATTRIBUTES],
[
if test "$GCC" = yes
then
cat > conftest.i <<EOF
#ifndef GCC_PRINTF
#define GCC_PRINTF 0
#endif
#ifndef GCC_SCANF
#define GCC_SCANF 0
#endif
#ifndef GCC_NORETURN
#define GCC_NORETURN /* nothing */
#endif
#ifndef GCC_UNUSED
#define GCC_UNUSED /* nothing */
#endif
EOF
if test "$GCC" = yes
then
	AC_CHECKING([for $CC __attribute__ directives])
cat > conftest.$ac_ext <<EOF
#line __oline__ "configure"
#include "confdefs.h"
#include "conftest.h"
#include "conftest.i"
#if	GCC_PRINTF
#define GCC_PRINTFLIKE(fmt,var) __attribute__((format(printf,fmt,var)))
#else
#define GCC_PRINTFLIKE(fmt,var) /*nothing*/
#endif
#if	GCC_SCANF
#define GCC_SCANFLIKE(fmt,var)  __attribute__((format(scanf,fmt,var)))
#else
#define GCC_SCANFLIKE(fmt,var)  /*nothing*/
#endif
extern void wow(char *,...) GCC_SCANFLIKE(1,2);
extern void oops(char *,...) GCC_PRINTFLIKE(1,2) GCC_NORETURN;
extern void foo(void) GCC_NORETURN;
int main(int argc GCC_UNUSED, char *argv[[]] GCC_UNUSED) { return 0; }
EOF
	for cf_attribute in scanf printf unused noreturn
	do
		CF_UPPER(CF_ATTRIBUTE,$cf_attribute)
		cf_directive="__attribute__(($cf_attribute))"
		echo "checking for $CC $cf_directive" 1>&AC_FD_CC
		case $cf_attribute in
		scanf|printf)
		cat >conftest.h <<EOF
#define GCC_$CF_ATTRIBUTE 1
EOF
			;;
		*)
		cat >conftest.h <<EOF
#define GCC_$CF_ATTRIBUTE $cf_directive
EOF
			;;
		esac
		if AC_TRY_EVAL(ac_compile); then
			test -n "$verbose" && AC_MSG_RESULT(... $cf_attribute)
			cat conftest.h >>confdefs.h
#		else
#			sed -e 's/__attr.*/\/*nothing*\//' conftest.h >>confdefs.h
		fi
	done
else
	fgrep define conftest.i >>confdefs.h
fi
rm -rf conftest*
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if the compiler supports useful warning options.  There's a few that
dnl we don't use, simply because they're too noisy:
dnl
dnl	-Wconversion (useful in older versions of gcc, but not in gcc 2.7.x)
dnl	-Wredundant-decls (system headers make this too noisy)
dnl	-Wtraditional (combines too many unrelated messages, only a few useful)
dnl	-Wwrite-strings (too noisy, but should review occasionally)
dnl	-pedantic
dnl
AC_DEFUN([CF_GCC_WARNINGS],
[
if ( test "$GCC" = yes || test "$GXX" = yes )
then
	cat > conftest.$ac_ext <<EOF
#line __oline__ "configure"
int main(int argc, char *argv[[]]) { return (argv[[argc-1]] == 0) ; }
EOF
	AC_CHECKING([for $CC warning options])
	cf_save_CFLAGS="$CFLAGS"
	EXTRA_CFLAGS="-W -Wall"
	cf_warn_CONST=""
	test "$with_ext_const" = yes && cf_warn_CONST="Wwrite-strings"
	for cf_opt in \
		Wbad-function-cast \
		Wcast-align \
		Wcast-qual \
		Winline \
		Wmissing-declarations \
		Wmissing-prototypes \
		Wnested-externs \
		Wpointer-arith \
		Wshadow \
		Wstrict-prototypes \
		Wundef $cf_warn_CONST
	do
		CFLAGS="$cf_save_CFLAGS $EXTRA_CFLAGS -$cf_opt"
		if AC_TRY_EVAL(ac_compile); then
			test -n "$verbose" && AC_MSG_RESULT(... -$cf_opt)
			EXTRA_CFLAGS="$EXTRA_CFLAGS -$cf_opt"
			test "$cf_opt" = Wcast-qual && EXTRA_CFLAGS="$EXTRA_CFLAGS -DXTSTRINGDEFINES"
		fi
	done
	rm -f conftest*
	CFLAGS="$cf_save_CFLAGS"
fi
AC_SUBST(EXTRA_CFLAGS)
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if we must define _GNU_SOURCE to get a reasonable value for
dnl _XOPEN_SOURCE, upon which many POSIX definitions depend.  This is a defect
dnl (or misfeature) of glibc2, which breaks portability of many applications,
dnl since it is interwoven with GNU extensions.
dnl
dnl Well, yes we could work around it...
AC_DEFUN([CF_GNU_SOURCE],
[
AC_CACHE_CHECK(if we must define _GNU_SOURCE,cf_cv_gnu_source,[
AC_TRY_COMPILE([#include <sys/types.h>],[
#ifndef _XOPEN_SOURCE
make an error
#endif],
	[cf_cv_gnu_source=no],
	[cf_save="$CPPFLAGS"
	 CPPFLAGS="$CPPFLAGS -D_GNU_SOURCE"
	 AC_TRY_COMPILE([#include <sys/types.h>],[
#ifdef _XOPEN_SOURCE
make an error
#endif],
	[cf_cv_gnu_source=no],
	[cf_cv_gnu_source=yes])
	CPPFLAGS="$cf_save"
	])
])
test "$cf_cv_gnu_source" = yes && CPPFLAGS="$CPPFLAGS -D_GNU_SOURCE"
])dnl
dnl ---------------------------------------------------------------------------
dnl Insert text into the help-message, for readability, from AC_ARG_WITH.
AC_DEFUN([CF_HELP_MESSAGE],
[AC_DIVERT_HELP([$1])dnl
])dnl
dnl ---------------------------------------------------------------------------
dnl Use imake to obtain compiler flags.  We could, in principle, write tests to
dnl get these, but if imake is properly configured there is no point in doing
dnl this.
AC_DEFUN([CF_IMAKE_CFLAGS],
[
AC_PATH_PROGS(IMAKE,xmkmf imake)
case $IMAKE in # (vi
*/imake)
	cf_imake_opts="-DUseInstalled=YES" # (vi
	;;
*)
	cf_imake_opts=
	;;
esac

# If it's installed properly, imake (or its wrapper, xmkmf) will point to the
# config directory.
if mkdir conftestdir; then
	cf_makefile=`cd $srcdir;pwd`/Imakefile
	CDPATH=; export CDPATH
	cd conftestdir
	echo >./Imakefile
	test -f $cf_makefile && cat $cf_makefile >>./Imakefile
	cat >> ./Imakefile <<'CF_EOF'
findstddefs:
	@echo 'IMAKE_CFLAGS="${ALLDEFINES} ifelse($1,,,$1)"'
	@echo 'IMAKE_LOADFLAGS="${EXTRA_LOAD_FLAGS} ifelse($2,,,$2)"'
CF_EOF
	if ( $IMAKE $cf_imake_opts 1>/dev/null 2>&AC_FD_CC && test -f Makefile)
	then
		CF_VERBOSE(Using $IMAKE)
	else
		# sometimes imake doesn't have the config path compiled in.  Find it.
		cf_config=
		for cf_libpath in $X_LIBS $LIBS ; do
			case $cf_libpath in # (vi
			-L*)
				cf_libpath=`echo .$cf_libpath | sed -e 's/^...//'`
				cf_libpath=$cf_libpath/X11/config
				if test -d $cf_libpath ; then
					cf_config=$cf_libpath
					break
				fi
				;;
			esac
		done
		if test -z "$cf_config" ; then
			AC_WARN(Could not find imake config-directory)
		else
			cf_imake_opts="$cf_imake_opts -I$cf_config"
			if ( $IMAKE -v $cf_imake_opts 2>&AC_FD_CC)
			then
				CF_VERBOSE(Using $IMAKE $cf_config)
			else
				AC_WARN(Cannot run $IMAKE)
			fi
		fi
	fi

	# GNU make sometimes prints "make[1]: Entering...", which
	# would confuse us.
	eval `make findstddefs 2>/dev/null | grep -v make`

	cd ..
	rm -rf conftestdir

	# We use $(ALLDEFINES) rather than $(STD_DEFINES) because the former
	# declares XTFUNCPROTO there.  However, some vendors (e.g., SGI) have
	# modified it to support site.cf, adding a kludge for the /usr/include
	# directory.  Try to filter that out, otherwise gcc won't find its
	# headers.
	if test -n "$GCC" ; then
	    if test -n "$IMAKE_CFLAGS" ; then
		cf_nostdinc=""
		cf_std_incl=""
		cf_cpp_opts=""
		for cf_opt in $IMAKE_CFLAGS
		do
		    case "$cf_opt" in
		    -nostdinc) #(vi
			cf_nostdinc="$cf_opt"
			;;
		    -I/usr/include) #(vi
			cf_std_incl="$cf_opt"
			;;
		    *) #(vi
			cf_cpp_opts="$cf_cpp_opts $cf_opt"
			;;
		    esac
		done
		if test -z "$cf_nostdinc" ; then
		    IMAKE_CFLAGS="$cf_cpp_opts $cf_std_incl"
		elif test -z "$cf_std_incl" ; then
		    IMAKE_CFLAGS="$cf_cpp_opts $cf_nostdinc"
		else
		    CF_VERBOSE(suppressed \"$cf_nostdinc\" and \"$cf_std_incl\")
		    IMAKE_CFLAGS="$cf_cpp_opts"
		fi
	    fi
	fi
fi
AC_SUBST(IMAKE_CFLAGS)
AC_SUBST(IMAKE_LOADFLAGS)
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if the X libraries support input-method
AC_DEFUN([CF_INPUT_METHOD],
[
AC_CACHE_CHECK([if X libraries support input-method],cf_cv_input_method,[
AC_TRY_LINK([
#include <X11/IntrinsicP.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xmu/Atoms.h>
#include <X11/Xmu/Converters.h>
#include <X11/Xaw/XawImP.h>
],[
{
	XIM xim;
	XIMStyles *xim_styles = 0;
	XIMStyle input_style;
	Widget w = 0;

	XSetLocaleModifiers("@im=none");
	xim = XOpenIM(XtDisplay(w), NULL, NULL, NULL);
	XGetIMValues(xim, XNQueryInputStyle, &xim_styles, NULL);
	XCloseIM(xim);
	input_style = (XIMPreeditNothing | XIMStatusNothing);
}
],
[cf_cv_input_method=yes],
[cf_cv_input_method=no])])
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for header defining _PATH_LASTLOG, or failing that, see if the lastlog
dnl file exists.
AC_DEFUN([CF_LASTLOG],
[
AC_CHECK_HEADERS(lastlog.h paths.h)
AC_CACHE_CHECK(for lastlog path,cf_cv_path_lastlog,[
AC_TRY_COMPILE([
#include <sys/types.h>
#ifdef HAVE_LASTLOG_H
#include <lastlog.h>
#else
#ifdef HAVE_PATHS_H
#include <paths.h>
#endif
#endif],[char *path = _PATH_LASTLOG],
	[cf_cv_path_lastlog="_PATH_LASTLOG"],
	[if test -f /usr/adm/lastlog ; then
	 	cf_cv_path_lastlog=/usr/adm/lastlog
	else
		cf_cv_path_lastlog=no
	fi])
])
test $cf_cv_path_lastlog != no && AC_DEFINE(USE_LASTLOG)
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for POSIX wait support
AC_DEFUN([CF_POSIX_WAIT],
[
AC_REQUIRE([AC_HEADER_SYS_WAIT])
AC_CACHE_CHECK(for POSIX wait functions,cf_cv_posix_wait,[
AC_TRY_LINK([
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
],[
	int stat_loc;
	pid_t pid = waitpid(-1, &stat_loc, WNOHANG|WUNTRACED);
	pid_t pid2 = wait(&stat_loc);
],
[cf_cv_posix_wait=yes],
[cf_cv_posix_wait=no])
])
test "$cf_cv_posix_wait" = yes && AC_DEFINE(USE_POSIX_WAIT)
])dnl
dnl ---------------------------------------------------------------------------
dnl	On both Ultrix and CLIX, I find size_t defined in <stdio.h>
AC_DEFUN([CF_SIZE_T],
[
AC_MSG_CHECKING(for size_t in <sys/types.h> or <stdio.h>)
AC_CACHE_VAL(cf_cv_type_size_t,[
	AC_TRY_COMPILE([
#include <sys/types.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif
#include <stdio.h>],
		[size_t x],
		[cf_cv_type_size_t=yes],
		[cf_cv_type_size_t=no])
	])
AC_MSG_RESULT($cf_cv_type_size_t)
test $cf_cv_type_size_t = no && AC_DEFINE(size_t, unsigned)
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if this is an SVR4 system.  We need the definition for xterm
AC_DEFUN([CF_SVR4],
[
AC_CHECK_LIB(elf, elf_begin,[
AC_CACHE_CHECK(if this is an SVR4 system, cf_cv_svr4,[
AC_TRY_COMPILE([
#include <elf.h>
#include <sys/termio.h>
],[
static struct termio d_tio;
	d_tio.c_cc[VINTR] = 0;
	d_tio.c_cc[VQUIT] = 0;
	d_tio.c_cc[VERASE] = 0;
	d_tio.c_cc[VKILL] = 0;
	d_tio.c_cc[VEOF] = 0;
	d_tio.c_cc[VEOL] = 0;
	d_tio.c_cc[VMIN] = 0;
	d_tio.c_cc[VTIME] = 0;
	d_tio.c_cc[VLNEXT] = 0;
],
[cf_cv_svr4=yes],
[cf_cv_svr4=no])
])
])
test "$cf_cv_svr4" = yes && AC_DEFINE(SVR4)
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if this is a SYSV platform
AC_DEFUN([CF_SYSV],
[
AC_CACHE_CHECK(if this platform has SYSV flavor,cf_cv_sysv,[
AC_TRY_COMPILE([
#include <curses.h>
#include <term.h>
#include <sys/termio.h>
],[
/* FIXME: need a test that excludes linux */
#ifdef linux
make an error
#endif
static struct termio d_tio;
	d_tio.c_cc[VINTR] = 0;
	d_tio.c_cc[VQUIT] = 0;
	d_tio.c_cc[VERASE] = 0;
	d_tio.c_cc[VKILL] = 0;
	d_tio.c_cc[VEOF] = 0;
	d_tio.c_cc[VEOL] = 0;
	d_tio.c_cc[VMIN] = 0;
	d_tio.c_cc[VTIME] = 0;
],
[cf_cv_sysv=yes],
[cf_cv_sysv=no])
])
test "$cf_cv_sysv" = yes && AC_DEFINE(SYSV)
])dn
dnl ---------------------------------------------------------------------------
dnl Check if this is a SYSV flavor of UTMP
AC_DEFUN([CF_SYSV_UTMP],
[
AC_CACHE_CHECK(if $cf_cv_have_utmp is SYSV flavor,cf_cv_sysv_utmp,[
test "$cf_cv_have_utmp" = "utmp" && cf_prefix="ut" || cf_prefix="utx"
AC_TRY_LINK([
#include <sys/types.h>
#include <${cf_cv_have_utmp}.h>],[
struct $cf_cv_have_utmp x;
	set${cf_prefix}ent ();
	get${cf_prefix}id(&x);
	put${cf_prefix}line(&x);
	end${cf_prefix}ent();],
	[cf_cv_sysv_utmp=yes],
	[cf_cv_sysv_utmp=no])
])
test $cf_cv_sysv_utmp = yes && AC_DEFINE(USE_SYSV_UTMP)
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for SGI's broken redefinition of baud rates introduced in IRIX 6.5
dnl (there doesn't appear to be a useful predefined symbol).
AC_DEFUN([CF_TERMIO_C_ISPEED],
[
AC_CACHE_CHECK(for IRIX 6.5 baud-rate redefinitions,cf_cv_termio_c_ispeed,[
AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/termio.h>],[
struct termio foo;
foo.c_ispeed = B38400;
foo.c_ospeed = B9600;
],[cf_cv_termio_c_ispeed=yes
],[cf_cv_termio_c_ispeed=no])
])
test "$cf_cv_termio_c_ispeed" = yes && AC_DEFINE(HAVE_TERMIO_C_ISPEED)
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if the system has a tty-group defined.  This is used in xterm when
dnl setting pty ownership.
AC_DEFUN([CF_TTY_GROUP],
[
AC_CACHE_CHECK(for tty group name,cf_cv_tty_group_name,[

# If we are configuring as root, it is hard to get a clue about the tty group.
# But we'll guess based on how our connection is set up - assuming it is done
# properly.

cf_uid=`id | sed -e 's/^[^=]*=//' -e 's/(.*$//'`
if test "$cf_uid" != 0 ; then
cf_cv_tty_group_name=
cf_tty_name=`tty`
test "$cf_tty_name" = "not a tty" && cf_tty_name=/dev/tty
test -z "$cf_tty_name" && cf_tty_name=/dev/tty
if test -c "$cf_tty_name"
then
	cf_option="-l -L"

	# Expect listing to have fields like this:
	#-rwxrwxrwx   1 user      group       34293 Jul 18 16:29 pathname
	ls $cf_option $cf_tty_name >conftest.out
	read cf_mode cf_links cf_usr cf_grp cf_size cf_date1 cf_date2 cf_date3 cf_rest <conftest.out
	if test -z "$cf_rest" ; then
		cf_option="$cf_option -g"
		ls $cf_option $cf_tty_name >conftest.out
		read cf_mode cf_links cf_usr cf_grp cf_size cf_date1 cf_date2 cf_date3 cf_rest <conftest.out
	fi
	rm -f conftest.out
	cf_cv_tty_group_name=$cf_grp
fi
fi

# If we cannot deduce the tty group, fall back on hardcoded cases

if test -z "$cf_cv_tty_group_name"
then
case $host_os in #(vi
osf*) #(vi
	cf_cv_tty_group_name="terminal"
	;;
*)
	cf_cv_tty_group_name="tty"
	;;
esac
fi
])

AC_DEFINE_UNQUOTED(TTY_GROUP_NAME,"$cf_cv_tty_group_name")

AC_CACHE_CHECK(if we may use tty group,cf_cv_tty_group,[
AC_TRY_RUN([
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <grp.h>
int main()
{
	struct stat sb;
	struct group *ttygrp = getgrnam(TTY_GROUP_NAME);
	char *name = ttyname(0);

	endgrent();
	if (ttygrp != 0
	 && name != 0
	 && stat(name, &sb) == 0
	 && sb.st_gid != getgid()
	 && sb.st_gid == ttygrp->gr_gid) {
		exit(0);
	}
	exit(1);
}
	],
	[cf_cv_tty_group=yes],
	[cf_cv_tty_group=no],
	[cf_cv_tty_group=unknown])
])
test $cf_cv_tty_group = yes && AC_DEFINE(USE_TTY_GROUP)
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for the declaration of fd_set.  Some platforms declare it in
dnl <sys/types.h>, and some in <sys/select.h>, which requires <sys/types.h>.
dnl Finally, if we are using this for an X application, Xpoll.h may include
dnl <sys/select.h>, so we don't want to do it twice.
AC_DEFUN([CF_TYPE_FD_SET],
[
AC_CACHE_CHECK(for declaration of fd_set,cf_cv_type_fd_set,
	[echo "trying sys/types alone" 1>&AC_FD_CC
AC_TRY_COMPILE([
#include <sys/types.h>],
	[fd_set x],
	[cf_cv_type_fd_set=sys/types.h],
	[echo "trying X11/Xpoll.h" 1>&AC_FD_CC
AC_TRY_COMPILE([
#ifdef HAVE_X11_XPOLL_H
#include <X11/Xpoll.h>
#endif],
	[fd_set x],
	[cf_cv_type_fd_set=X11/Xpoll.h],
	[echo "trying sys/select.h" 1>&AC_FD_CC
AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/select.h>],
	[fd_set x],
	[cf_cv_type_fd_set=sys/select.h],
	[cf_cv_type_fd_set=unknown])])])])
if test $cf_cv_type_fd_set = sys/select.h ; then
	AC_DEFINE(USE_SYS_SELECT_H)
fi
])
dnl ---------------------------------------------------------------------------
dnl Make an uppercase version of a variable
dnl $1=uppercase($2)
AC_DEFUN([CF_UPPER],
[
$1=`echo "$2" | sed y%abcdefghijklmnopqrstuvwxyz./-%ABCDEFGHIJKLMNOPQRSTUVWXYZ___%`
])dnl
dnl ---------------------------------------------------------------------------
dnl Try to link with utempter library
AC_DEFUN([CF_UTEMPTER],
[
AC_CACHE_CHECK(if we can link with utempter library,cf_cv_have_utempter,[
cf_save_LIBS="$LIBS"
LIBS="-lutempter $LIBS"
AC_TRY_LINK([
#include <utempter.h>
],[
	addToUtmp("/dev/tty", 0, 1);
	removeFromUtmp();
],[
	cf_cv_have_utempter=yes],[
	cf_cv_have_utempter=no])
LIBS="$cf_save_LIBS"
])
if test "$cf_cv_have_utempter" = yes ; then
	AC_DEFINE(USE_UTEMPTER)
	LIBS="-lutempter $LIBS"
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for UTMP/UTMPX headers
AC_DEFUN([CF_UTMP],
[
AC_REQUIRE([CF_LASTLOG])

AC_CACHE_CHECK(for utmp implementation,cf_cv_have_utmp,[
	cf_cv_have_utmp=no
for cf_header in utmpx utmp ; do
cf_utmp_includes="
#include <sys/types.h>
#include <${cf_header}.h>
#define getutent getutxent
#ifdef USE_LASTLOG
#include <lastlog.h>	/* may conflict with utmpx.h on Linux */
#endif
"
	AC_TRY_COMPILE([$cf_utmp_includes],
	[struct $cf_header x;
	 char *name = x.ut_name; /* utmp.h and compatible definitions */
	],
	[cf_cv_have_utmp=$cf_header
	 break],
	[
	AC_TRY_COMPILE([$cf_utmp_includes],
	[struct $cf_header x;
	 char *name = x.ut_user; /* utmpx.h must declare this */
	],
	[cf_cv_have_utmp=$cf_header
	 break
	])])
done
])

if test $cf_cv_have_utmp != no ; then
	AC_DEFINE(HAVE_UTMP)
	test $cf_cv_have_utmp = utmpx && AC_DEFINE(UTMPX_FOR_UTMP)
	CF_UTMP_UT_HOST
	CF_UTMP_UT_NAME
	CF_UTMP_UT_XSTATUS
	CF_UTMP_UT_XTIME
	CF_UTMP_UT_SESSION
	CF_SYSV_UTMP
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if UTMP/UTMPX struct defines ut_host member
AC_DEFUN([CF_UTMP_UT_HOST],
[
if test $cf_cv_have_utmp != no ; then
AC_MSG_CHECKING(if utmp.ut_host is declared)
AC_CACHE_VAL(cf_cv_have_utmp_ut_host,[
	AC_TRY_COMPILE([
#include <sys/types.h>
#include <${cf_cv_have_utmp}.h>],
	[struct $cf_cv_have_utmp x; char *y = &x.ut_host[0]],
	[cf_cv_have_utmp_ut_host=yes],
	[cf_cv_have_utmp_ut_host=no])
	])
AC_MSG_RESULT($cf_cv_have_utmp_ut_host)
test $cf_cv_have_utmp_ut_host != no && AC_DEFINE(HAVE_UTMP_UT_HOST)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if UTMP/UTMPX struct defines ut_name member
AC_DEFUN([CF_UTMP_UT_NAME],
[
if test $cf_cv_have_utmp != no ; then
AC_CACHE_CHECK(if utmp.ut_name is declared,cf_cv_have_utmp_ut_name,[
	cf_cv_have_utmp_ut_name=no
cf_utmp_includes="
#include <sys/types.h>
#include <${cf_cv_have_utmp}.h>
#define getutent getutxent
#ifdef USE_LASTLOG
#include <lastlog.h>		/* may conflict with utmpx.h on Linux */
#endif
"
for cf_header in ut_name ut_user ; do
	AC_TRY_COMPILE([$cf_utmp_includes],
	[struct $cf_cv_have_utmp x;
	 char *name = x.$cf_header;
	],
	[cf_cv_have_utmp_ut_name=$cf_header
	 break])
done
])

case $cf_cv_have_utmp_ut_name in #(vi
no) #(vi
	AC_MSG_ERROR(Cannot find declaration for ut.ut_name)
	;;
ut_user)
	AC_DEFINE(ut_name,ut_user)
	;;
esac
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if UTMP/UTMPX struct defines ut_session member
AC_DEFUN([CF_UTMP_UT_SESSION],
[
if test $cf_cv_have_utmp != no ; then
AC_CACHE_CHECK(if utmp.ut_session is declared, cf_cv_have_utmp_ut_session,[
	AC_TRY_COMPILE([
#include <sys/types.h>
#include <${cf_cv_have_utmp}.h>],
	[struct $cf_cv_have_utmp x; long y = x.ut_session],
	[cf_cv_have_utmp_ut_session=yes],
	[cf_cv_have_utmp_ut_session=no])
])
if test $cf_cv_have_utmp_ut_session != no ; then
	AC_DEFINE(HAVE_UTMP_UT_SESSION)
fi
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for known variants on the UTMP/UTMPX struct's exit-status as reported
dnl by various people:
dnl
dnl	ut_exit.__e_exit (HPUX 11 - David Ellement, also in glibc2)
dnl	ut_exit.e_exit (SVR4)
dnl	ut_exit.ut_e_exit (os390 - Greg Smith)
dnl	ut_exit.ut_exit (Tru64 4.0f - Jeremie Petit, 4.0e - Tomas Vanhala)
dnl
dnl Note: utmp_xstatus is not a conventional compatibility definition in the
dnl system header files.
AC_DEFUN([CF_UTMP_UT_XSTATUS],
[
if test $cf_cv_have_utmp != no ; then
AC_CACHE_CHECK(for exit-status in $cf_cv_have_utmp,cf_cv_have_utmp_ut_xstatus,[
for cf_result in \
	ut_exit.__e_exit \
	ut_exit.e_exit \
	ut_exit.ut_e_exit \
	ut_exit.ut_exit
do
AC_TRY_COMPILE([
#include <sys/types.h>
#include <${cf_cv_have_utmp}.h>],
	[struct $cf_cv_have_utmp x; long y = x.$cf_result = 0],
	[cf_cv_have_utmp_ut_xstatus=$cf_result
	 break],
	[cf_cv_have_utmp_ut_xstatus=no])
done
])
if test $cf_cv_have_utmp_ut_xstatus != no ; then
	AC_DEFINE(HAVE_UTMP_UT_XSTATUS)
	AC_DEFINE_UNQUOTED(ut_xstatus,$cf_cv_have_utmp_ut_xstatus)
fi
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if UTMP/UTMPX struct defines ut_xtime member
AC_DEFUN([CF_UTMP_UT_XTIME],
[
if test $cf_cv_have_utmp != no ; then
AC_CACHE_CHECK(if utmp.ut_xtime is declared, cf_cv_have_utmp_ut_xtime,[
	AC_TRY_COMPILE([
#include <sys/types.h>
#include <${cf_cv_have_utmp}.h>],
	[struct $cf_cv_have_utmp x; long y = x.ut_xtime = 0],
	[cf_cv_have_utmp_ut_xtime=yes],
	[AC_TRY_COMPILE([
#include <sys/types.h>
#include <${cf_cv_have_utmp}.h>],
	[struct $cf_cv_have_utmp x; long y = x.ut_tv.tv_sec],
	[cf_cv_have_utmp_ut_xtime=define],
	[cf_cv_have_utmp_ut_xtime=no])
	])
])
if test $cf_cv_have_utmp_ut_xtime != no ; then
	AC_DEFINE(HAVE_UTMP_UT_XTIME)
	if test $cf_cv_have_utmp_ut_xtime = define ; then
		AC_DEFINE(ut_xtime,ut_tv.tv_sec)
	fi
fi
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Use AC_VERBOSE w/o the warnings
AC_DEFUN([CF_VERBOSE],
[test -n "$verbose" && echo "	$1" 1>&AC_FD_MSG
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for Xaw (Athena) libraries
dnl
AC_DEFUN([CF_X_ATHENA],
[AC_REQUIRE([CF_X_TOOLKIT])
cf_x_athena=${cf_x_athena-Xaw}

AC_ARG_WITH(Xaw3d,
	[  --with-Xaw3d            link with Xaw 3d library],
	[cf_x_athena=Xaw3d])

AC_ARG_WITH(neXtaw,
	[  --with-neXtaw           link with neXT Athena library],
	[cf_x_athena=neXtaw])


AC_CHECK_LIB(Xext,XextCreateExtension,
	[LIBS="-lXext $LIBS"])

cf_x_athena_lib=""

CF_X_ATHENA_CPPFLAGS($cf_x_athena)
CF_X_ATHENA_LIBS($cf_x_athena)
])dnl
dnl ---------------------------------------------------------------------------
dnl Normally invoked by CF_X_ATHENA, with $1 set to the appropriate flavor of
dnl the Athena widgets, e.g., Xaw, Xaw3d, neXtaw.
AC_DEFUN([CF_X_ATHENA_CPPFLAGS],
[
cf_x_athena_root=ifelse($1,,Xaw,$1)
cf_x_athena_include=""

for cf_path in default \
	/usr/contrib/X11R6 \
	/usr/contrib/X11R5 \
	/usr/lib/X11R5 \
	/usr/local
do
	if test -z "$cf_x_athena_include" ; then
		cf_save="$CPPFLAGS"
		cf_test=X11/$cf_x_athena_root/SimpleMenu.h
		if test $cf_path != default ; then
			CPPFLAGS="-I$cf_path/include $cf_save"
			AC_MSG_CHECKING(for $cf_test in $cf_path)
		else
			AC_MSG_CHECKING(for $cf_test)
		fi
		AC_TRY_COMPILE([
#include <X11/Intrinsic.h>
#include <$cf_test>],[],
			[cf_result=yes],
			[cf_result=no])
		AC_MSG_RESULT($cf_result)
		if test "$cf_result" = yes ; then
			cf_x_athena_include=$cf_path
			break
		else
			CPPFLAGS="$cf_save"
		fi
	fi
done

if test -z "$cf_x_athena_include" ; then
	AC_MSG_WARN(
[Unable to successfully find Athena header files with test program])
elif test "$cf_x_athena_include" != default ; then
	CPPFLAGS="$CPPFLAGS -I$cf_x_athena_include"
fi
])
dnl ---------------------------------------------------------------------------
dnl Normally invoked by CF_X_ATHENA, with $1 set to the appropriate flavor of
dnl the Athena widgets, e.g., Xaw, Xaw3d, neXtaw.
AC_DEFUN([CF_X_ATHENA_LIBS],
[AC_REQUIRE([CF_X_TOOLKIT])
cf_x_athena_root=ifelse($1,,Xaw,$1)
cf_x_athena_lib=""

for cf_path in default \
	/usr/contrib/X11R6 \
	/usr/contrib/X11R5 \
	/usr/lib/X11R5 \
	/usr/local
do
	for cf_lib in \
		"-l$cf_x_athena_root -lXmu" \
		"-l$cf_x_athena_root -lXpm -lXmu" \
		"-l${cf_x_athena_root}_s -lXmu_s"
	do
		if test -z "$cf_x_athena_lib" ; then
			cf_save="$LIBS"
			cf_test=XawSimpleMenuAddGlobalActions
			if test $cf_path != default ; then
				LIBS="-L$cf_path/lib $cf_lib $LIBS"
				AC_MSG_CHECKING(for $cf_lib in $cf_path)
			else
				LIBS="$cf_lib $LIBS"
				AC_MSG_CHECKING(for $cf_test in $cf_lib)
			fi
			AC_TRY_LINK([],[$cf_test()],
				[cf_result=yes],
				[cf_result=no],
				[$X_PRE_LIBS $LIBS $X_EXTRA_LIBS])
			AC_MSG_RESULT($cf_result)
			if test "$cf_result" = yes ; then
				cf_x_athena_lib="$cf_lib"
				break
			else
				LIBS="$cf_save"
			fi
		fi
	done
done

if test -z "$cf_x_athena_lib" ; then
	AC_ERROR(
[Unable to successfully link Athena library (-l$cf_x_athena_root) with test program])
fi

CF_UPPER(cf_x_athena_LIBS,HAVE_LIB_$cf_x_athena)
AC_DEFINE_UNQUOTED($cf_x_athena_LIBS)
])
dnl ---------------------------------------------------------------------------
dnl Check for X freetype libraries (XFree86 4.x)
AC_DEFUN([CF_X_FREETYPE],
[
cf_freetype_libs="-lXft -lfreetype -lXrender -lXrender"
AC_CACHE_CHECK(for X FreeType libraries,cf_cv_x_freetype,[

cf_save_LIBS="$LIBS"
LIBS="$cf_freetype_libs $LIBS"

AC_TRY_LINK([
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xft/Xft.h>],[
	XftPattern  *pat = XftNameParse ("name");
	],[cf_cv_x_freetype=yes],[cf_cv_x_freetype=no])
	LIBS="$cf_save_LIBS"
])
if test "$cf_cv_x_freetype" = yes ; then
	LIBS="$cf_freetype_libs $LIBS"
	AC_DEFINE(XRENDERFONT)
else
	CPPFLAGS=`echo "$CPPFLAGS" | sed -e s/-DXRENDERFONT//`
fi
])
dnl ---------------------------------------------------------------------------
dnl Check for X Toolkit libraries
dnl
AC_DEFUN([CF_X_TOOLKIT],
[
AC_REQUIRE([CF_CHECK_CACHE])
# We need to check for -lsocket and -lnsl here in order to work around an
# autoconf bug.  autoconf-2.12 is not checking for these prior to checking for
# the X11R6 -lSM and -lICE libraries.  The resultant failures cascade...
# 	(tested on Solaris 2.5 w/ X11R6)
SYSTEM_NAME=`echo "$cf_cv_system_name"|tr ' ' -`
cf_have_X_LIBS=no
case $SYSTEM_NAME in
irix[[56]]*) ;;
clix*)
	# FIXME: modify the library lookup in autoconf to
	# allow _s.a suffix ahead of .a
	AC_CHECK_LIB(c_s,open,
		[LIBS="-lc_s $LIBS"
	AC_CHECK_LIB(bsd,gethostname,
		[LIBS="-lbsd $LIBS"
	AC_CHECK_LIB(nsl_s,gethostname,
		[LIBS="-lnsl_s $LIBS"
	AC_CHECK_LIB(X11_s,XOpenDisplay,
		[LIBS="-lX11_s $LIBS"
	AC_CHECK_LIB(Xt_s,XtAppInitialize,
		[LIBS="-lXt_s $LIBS"
		 cf_have_X_LIBS=Xt
		]) ]) ]) ]) ])
	;;
*)
	AC_CHECK_LIB(socket,socket)
	AC_CHECK_LIB(nsl,gethostname)
	;;
esac

if test $cf_have_X_LIBS = no ; then
	AC_PATH_XTRA
	LDFLAGS="$LDFLAGS $X_LIBS"
	CF_CHECK_CFLAGS($X_CFLAGS)
	AC_CHECK_LIB(X11,XOpenDisplay,
		[LIBS="-lX11 $LIBS"],,
		[$X_PRE_LIBS $LIBS $X_EXTRA_LIBS])
	AC_CHECK_LIB(Xt, XtAppInitialize,
		[AC_DEFINE(HAVE_LIBXT)
		 cf_have_X_LIBS=Xt
		 LIBS="-lXt $X_PRE_LIBS $LIBS"],,
		[$X_PRE_LIBS $LIBS $X_EXTRA_LIBS])
else
	LDFLAGS="$LDFLAGS $X_LIBS"
	CF_CHECK_CFLAGS($X_CFLAGS)
fi

if test $cf_have_X_LIBS = no ; then
	AC_WARN(
[Unable to successfully link X Toolkit library (-lXt) with
test program.  You will have to check and add the proper libraries by hand
to makefile.])
fi
])dnl
