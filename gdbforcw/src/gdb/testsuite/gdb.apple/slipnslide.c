#include <stdio.h>
#include <mach-o/dyld.h>

main ()
{

  fputs ("hi\n", stdout);
  fputs ("there\n", stdout);
  puts ("test");     /* this call will always go to libSystem, even though it is defined in mylib.dylib */

  if (NSIsSymbolNameDefined ("_foo")) {
    int (*addr)(void) = 
         NSAddressOfSymbol (NSLookupAndBindSymbol ("_foo"));
    if (addr) {
      addr ();
    }
  }

}
