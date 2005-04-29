#
# Pull in the standard Tcl autoconf macros.
# If you don't have the "tclconfig" subdirectory, it is a dependent CVS
# module. Either "cvs -d <root> checkout tclconfig" right here, or 
# re-checkout the thread module
#
builtin(include,tclconfig/tcl.m4)
builtin(include,aolserver.m4)

#
# Handle the "--enable-gdbm" option for linking-in
# the gdbm-based peristent store for shared arrays.
# It tries to locate gdbm files in couple of standard
# system directories and/or common install locations.
#

AC_DEFUN(TCLTHREAD_ENABLE_GDBM, [
    AC_MSG_CHECKING([wether to link with the GNU gdbm library])
    AC_ARG_ENABLE(gdbm,
	[  --enable-gdbm           link with optional gdbm support [--disable-gdbm]],
	[gdbm_ok=$enableval], [gdbm_ok=no])

    if test "${enable_gdbm+set}" = set; then
        enableval="$enable_gdbm"
        gdbm_ok=$enableval
    else
        gdbm_ok=no
    fi
    if test "$gdbm_ok" = "yes" ; then
        for i in `ls -d ${exec_prefix}/lib 2>/dev/null` \
                `ls -d ${prefix}/lib 2>/dev/null` \
                `ls -d /usr/local/lib 2>/dev/null` \
                `ls -d /usr/lib 2>/dev/null` ; do
            if test -f "$i/libgdbm*" ; then
                libdir=`(cd $i; pwd)`
                break
            fi
        done
        for i in `ls -d ${prefix}/include 2>/dev/null` \
                `ls -d /usr/local/include 2>/dev/null` \
                `ls -d /usr/include 2>/dev/null` ; do
            if test -f "$i/gdbm.h" ; then
                incldir=`(cd $i; pwd)`
                break
            fi
        done
        if test x"$libdir" = x -o x"$incldir" = x ; then
            AC_MSG_ERROR([can't, because no gdbm found on this system])
        else
            AC_MSG_RESULT([yes])
            AC_DEFINE(USE_GDBM)
            CFLAGS="${CFLAGS} -I$incldir"
            LIBS="${LIBS} -L$libdir -lgdbm"
        fi
    else
        AC_MSG_RESULT([no])
    fi
])

# EOF