# Check to see if we use dir\file name conventtion
# If so, set macro HAVE_DOS_FILE_NAMES 
dnl AC_DOSFILE()
AC_DEFUN(AC_DOSFILE,
[AC_CACHE_CHECK([for dos file convention], ac_cv_dosfile,
[if test -d ".\."; then
   ac_cv_dosfile=yes
   AC_DEFINE(HAVE_DOS_FILE_NAMES)
else
   ac_cv_dosfile=no
fi
])])
