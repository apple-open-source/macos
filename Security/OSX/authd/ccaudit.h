/* Copyright (c) 2012-2013 Apple Inc. All Rights Reserved. */

#ifndef _SECURITY_AUTH_CCAUDIT_H_
#define _SECURITY_AUTH_CCAUDIT_H_

#include <bsm/audit_uevents.h>

AUTH_WARN_RESULT AUTH_MALLOC AUTH_NONNULL_ALL AUTH_RETURNS_RETAINED
ccaudit_t ccaudit_create(process_t, auth_token_t, int32_t event);

AUTH_NONNULL_ALL
void ccaudit_log_authorization(ccaudit_t, const char * right, OSStatus err);

AUTH_NONNULL_ALL
void ccaudit_log_success(ccaudit_t, credential_t cred, const char * right);

AUTH_NONNULL_ALL
void ccaudit_log_failure(ccaudit_t, const char * credName, const char * right);

AUTH_NONNULL1
void ccaudit_log_mechanism(ccaudit_t, const char * right, const char * mech, uint32_t status, const char * interrupted);

AUTH_NONNULL1
void ccaudit_log(ccaudit_t, const char * right, const char * msg, OSStatus err);

#endif /* !_SECURITY_AUTH_CCAUDIT_H_ */
