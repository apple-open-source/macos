PHP_ARG_WITH(ircg, for ircg support,
[  --with-ircg             Include ircg support])

if test "$PHP_IRCG" != "no"; then
  AC_ADD_LIBRARY(st)
  AC_ADD_LIBRARY_WITH_PATH(ircg, $PHP_IRCG/lib)
  AC_ADD_INCLUDE($PHP_IRCG/include)
  AC_DEFINE(HAVE_IRCG, 1, [ ])
  PHP_EXTENSION(ircg, $ext_shared)
fi
