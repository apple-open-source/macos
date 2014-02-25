#ifndef _OPEN_DIRECTORY_H_
#define _OPEN_DIRECTORY_H_

#include <CoreFoundation/CoreFoundation.h>
#include <OpenDirectory/OpenDirectory.h>

extern void setrestricted(CFDictionaryRef attrs);

ODRecordRef odGetUser(CFStringRef location, CFStringRef authname, CFStringRef user, CFDictionaryRef* attrs);

void odUpdateUser(ODRecordRef rec, CFDictionaryRef attrs_orig, CFDictionaryRef attrs);

#endif /* _OPEN_DIRECTORY_H_ */
