/* APPLE LOCAL begin mainline 2005-09-01 3449986 */
/* Basic test of the -mmacosx-version-min option.  */

/* { dg-options "-mmacosx-version-min=10.1" } */
/* { dg-do link { target *-*-darwin* } } */

int main()
{
  return 0;
}

/* APPLE LOCAL end mainline 2005-09-01 3449986 */
