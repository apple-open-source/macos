#include <stdio.h>
#include "single-def.h"

int 
main ()
{
  test1abcdefg t1;

  if (t1.size () || t1.m_uint)
    printf ("t1 size is %zd\n", t1.size ());

  return 0; /* good place to stop in single-def-main.cc */
}
