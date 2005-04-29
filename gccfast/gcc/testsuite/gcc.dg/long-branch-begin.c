/* { dg-do run onestep { long-branch-middle.c long-branch-end.c } { target powerpc-*-darwin* } } */
/* { dg-options "-mlongcall" } */

/* Test for rs6000 -mlong-branch option.
  Three-file testcase, spread across
  long-branch-begin.c (this file),
  long-branch-middle.c (allocates huge text area to call across)
  long-branch-end.c (end() function)
*/

int
main (int argc, const char * argv[])
{
  if (end() == 42)
    {
      /* printf ("Success !!\n"); */
      exit (0);
    }
  /* printf ("FAILURE! :-(\n"); */
  abort ();
}

int
begin ()
{
  return 42;
}
