sinclude(../bfd/acinclude.m4)

## for AC_PROG_LEX and AC_DECL_YYTEXT
## Alexandre Oliva <oliva@dcc.unicamp.br>
##
## We need to override the installed aclocal/lex.m4 because of a bug in
## this definition in the recommended automake snapshot of 000227:
## There were double-quotes around ``$missing_dir/missing flex'' which was
## bad since aclocal wraps it in double-quotes.

dnl AM_PROG_LEX
dnl Look for flex, lex or missing, then run AC_PROG_LEX and AC_DECL_YYTEXT
AC_DEFUN(AM_PROG_LEX,
[missing_dir=ifelse([$1],,`cd $ac_aux_dir && pwd`,$1)
AC_CHECK_PROGS(LEX, flex lex, [$missing_dir/missing flex])
AC_PROG_LEX
AC_DECL_YYTEXT])
