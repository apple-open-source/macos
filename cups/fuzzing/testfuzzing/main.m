//
//  main.m
//  testfuzzing
//
//  Created by steve on 3/10/21.
//

#import <Foundation/Foundation.h>
#include <dirent.h>
#include <sys/stat.h>

#import "config.h"

static Boolean gVerbose = false;
static dispatch_group_t gGroup;
static Boolean gParallel = false;
static int (*gFuzzer)(Boolean verbose, const uint8_t* data, size_t len);

extern int _ipp_fuzzing(Boolean verbose, const uint8_t* data, size_t len);
extern int _asn1_fuzzing(Boolean verbose, const uint8_t* data, size_t len);

static void fuzzFile(const char* path);

static BOOL needsSlash(const char* path)
{
    unsigned long len = strlen(path);
    return (len == 0 || len >= PATH_MAX || path[len - 1] != '/');
}

static void failErr(const char* file, const char* msg, int err)
{
    NSLog(@"Error: %s for %s (%d %s)\n", msg, file, err, strerror(err));
    exit(-1);
}

static void fuzzDir(const char* path)
{
    DIR* dir = opendir(path);

    if (dir == NULL)
        failErr(path, "not a directory", ENOTDIR);
    else {
        const char* slash = needsSlash(path)? "/" : "";

        struct dirent* ent;

        while ((ent = readdir(dir)) != NULL) {
            const char* name = ent->d_name;
            if (strlen(name) != ent->d_namlen) {
                continue;
            }
            if (strcmp(name, ".") == 0) {
                continue;
            }
            if (strcmp(name, "..") == 0) {
                continue;
            }

            char filePath[PATH_MAX];
            snprintf(filePath, PATH_MAX, "%s%s%.*s", path, slash, ent->d_namlen, ent->d_name);

            if (ent->d_type == DT_DIR) {
                fuzzDir(filePath);
            } else {
                fuzzFile(filePath);
            }
        }

        closedir(dir);
    }
}

static void fuzzFileWithSize0(const char* path, size_t size)
{
//    if (size > 1024 * 1024) {
//        failErr(path, "file too big (> 1M)", ENOMEM);
//    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        failErr(path, "can't open", errno);
    }

    uint8_t* p = (uint8_t*) malloc(size);
    size_t ctRead = (size_t) read(fd, p, size);
    int errno_r  = errno;
    close(fd);

    if (ctRead != size) {
        failErr(path, "can't read", errno_r);
    }

    if (gVerbose) {
        NSLog(@"Read [%s] with %zu bytes", path, size);
    }

    (*gFuzzer)(gVerbose, p, size);

    free((char*) p);
}

void fuzzFileWithSize(const char* path, size_t size)
{
    if (gGroup == NULL)
        fuzzFileWithSize0(path, size);
    else {
        char* p = strdup(path);
        dispatch_group_async(gGroup, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            fuzzFileWithSize0(p, size);
            free(p);
        });
    }
}

static void fuzzFile(const char* path)
{
    struct stat sb;
    if (stat(path, &sb) < 0) {
        failErr(path, "can't stat", errno);
    }

    if (S_ISDIR(sb.st_mode))
        fuzzDir(path);
    else {
        fuzzFileWithSize(path, sb.st_size);
    }
}

static void fuzzStdin0()
{
    NSMutableData* d = [NSMutableData new];

    while (true) {
        char tmp[1024];
        ssize_t ctRead = read(STDIN_FILENO, tmp, sizeof(tmp));
        if (ctRead <= 0)
            break;
        [d appendBytes:tmp length:ctRead];

        if (d.length > 1024 * 4096) {
            failErr("stdin", "has grown too big", ENOMEM);
            abort();
        }
    }

    if (d.length == 0) {
        failErr("stdin", "no bytes from stdin", ENOMEM);
        abort();
    }

    (*gFuzzer)(gVerbose, (uint8_t*) d.bytes, d.length);
}

static void fuzzStdin()
{
    @autoreleasepool {
        fuzzStdin0();
    }
}

int main(int argc, char **argv)
{
    @autoreleasepool {
        NSMutableArray<NSString*>* arr = nil;

        for (int i = 1;  i < argc;  i++) {
            if (strcmp(argv[i], "--ipp") == 0)
                gFuzzer = _ipp_fuzzing;
            else if (strcmp(argv[i], "--asn1") == 0)
                gFuzzer = _asn1_fuzzing;
            else if (strcmp(argv[i], "--verbose") == 0)
                gVerbose++;
//            else if (strcmp(argv[i], "--debug") == 0)
//                _cups_debug_set("-", "2", NULL, 0);
            else if (strcmp(argv[i], "--parallel") == 0)
                gParallel++;
            else {
                if (arr == nil)
                    arr = [NSMutableArray new];
                [arr addObject:[NSString stringWithUTF8String:argv[i]]];
            }
        }


#if _ASN1_FUZZER
        gFuzzer = _asn1_fuzzing;
#endif
#if _IPP_FUZZER
        gFuzzer = _ipp_fuzzing;
#endif


        if (gFuzzer == NULL) {
            NSLog(@"%s requires at least --ipp or --asn1", argv[0]);
            exit(-1);
        }

        @autoreleasepool {
            if (arr == nil) {
                NSLog(@"fuzzing stdin");
                fuzzStdin();
            } else {
                if (gParallel) {
                    gGroup = dispatch_group_create();
                }

                [arr enumerateObjectsUsingBlock:^(NSString * _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
                    fuzzFile([obj UTF8String]);
                }];

                if (gGroup) {
                    dispatch_group_wait(gGroup, DISPATCH_TIME_FOREVER);
                }
            }
        }
    }

    exit(0);
}
