/* APPLE LOCAL file mainline 2883585 2005-04-20 */
/* Test -Wpointer-to-int-cast - on by default.  */
/* Origin: Joseph Myers <joseph@codesourcery.com> */
/* { dg-do compile } */
/* { dg-options "" } */

void *p;

char
f (void)
{
  return (char) p; /* { dg-warning "warning: cast from pointer to integer of different size" } */
}
