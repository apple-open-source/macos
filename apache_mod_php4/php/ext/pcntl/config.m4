dnl
dnl $Id: config.m4,v 1.7 2002/03/12 16:49:27 sas Exp $
dnl

dnl Process Control (pcntl) extentsion --EXPERIMENTAL--
dnl TODO - Add platform checks 

PHP_ARG_ENABLE(pcntl, whether to enable pcntl support,
[  --enable-pcntl          Enable experimental pcntl support (CLI/CGI only)])

if test "$PHP_PCNTL" != "no"; then
 
  AC_CHECK_FUNCS(fork, [ AC_DEFINE(HAVE_FORK,1,[ ]) ], [ AC_MSG_ERROR(pcntl: fork() not supported by this platform) ])
  AC_CHECK_FUNCS(waitpid, [ AC_DEFINE(HAVE_WAITPID,1,[ ]) ], [ AC_MSG_ERROR(pcntl: fork() not supported by this platform) ])
  AC_CHECK_FUNCS(sigaction, [ AC_DEFINE(HAVE_SIGACTION,1,[ ]) ], [ AC_MSG_ERROR(pcntl: sigaction() not supported by this platform) ])
  
  PHP_NEW_EXTENSION(pcntl, pcntl.c php_signal.c, $ext_shared, cli)
fi
