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
