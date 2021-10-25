/* Copyright (c) 2021 Apple Inc. All Rights Reserved. */

#ifndef _SECURITY_AUTHD_OD_H_
#define _SECURITY_AUTHD_OD_H_

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCode.h>

AUTH_NONNULL_ALL
bool
odUserHasSecureToken(const char *username);

#endif /* _SECURITY_AUTHD_OD_H_ */
