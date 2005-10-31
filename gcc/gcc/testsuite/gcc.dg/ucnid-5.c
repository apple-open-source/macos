/* APPLE LOCAL begin mainline UCNs 2005-04-17 3892809 */
/* { dg-do run } */
/* { dg-options "-std=c99 -fdollars-in-identifiers" } */
void abort (void);

int a$b(void) { return 1; }

int main (void)
{
  
  if (a\u0024b() != 1)
    abort ();
  
  return 0;
}
/* APPLE LOCAL end mainline UCNs 2005-04-17 3892809 */
