#import <Foundation/Foundation.h>
#include <mach-o/dyld.h>

@interface MyClass: NSObject
{
  /* secret! */
}
+ newWithArg: arg;
- takeArg: arg;
- randomFunc;
@end

#ifndef LIBNAME
#define LIBNAME NULL
#endif

int main (int argc, const char * argv[]) {
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    const char *libname = NULL;

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
        if (NSIsSymbolNameDefined ("_return_an_object"))
          {
            id (*get_obj)(void) = NSAddressOfSymbol 
                                (NSLookupAndBindSymbol ("_return_an_object"));
             id object = get_obj();
             [object takeArg:@"hi there"];
             [object randomFunc];
          }
      } 
    else 
      {
        fprintf (stderr, "USER PROGRAM ERROR: No library specified.\n");
      }

    [pool release];
    return 0;
}
