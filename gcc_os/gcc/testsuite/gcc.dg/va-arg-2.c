/* <varargs.h> is not supported anymore, but we do install a stub
   file which issues an #error telling the user to convert their code.  */

/* { dg-do compile } */

#include <varargs.h>  /* { dg-bogus "varargs.h" "missing file" } */

/* { dg-error "" "In file included from" { target *-*-* } 6 } */

/* APPLE LOCAL BEGIN - varargs.h changes to line numbers hartoog@apple.com */
/* { dg-error "no longer implements" "#error 1" { target *-*-* } 10 } */
/* { dg-error "Revise your code" "#error 2" { target *-*-* } 11 } */
/* APPLE LOCAL END - varargs.h changes to line numbers hartoog@apple.com */

int x;  /* prevent empty-source-file warning */
