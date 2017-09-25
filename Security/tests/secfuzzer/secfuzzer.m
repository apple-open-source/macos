//
//  secfuzzer.c
//  Security
//

#import <Foundation/Foundation.h>
#import <stdio.h>
#import <dlfcn.h>
#import <err.h>

int
main(int argc, char **argv)
{
    int (*function)(const void *data, size_t len) = NULL;

    if (argc < 2)
        err(1, "%s library funcation", getprogname());

    char *libraryName = argv[1];
    char *funcationName = argv[2];
    void *library;

    library = dlopen(libraryName, RTLD_NOW);
    if (library == NULL)
        errx(1, "failed to open %s: %s", libraryName, dlerror());


    function = dlsym(library, funcationName);
    if (function == NULL)
        errx(1, "didn't find %s in %s: %s", funcationName, libraryName, dlerror());

    argc -= 3;
    argv += 3;

    while (argc > 0) {
        @autoreleasepool {
            NSError *error = NULL;
            NSData *data = [NSData dataWithContentsOfFile:[NSString stringWithUTF8String:argv[0]] options:0 error:&error];
            if (data == NULL)
                NSLog(@"%s: %@", argv[0], error);

            function([data bytes], [data length]);
        }
        argv++;
        argc--;
    }
    return 0;
}
