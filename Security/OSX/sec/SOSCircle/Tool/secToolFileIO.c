//
//  secToolFileIO.c
//  sec
//
//
//

#include "secToolFileIO.h"

#include <copyfile.h>
#include <libgen.h>
#include <utilities/SecCFWrappers.h>

FILE *outFile = NULL;
FILE *errFile = NULL;

void _printcfmsg(FILE *ff, CFDictionaryRef formatOptions, CFStringRef format, ...)
{
    va_list args;
    va_start(args, format);
    CFStringRef message = CFStringCreateWithFormatAndArguments(kCFAllocatorDefault, formatOptions, format, args);
    va_end(args);
    CFStringPerformWithCString(message, ^(const char *utf8String) { fprintf(ff, utf8String, ""); });
    CFRelease(message);
}


int SOSLogSetOutputTo(char *dir, char *filename) {
    size_t pathlen = 0;

    if(dir && filename) {
        pathlen = strlen(dir) + strlen(filename) + 2;
        char path[pathlen];
        snprintf(path, pathlen, "%s/%s", dir, filename);
        outFile = fopen(path, "a");
    } else if(dir || filename) {
        outFile = stdout;
        return -1;
    } else {
        outFile = stdout;
    }
    errFile = stderr;
    return 0;
}

void closeOutput(void) {
    if(outFile != stdout) {
        fclose(outFile);
    }
    outFile = stdout;
}

int copyFileToOutputDir(char *dir, char *toCopy) {
    char *bname = basename(toCopy);
    char destpath[256];
    int status;
    copyfile_state_t cpfilestate = copyfile_state_alloc();

    status = snprintf(destpath, 256, "%s/%s", dir, bname);
    if(status < 0 || status > 256) return -1;

    int retval = copyfile(toCopy, destpath, cpfilestate, COPYFILE_ALL);

    copyfile_state_free(cpfilestate);
    return retval;
}
