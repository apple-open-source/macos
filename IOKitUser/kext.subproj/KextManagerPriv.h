#if !__LP64__

#ifndef __KEXTMANAGERPRIV_H__
#define __KEXTMANAGERPRIV_H__

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Authorization.h>

#include <sys/cdefs.h>

__BEGIN_DECLS

kern_return_t _KextManagerRecordPathForBundleID(CFStringRef kextBundleID,
    CFStringRef kextPath);

CFArrayRef _KextManagerCreatePropertyValueArray(
    CFAllocatorRef allocator,
    CFStringRef    propertyKey);

void _KextManagerUserDidLogIn(uid_t euid, AuthorizationExternalForm authref);
void _KextManagerUserWillLogOut(uid_t euid);

uid_t _KextManagerGetLoggedInUserid(void);

void _KextManagerRecordNonsecureKextload(const char * load_data,
    size_t data_length);

__END_DECLS

#endif __KEXTMANAGERPRIV_H__
#endif // !__LP64__
