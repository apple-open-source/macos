/* Copyright (c) 2012-2013 Apple Inc. All Rights Reserved. */

#ifndef _SECURITY_AUTH_UTILITIES_H_
#define _SECURITY_AUTH_UTILITIES_H_

#include <xpc/xpc.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Authorization.h>

#if defined(__cplusplus)
extern "C" {
#endif

CF_RETURNS_RETAINED AuthorizationItemSet * DeserializeItemSet(const xpc_object_t);
XPC_RETURNS_RETAINED xpc_object_t SerializeItemSet(const AuthorizationItemSet*);
void FreeItemSet(AuthorizationItemSet*);

char * _copy_cf_string(CFTypeRef,const char*);
int64_t _get_cf_int(CFTypeRef,int64_t);
bool _get_cf_bool(CFTypeRef,bool);

bool _compare_string(const char *, const char *);
char * _copy_string(const char *);
void * _copy_data(const void * data, size_t dataLen);

bool _cf_set_iterate(CFSetRef, bool(^iterator)(CFTypeRef value));
bool _cf_bag_iterate(CFBagRef, bool(^iterator)(CFTypeRef value));
bool _cf_dictionary_iterate(CFDictionaryRef, bool(^iterator)(CFTypeRef key,CFTypeRef value));

bool isInFVUnlockOrRecovery(void);

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_AUTH_UTILITIES_H_ */
