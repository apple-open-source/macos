#include <stdio.h>

extern int global_var;

main ()
{
  puts ("Hi, I am in main().");
  bar (); /* 1 */
  bar (); /* 2 */
  bar (); /* 3 */
  bar (); /* 4 */
  bar (); /* 5 */
  bar (); /* 6 */
  bar (); /* 7 */
  bar (); /* 8 */
  bar (); /* 9 */
  bar (); /* 10 */
  bar (); /* 11 */
  printf ("global_var is %d\n", global_var);
}

int
slurry (char , int , double );

fred ()
{
  printf ("I am in fred() over in main(), global_var is %d.\n", global_var);

  slurry ('a', 1, 1.0);
}
