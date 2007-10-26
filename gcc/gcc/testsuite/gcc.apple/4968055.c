/* APPLE LOCAL file 4968055 */
/* { dg-do compile { target i?86-*-darwin* } } */ 
/* { dg-options "-O1" } */
typedef struct {
  int _ignored;
  unsigned int first_14:14;
  unsigned int second_14:14;
} nsv;
void
check_2nd_bitfield (nsv *this)
{
  if (this->second_14 > 0)
    foo();
}
/* { dg-final { scan-assembler "testl.*4\\(%" } } */
/* { dg-final { scan-assembler-not "testl.*5\\(%" } } */
