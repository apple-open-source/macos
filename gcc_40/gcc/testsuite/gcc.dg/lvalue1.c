/* PR c/5225 */
/* { dg-do compile } */
/* APPLE LOCAL non lvalue assign */
/* { dg-options "-fno-non-lvalue-assign" } */

int main()
{
  int i;
  +i = 1;	/* { dg-error "invalid lvalue in assignment" } */
  return 0;
}
