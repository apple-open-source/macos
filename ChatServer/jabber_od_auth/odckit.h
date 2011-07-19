/*
 *  odckit.h
 *
 *  korver@apple.com
 *
 *  Copyright (c) 2009, Apple Inc. All rights reserved.
 */

#ifndef __ODCKIT_H__
#define __ODCKIT_H__

typedef struct ODCKSession ODCKSession;

#ifdef __cplusplus
extern "C" {
#endif

int ODCKCreateSession(/*const char *applicationName, const char *serviceName, const char *hostName,*/ ODCKSession **sessionOut);
int ODCKDeleteSession(ODCKSession **sessionOut);

int ODCKFlushSession(ODCKSession *session);

int ODCKGetServerChallenge(ODCKSession *session, const char *authMethod, const char *userName, char **serverChallengeOut, unsigned int *serverChallengeLenOut);
int ODCKSetServerChallenge(ODCKSession *session, const char *authMethod, const char *userName, const char *serverChallenge, unsigned int serverChallengeLen);
int ODCKVerifyClientRequest(ODCKSession *session, const char *clientRequest, unsigned int clientRequestLen);
int ODCKGetServerResponse(ODCKSession *session, char **username, char **serverResponseOut, unsigned int *serverResponseLenOut);
int ODCKGetUserRealm(char *realm);
char *ODCKGetError(ODCKSession *session);

#ifdef __cplusplus
};
#endif
#endif
