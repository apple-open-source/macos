#include <stdio.h>
#include <mach-o/dyld.h>

main ()
{

  printf ("hi\n");
  printf ("there\n");
  puts ("test");     /* this call will always go to libSystem */

  if (NSIsSymbolNameDefined ("_foo")) {
    int (*addr)(void) = 
         NSAddressOfSymbol (NSLookupAndBindSymbol ("_foo"));
    if (addr) {
      addr ();
    }
  }

}
