#include <stdio.h>
#include <mach-o/dyld.h>
#include <unistd.h>

 /* This program dynamically loads a shared library (dylib) at run-time,
    looks up a function called foo(), and calls that function. */

#ifndef LIBNAME
#define LIBNAME NULL
#endif

main (int argc, char **argv)
{
  char *libname = NULL;

  if (LIBNAME)
    libname = LIBNAME;
  else if (argc == 2 && argv[1] != NULL)
    libname = argv[1];
    
  if (libname)
    {
      if (! NSAddLibrary (libname))
        {
          fprintf (stderr, "Unable to load `%s' library.\n", libname);
          exit (1);
        }

      if (NSIsSymbolNameDefined ("_foo")) 
        {
          int (*addr)(void) = 
            NSAddressOfSymbol (NSLookupAndBindSymbol ("_foo"));
          printf ("foo is resolved to address %lx\n", (unsigned long) addr);
          if (addr)
           addr ();
        }
  } else {
      fprintf (stderr, "No library specified.\n");
  }
}
