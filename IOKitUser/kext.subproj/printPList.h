#if !__LP64__

#include <CoreFoundation/CoreFoundation.h>

#ifndef __PRINTPLIST_H__
#define __PRINTPLIST_H__

#include <sys/cdefs.h>

__BEGIN_DECLS

void printPList(FILE * stream, CFPropertyListRef plist);
void showPList(CFPropertyListRef plist);
CFMutableStringRef createCFStringForPlist(CFTypeRef plist);

__END_DECLS

#endif __PRINTPLIST_H__
#endif // !__LP64__
