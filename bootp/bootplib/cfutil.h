
#ifndef _S_CFUTIL_H
#define _S_CFUTIL_H

#include <CoreFoundation/CFPropertyList.h>
#include <CoreFoundation/CFString.h>

void
my_CFRelease(void * t);

CFPropertyListRef 
my_CFPropertyListCreateFromFile(char * filename);

int
my_CFPropertyListWriteFile(CFPropertyListRef plist, char * filename);

Boolean
DNSHostNameStringIsClean(CFStringRef str);

#endif _S_CFUTIL_H
