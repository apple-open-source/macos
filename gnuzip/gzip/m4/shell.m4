#serial 4

# AC_PROG_SHELL
# -------------
# Check for a working (i.e. POSIX-compatible) shell.
# Written by Paul Eggert <eggert@twinsun.com>,
# from an idea suggested by Albert Chin-A-Young <china@thewrittenword.com>.
AC_DEFUN([AC_PROG_SHELL],
  [AC_MSG_CHECKING([for a POSIX-compliant shell])
   AC_CACHE_VAL(ac_cv_path_shell,
     [ac_command='
	# Test the noclobber option, using the portable POSIX.2 syntax.
	set -C
	rm -f conftest.c
	>conftest.c || exit
	>|conftest.c || exit
	!>conftest.c || exit
      '
      ac_cv_path_shell=no

      case $SHELL in
      /*)
	rm -f conftest.c
	if ("$SHELL" -c "$ac_command") 2>/dev/null; then
	  ac_cv_path_shell=$SHELL
	fi;;
      esac

      case $ac_cv_path_shell in
      no)
	# Prefer shells that are more likely to be installed in the
	# same place on all hosts of this platform.  Therefore, prefer
	# shells in /bin and /usr/bin to shells in the installer's
	# PATH.  Also, loop through PATH first and then through
	# shells, since less-"nice" shells in /bin and /usr/bin are
	# more likely to be installed than "nicer" shells elsewhere.
	as_save_IFS=$IFS; IFS=:
	for as_dir in /bin /usr/bin $PATH
	do
	  IFS=$as_save_IFS
	  case $as_dir in
	  /*)
	    for ac_base in sh bash ksh sh5; do
	      rm -f conftest.c
	      if ("$as_dir/$ac_base" -c "$ac_command") 2>/dev/null; then
		ac_cv_path_shell=$as_dir/$ac_base
		break
	      fi
	    done
	    case $ac_cv_path_shell in
	    /*) break;;
	    esac;;
	  esac
	done
	rm -f conftest.c;;
      esac])
   AC_MSG_RESULT($ac_cv_path_shell)
   SHELL=$ac_cv_path_shell
   if test "$SHELL" = no; then
     SHELL=/bin/sh
     AC_MSG_WARN([using $SHELL, even though it does not conform to POSIX])
   fi
   AC_SUBST(SHELL)])
