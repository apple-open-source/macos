/* APPLE LOCAL file mainline 2883585 2005-04-20 */
/* Test -Wint-to-pointer-cast - on by default.  */
/* Origin: Joseph Myers <joseph@codesourcery.com> */
/* { dg-do compile } */
/* { dg-options "" } */

char c;

void *
f (void)
{
  return (void *) c; /* { dg-warning "warning: cast to pointer from integer of different size" } */
}
