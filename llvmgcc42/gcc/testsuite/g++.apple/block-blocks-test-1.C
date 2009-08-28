/* APPLE LOCAL file 5932809 */
/* { dg-options "-fblocks" } */
/* { dg-do compile } */

__block  int X; /* { dg-warning "__block attribute is only allowed on local variables - ignored" } */

int foo(__block int param) { /* { dg-warning "__block attribute can be specified on variables only - ignored" } */
  __block int OK = 1;

  extern __block double extern_var;	/* { dg-warning "__block attribute is only allowed on local variables - ignored" } */
  if (X) {
	static __block char * pch;	/* { dg-warning "__block attribute is only allowed on local variables - ignored" } */	
  }
  return OK - 1;
}
