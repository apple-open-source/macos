//
//  secToolFileIO.h
//  sec
//
//
//

#ifndef secToolFileIO_h
#define secToolFileIO_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <time.h>
#include <CoreFoundation/CoreFoundation.h>

#define printmsg(format, ...) _printcfmsg(outFile, NULL, format, ##__VA_ARGS__)
#define printmsgWithFormatOptions(formatOptions, format, ...) _printcfmsg(outFile, formatOptions, format, ##__VA_ARGS__)
#define printerr(format, ...) _printcfmsg(errFile, NULL, format, ##__VA_ARGS__)

extern FILE *outFile;
extern FILE *errFile;

void _printcfmsg(FILE *ff, CFDictionaryRef formatOptions, CFStringRef format, ...);

int SOSLogSetOutputTo(char *dir, char *filename);

void closeOutput(void);

int copyFileToOutputDir(char *dir, char *toCopy);

#endif /* secToolFileIO_h */
