#include <CoreFoundation/CoreFoundation.h>

#ifndef __PRINTPLIST_H__
#define __PRINTPLIST_H__

#ifdef __cplusplus
extern "C" {
#endif

void printPList(FILE * stream, CFPropertyListRef plist);
void showPList(CFPropertyListRef plist);
CFMutableStringRef createCFStringForPlist(CFTypeRef plist);

#ifdef __cplusplus
}
#endif

#endif __PRINTPLIST_H__
