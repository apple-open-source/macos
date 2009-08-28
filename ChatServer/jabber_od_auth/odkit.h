/*
 *  odkit.h
 *
 *  korver@apple.com
 *
 *  Copyright (c) 2009, Apple Inc. All rights reserved.
 */

#ifndef __ODKIT_H__
#define __ODKIT_H__

#include <CoreFoundation/CoreFoundation.h>

typedef struct ODKSession ODKSession;

#ifdef __cplusplus
extern "C" {
#endif

int ODKCreateSession(/*CFStringRef applicationName, CFStringRef serviceName, CFStringRef hostName,*/ ODKSession **sessionOut);
int ODKDeleteSession(ODKSession **sessionOut);

int ODKCopyServerChallenge(ODKSession *session, CFStringRef authMethod, CFStringRef userName, CFDataRef *serverChallengeOut);
int ODKSetServerChallenge(ODKSession *session, CFStringRef authMethod, CFStringRef userName, CFDataRef serverChallenge);
int ODKVerifyClientRequest(ODKSession *session, CFDataRef clientRequest);
int ODKCopyServerResponse(ODKSession *session, CFStringRef *userNameOut, CFDataRef *serverResponseOut);
int ODKFlushSession(ODKSession *session);

char *ODKGetError(ODKSession *session);

int ODKMaybeCreateString(ODKSession *session, CFStringRef *dst, const char *src);
int ODKMaybeCreateData(ODKSession *session, CFDataRef *dst, const char *src, const unsigned int srclen);

#ifdef __cplusplus
};
#endif
#endif
