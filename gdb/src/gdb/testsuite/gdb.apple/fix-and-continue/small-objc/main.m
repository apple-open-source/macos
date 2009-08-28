#import <Foundation/Foundation.h>
#include <mach-o/dyld.h>
#import "class.h"

int main (int argc, const char * argv[]) {
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    char *fn = "class-bundlized.o2";
    NSObjectFileImage objfile_ref;
    NSObjectFileImageReturnCode retval1;
    NSModule retval2;
  

    /* Create a MyClass object given the original program */

    id first_var = [MyClass newWithArg:@"first string"];
    printf ("first_var says: ");
    [first_var sayHello];

    /* fix the program - load a changed MyClass */

    printf ("Fix in class-bundlized.o version #2 here.\n");
#if 0
    retval1 = NSCreateObjectFileImageFromFile (fn, &objfile_ref);
    retval2 = NSLinkModule (objfile_ref, fn, NSLINKMODULE_OPTION_PRIVATE);
#endif

    /* See how the old object behaves */

    printf ("first_var says: ");
    [first_var sayHello];


    /* Can we create a new object?  When we call sayHello will we get the
       old "hi v1" behavior, or the new "hi v2" behavior? */

    id second_var = [MyClass newWithArg:@"second string"];

    printf ("second_var says: ");
    [second_var sayHello];

    /* Code from kledzik - clear class cache and create new object */

    Class myCls = objc_getClass("MyClass");
    id third_var = [myCls newWithArg:@"third string"];

    printf ("third_var says: ");
    [third_var sayHello];

    printf ("Fix in class-bundlized.o version #3 here.\n");

    /* And one last new variable, a full pass over them all. */

    Class myCls2 = objc_getClass("MyClass");
    id fourth_var = [myCls2 newWithArg:@"fourth string"];

    printf ("first_var says: ");
    [first_var sayHello];

    printf ("second_var says: ");
    [second_var sayHello];

    printf ("third_var says: ");
    [third_var sayHello];

    printf ("fourth_var says: ");
    [fourth_var sayHello];

    /* Put a breakpoint on sayHello, see if we can stop there.  */
    [third_var sayHello];

    printf ("first_var isa is %p\n", first_var->isa);
    printf ("second_var isa is %p\n", second_var->isa);
    printf ("third_var isa is %p\n", third_var->isa);
    printf ("fourth_var isa is %p\n", fourth_var->isa);

    [pool release];
    return 0;
}
