sinclude(../bfd/acinclude.m4)

dnl sinclude(../gettext.m4) already included in bfd/acinclude.m4
ifelse(yes,no,[
AC_DEFUN([CY_WITH_NLS],)
AC_SUBST(INTLLIBS)
])

dnl AM_INSTALL_LIBBFD already included in bfd/acinclude.m4
ifelse(yes,no,[
AC_DEFUN([AM_INSTALL_LIBBFD],)
AC_SUBST(bfdlibdir)
AC_SUBST(bfdincludedir)
])
