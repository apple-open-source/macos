AC_DEFUN(DJ_AC_STL, [
AC_MSG_CHECKING(for STL versions)
AC_CACHE_VAL(ac_cv_stl,[
  AC_LANG_CPLUSPLUS
  AC_TRY_COMPILE([#include <iostream>], [
  using namespace std;
  char bbuuff[5120];
  cout.rdbuf()->pubsetbuf(bbuuff, 5120); ],
  ac_cv_stl=v3
  ,
  ac_cv_stl=v2
  ),
])

AC_LANG_C
if test x"${ac_cv_stl}" != x"v2" ; then  
  AC_MSG_RESULT(v3)
  AC_DEFINE(HAVE_STL3)
else
  AC_MSG_RESULT(v2)
fi
])

AC_DEFUN(DJ_AC_PATH_TCLSH, [
dirlist=".. ../../ ../../../ ../../../../ ../../../../../ ../../../../../../ ../
../../../../../.. ../../../../../../../.. ../../../../../../../../.. ../../../..
/../../../../../.."
no_itcl=true
AC_MSG_CHECKING(for the tclsh program)
AC_ARG_WITH(tclinclude, [  --with-tclinclude       directory where tcl header
s are], with_tclinclude=${withval})
AC_CACHE_VAL(ac_cv_path_tclsh,[
dnl first check to see if --with-itclinclude was specified
if test x"${with_tclinclude}" != x ; then
  if test -f ${with_tclinclude}/tclsh ; then
    ac_cv_path_tclsh=`(cd ${with_tclinclude}; pwd)`
  elif test -f ${with_tclinclude}/src/tclsh ; then
    ac_cv_path_tclsh=`(cd ${with_tclinclude}/src; pwd)`
  else
    AC_MSG_ERROR([${with_tclinclude} directory doesn't contain tclsh])
  fi
fi
])

dnl next check in private source directory
dnl since ls returns lowest version numbers first, reverse its output
if test x"${ac_cv_path_tclsh}" = x ; then
    dnl find the top level Itcl source directory
    for i in $dirlist; do
        if test -n "`ls -dr $srcdir/$i/tcl* 2>/dev/null`" ; then
            tclpath=$srcdir/$i
            break
        fi
    done

    dnl find the exact Itcl source dir. We do it this way, cause there
    dnl might be multiple version of Itcl, and we want the most recent one.
    for i in `ls -dr $tclpath/tcl* 2>/dev/null ` ; do
        if test -f $i/src/tclsh ; then
          ac_cv_path_tclsh=`(cd $i/src; pwd)`/tclsh
          break
        fi
    done
fi

dnl see if one is installed
if test x"${ac_cv_path_tclsh}" = x ; then
   AC_MSG_RESULT(none)
   AC_PATH_PROG(tclsh, tclsh)
else
   AC_MSG_RESULT(${ac_cv_path_tclsh})
fi
TCLSH="${ac_cv_path_tclsh}"
AC_SUBST(TCLSH)
])


AC_DEFUN(DJ_AC_PATH_DOCBOOK, [
dirlist=".. ../../ ../../.. ../../../.. ../../../../.. ../../../../../.. ../../../../../../.. ../../../../../../../.. ../../../../../../../../.. ../../../../../../../../../.."
AC_MSG_CHECKING(for docbook tools)
AC_ARG_WITH(oskith, [  --with-docbook       directory where the db2 sgml tools are], with_docbook=${withval})
AC_CACHE_VAL(ac_cv_c_docbook,[
dnl first check to see if --with-docbook was specified
if test x"${with_docbook}" != x ; then
  if test -f ${with_docbook}/db2html ; then
    ac_cv_c_docbook=`(cd ${with_docbook}; pwd)`
  else
    AC_MSG_ERROR([${with_docbook} directory doesn't contain SGML tools])
  fi
fi
])
if test x"${ac_cv_c_docbook}" = x ; then
    for i in $ac_default_prefix/bin /usr/local/bin $OSKITHDIR/../bin /usr/bin /bin /opt /home; do
	dnl See is we have an SGML tool in that directory.
	if test -f $i/db2html ; then
	    ac_cv_c_docbook=$i
	    break
	fi
    done
fi

if test x"${ac_cv_c_docbook}" = x ; then
    AC_MSG_RESULT(none)
else
    DOCBOOK="${ac_cv_c_docbook}"
    AC_MSG_RESULT(${ac_cv_c_docbook})
fi

AC_SUBST(DOCBOOK)
])

