/* Copyright (c) 2012 Apple Inc. All rights reserved. */

#ifndef _SECURITY_AUTH_ITEMS_H_
#define _SECURITY_AUTH_ITEMS_H_

#include <Security/Authorization.h>
#include <xpc/xpc.h>

#if defined(__cplusplus)
extern "C" {
#endif

enum {
    AI_TYPE_UNKNOWN = 0,
    AI_TYPE_RIGHT,
    AI_TYPE_STRING,
    AI_TYPE_INT,
    AI_TYPE_UINT,
    AI_TYPE_INT64,
    AI_TYPE_UINT64,
    AI_TYPE_DOUBLE,
    AI_TYPE_BOOL,
    AI_TYPE_DATA
};

#pragma mark -
#pragma mark auth_items_t
    
/* unordered items */
    
#ifdef __BLOCKS__
typedef bool (^auth_items_iterator_t)(const char *key);
#endif /* __BLOCKS__ */

CFTypeID auth_items_get_type_id(void);
    
AUTH_WARN_RESULT AUTH_MALLOC AUTH_RETURNS_RETAINED
auth_items_t auth_items_create(void);
    
AUTH_WARN_RESULT AUTH_MALLOC AUTH_NONNULL_ALL AUTH_RETURNS_RETAINED
auth_items_t auth_items_create_with_xpc(const xpc_object_t data);
    
AUTH_WARN_RESULT AUTH_MALLOC AUTH_NONNULL_ALL AUTH_RETURNS_RETAINED
auth_items_t auth_items_create_copy(auth_items_t);

AUTH_WARN_RESULT AUTH_NONNULL_ALL
size_t auth_items_get_count(auth_items_t);

AUTH_WARN_RESULT AUTH_NONNULL_ALL
AuthorizationItemSet * auth_items_get_item_set(auth_items_t);

AUTH_WARN_RESULT AUTH_NONNULL_ALL
xpc_object_t auth_items_export_xpc(auth_items_t);

AUTH_NONNULL_ALL
void auth_items_set_flags(auth_items_t, const char *key, uint32_t flags);

AUTH_NONNULL_ALL
void auth_items_clear_flags(auth_items_t, const char *key, uint32_t flags);
    
AUTH_WARN_RESULT AUTH_NONNULL_ALL
uint32_t auth_items_get_flags(auth_items_t, const char *key);

AUTH_NONNULL_ALL
bool auth_items_check_flags(auth_items_t, const char *key, uint32_t flags);

AUTH_NONNULL_ALL
void auth_items_set_key(auth_items_t, const char *key);
    
AUTH_NONNULL_ALL
bool auth_items_exist(auth_items_t, const char *key);

AUTH_NONNULL_ALL
void auth_items_remove(auth_items_t, const char *key);

AUTH_NONNULL_ALL
void auth_items_remove_with_flags(auth_items_t, uint32_t flags);
    
AUTH_NONNULL_ALL
void auth_items_clear(auth_items_t);

AUTH_NONNULL_ALL
void auth_items_copy(auth_items_t, auth_items_t src);
    
AUTH_NONNULL_ALL
void auth_items_copy_xpc(auth_items_t, const xpc_object_t src);

AUTH_NONNULL_ALL    
void auth_items_copy_with_flags(auth_items_t, auth_items_t src, uint32_t flags);
    
AUTH_NONNULL_ALL    
bool auth_items_iterate(auth_items_t, auth_items_iterator_t iter);
    
AUTH_NONNULL_ALL
void auth_items_set_string(auth_items_t, const char *key, const char *value);

AUTH_WARN_RESULT AUTH_NONNULL_ALL
const char * auth_items_get_string(auth_items_t, const char *key);
    
AUTH_NONNULL_ALL
void auth_items_set_data(auth_items_t, const char *key, const void *value, size_t len);

AUTH_WARN_RESULT AUTH_NONNULL_ALL
const void * auth_items_get_data(auth_items_t, const char *key, size_t * len);
    
AUTH_NONNULL_ALL
void auth_items_set_bool(auth_items_t, const char *key, bool value);

AUTH_WARN_RESULT AUTH_NONNULL_ALL
bool auth_items_get_bool(auth_items_t, const char *key);
    
AUTH_NONNULL_ALL
void auth_items_set_int(auth_items_t, const char *key, int32_t value);

AUTH_WARN_RESULT AUTH_NONNULL_ALL
int32_t auth_items_get_int(auth_items_t, const char *key);

AUTH_NONNULL_ALL
void auth_items_set_uint(auth_items_t, const char *key, uint32_t value);

AUTH_WARN_RESULT AUTH_NONNULL_ALL
uint32_t auth_items_get_uint(auth_items_t, const char *key);
    
AUTH_NONNULL_ALL
void auth_items_set_int64(auth_items_t, const char *key, int64_t value);

AUTH_WARN_RESULT AUTH_NONNULL_ALL
int64_t auth_items_get_int64(auth_items_t, const char *key);

AUTH_NONNULL_ALL
void auth_items_set_uint64(auth_items_t, const char *key, uint64_t value);

AUTH_WARN_RESULT AUTH_NONNULL_ALL    
uint64_t auth_items_get_uint64(auth_items_t, const char *key);

AUTH_NONNULL_ALL
void auth_items_set_double(auth_items_t, const char *key, double value);

AUTH_WARN_RESULT AUTH_NONNULL_ALL
double auth_items_get_double(auth_items_t, const char *key);
    
AUTH_WARN_RESULT AUTH_NONNULL_ALL
uint32_t auth_items_get_type(auth_items_t, const char *key);

AUTH_WARN_RESULT AUTH_NONNULL_ALL
size_t auth_items_get_length(auth_items_t, const char *key);

AUTH_NONNULL_ALL
void auth_items_set_value(auth_items_t, const char *key, uint32_t type, uint32_t flags, const void *value, size_t len);

#pragma mark -
#pragma mark auth_rights_t
    
/* ordered items */
    
AUTH_WARN_RESULT AUTH_MALLOC AUTH_RETURNS_RETAINED
auth_rights_t auth_rights_create(void);

AUTH_WARN_RESULT AUTH_MALLOC AUTH_NONNULL_ALL AUTH_RETURNS_RETAINED
auth_rights_t auth_rights_create_with_xpc(const xpc_object_t data);

AUTH_WARN_RESULT AUTH_NONNULL_ALL
xpc_object_t auth_rights_export_xpc(auth_rights_t);

AUTH_NONNULL_ALL
void auth_rights_set_flags(auth_rights_t, const char *key, uint32_t flags);

AUTH_NONNULL_ALL
void auth_rights_clear_flags(auth_rights_t, const char *key, uint32_t flags);

AUTH_WARN_RESULT AUTH_NONNULL_ALL
uint32_t auth_rights_get_flags(auth_rights_t, const char *key);

AUTH_NONNULL_ALL
bool auth_rights_check_flags(auth_rights_t, const char *key, uint32_t flags);
    
AUTH_WARN_RESULT AUTH_NONNULL_ALL
size_t auth_rights_get_count(auth_rights_t);

AUTH_NONNULL_ALL
void auth_rights_add(auth_rights_t, const char *key);

AUTH_NONNULL_ALL
bool auth_rights_exist(auth_rights_t, const char *key);

AUTH_NONNULL_ALL
void auth_rights_remove(auth_rights_t, const char *key);

AUTH_NONNULL_ALL
void auth_rights_clear(auth_rights_t);

AUTH_NONNULL_ALL
bool auth_rights_iterate(auth_rights_t rights, bool(^iter)(const char * key));
    
#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_AUTH_ITEMS_H_ */
