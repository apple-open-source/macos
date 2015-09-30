/* Copyright (c) 2012 Apple Inc. All Rights Reserved. */

#ifndef _SECURITY_AUTH_MECHANISM_H_
#define _SECURITY_AUTH_MECHANISM_H_

#include "authdb.h"

#if defined(__cplusplus)
extern "C" {
#endif

enum {
    kMechanismTypeEntitled              = 1
};
    
AUTH_WARN_RESULT AUTH_MALLOC AUTH_NONNULL_ALL AUTH_RETURNS_RETAINED
mechanism_t mechanism_create_with_sql(auth_items_t);

AUTH_WARN_RESULT AUTH_MALLOC AUTH_NONNULL1 AUTH_RETURNS_RETAINED
mechanism_t mechanism_create_with_string(const char *,authdb_connection_t);
    
AUTH_NONNULL_ALL
bool mechanism_sql_fetch(mechanism_t,authdb_connection_t);
    
AUTH_NONNULL_ALL
bool mechanism_sql_commit(mechanism_t,authdb_connection_t);

AUTH_NONNULL_ALL
bool mechanism_exists(mechanism_t);

AUTH_NONNULL_ALL
const char * mechanism_get_string(mechanism_t);
    
AUTH_NONNULL_ALL
int64_t mechanism_get_id(mechanism_t);
    
AUTH_NONNULL_ALL
const char * mechanism_get_plugin(mechanism_t);

AUTH_NONNULL_ALL
const char * mechanism_get_param(mechanism_t);
    
AUTH_NONNULL_ALL
uint64_t mechanism_get_type(mechanism_t);
  
AUTH_NONNULL_ALL
bool mechanism_is_privileged(mechanism_t);
    
#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_AUTH_MECHANISM_H_ */
