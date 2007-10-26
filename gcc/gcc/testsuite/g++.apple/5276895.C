/* APPLE LOCAL file 5276995 */
/* { dg-do run } */
#include <stdio.h>
#include <stdlib.h>
unsigned short int __attribute__ ((__noinline__))
check(unsigned short int i)
{
  /* printf ("should print 0x0000: 0x%04X\n", (unsigned int)i); */
  if (i != 0)
    abort ();
  return i+1;
}
void __attribute__ ((__noinline__))
add14(unsigned short int nextID)
{
  struct {
    unsigned short int id: 14;
  } hdr;
  hdr.id = nextID;
  do {
    hdr.id++;
    /* printf ("should print 0x0000: 0x%04X\n", (unsigned int)hdr.id)); */
    if (check (hdr.id))
      break;
  } while (1);
}
int
main(int argc, char *argv[])
{
  add14 (0x3FFF);
  return 0;
}
