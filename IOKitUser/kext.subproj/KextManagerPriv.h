#ifndef __KEXTMANAGER_H__
#define __KEXTMANAGER_H__

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Authorization.h>

#ifdef __cplusplus
extern "C" {
#endif

CFArrayRef _KextManagerCreatePropertyValueArray(
    CFAllocatorRef allocator,
    CFStringRef    propertyKey);

void _KextManagerUserDidLogIn(uid_t euid, AuthorizationExternalForm authref);
void _KextManagerUserWillLogOut(uid_t euid);

uid_t _KextManagerGetLoggedInUserid(void);

void _KextManagerRecordNonsecureKextload(const char * load_data,
    size_t data_length);

#ifdef __cplusplus
}
#endif

#endif __KEXTMANAGER_H__
