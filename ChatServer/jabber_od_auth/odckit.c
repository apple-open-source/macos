/*
 *  odckit.c
 *
 *  korver@apple.com
 *
 *  Copyright (c) 2009, Apple Inc. All rights reserved.
 */

#include "odckit.h"
#include "odkit.h"


/* data structures that can be flushed once auth has succeeded */
typedef struct ODCKSessionFlushable {
    ODKSession    *odkSession;
    char           serverChallengeCStr[1024];
    char           userNameCStr[64];
} ODCKSessionFlushable;

typedef struct ODCKSessionPriv {
    ODCKSessionFlushable  *flushable;
    char                   serverResponseCStr[256];
    char                   badParameter;
    char                  *error;
} ODCKSessionPriv;

#define ODCK_ODKSESSION(session) \
        ((session)->flushable->odkSession)

#define ODCK_SERVER_CHALLENGE_CSTR(session) \
        ((session)->flushable->serverChallengeCStr)

#define ODCK_USERNAME_CSTR(session) \
        ((session)->flushable->userNameCStr)

#define ODCK_SERVER_RESPONSE_CSTR(session) \
        ((session)->serverResponseCStr)

#define ODCK_SESSION_PRIV(session) \
        ODCKSessionOpaque(session)

#define ODCK_PARAM_ASSERT(expression) \
        do { _session->badParameter = !(expression); \
             if (_session->badParameter) return -1; } while(0)

#define CF_SAFE_RELEASE(cfobj) \
        do { if ((cfobj) != NULL) CFRelease((cfobj)); cfobj = NULL; } while (0)


static ODCKSessionPriv* ODCKSessionOpaque(ODCKSession *session);
static int ODCKCreateSessionFlushable(ODCKSessionPriv *session, ODCKSessionFlushable **out);
static int ODCKDeleteSessionFlushable(ODCKSessionFlushable **out);
static int ODCKMaybeCreateString(ODCKSession *session, CFStringRef *dst, const char *src);
static int ODCKMaybeCreateData(ODCKSession *session, CFDataRef *dst, const char *src, const unsigned int srclen);
static int ODCKGetData(char *dst, unsigned int dstlen, unsigned int *len, CFDataRef src);

ODCKSessionPriv*
ODCKSessionOpaque(ODCKSession *session)
{
    ODCKSessionPriv *priv = (ODCKSessionPriv*)session;
    assert(priv != 0);
    return priv;
}

int
ODCKMaybeCreateString(ODCKSession *session, CFStringRef *dst, const char *src)
{
    ODCKSessionPriv *_session = ODCK_SESSION_PRIV(session);
    return ODKMaybeCreateString(ODCK_ODKSESSION(_session), dst, src);
}

int
ODCKMaybeCreateData(ODCKSession *session, CFDataRef *dst, const char *src, const unsigned int srclen)
{
    ODCKSessionPriv *_session = ODCK_SESSION_PRIV(session);
    return ODKMaybeCreateData(ODCK_ODKSESSION(_session), dst, src, srclen);
}

int
ODCKGetData(char *dst, unsigned int dstlen, unsigned int *len, CFDataRef src)
{
    int retval = -1;
    unsigned int srclen = (unsigned int)CFDataGetLength(src);

    /* note: 1 extra character for safety, to null terminate */
    if (srclen >= dstlen)
        goto done;

    memcpy(dst, CFDataGetBytePtr(src), (size_t)srclen);
    dst[srclen] = '\0';

    *len = srclen;

    retval = 0;
done:
    return retval;
}

int
ODCKCreateSession(ODCKSession **out)
{
    int retval = -1;
    ODCKSessionPriv *session = 0;

    *out = 0;

    session = (void*)calloc(1, sizeof(*session));
    if (session == 0) {
        /* this isn't logged, but there isn't an ODCKSession for storing the error */
        goto done;
    }

    *out = (ODCKSession*)session;

    retval = 0;
done:
    return retval;
}

int
ODCKDeleteSession(ODCKSession **out)
{
    int retval = -1;

    if (out == 0)
        goto done;

    ODCKSessionPriv *session = *(ODCKSessionPriv**)out;
    if (session != 0) {
        (void)ODCKDeleteSessionFlushable(&session->flushable);

        if (session->error != 0)
            free(session->error);

        memset(session, 0, sizeof(*session));

        free(session);
    }

    *out = 0;

    retval = 0;

done:
    return retval;
}

int
ODCKCreateSessionFlushable(ODCKSessionPriv *session, ODCKSessionFlushable **out)
{
    int retval = -1;
    ODCKSessionFlushable *flushable = 0;

    *out = 0;

    flushable = (void*)calloc(1, sizeof(*flushable)); 
    if (flushable == 0) {
        session->error = strdup("Out of memory");  /* will probably fail, but worth a try */
        goto done;
    }

    if (ODKCreateSession(&flushable->odkSession) != 0) {
        session->error = strdup("Unable to create session");  /* will probably fail, but worth a try */
        goto done;
    }

    *out = flushable;

    retval = 0;
done:
    if (retval != 0)
        (void)ODCKDeleteSessionFlushable(&flushable);

    return retval;
}

int 
ODCKDeleteSessionFlushable(ODCKSessionFlushable **out) 
{
    int retval = -1;
    ODCKSessionFlushable *flushable = 0;

    if (out == 0)
        goto done;

    flushable = *out;
    if (flushable != 0) {
        if (flushable->odkSession != 0)
            (void)ODKDeleteSession(&flushable->odkSession);

        memset(flushable, 0, sizeof(*flushable));
        free(flushable);
    }

    *out = 0;

    retval = 0;
done:
    return retval;
}

int
ODCKFlushSession(ODCKSession *session)
{
    int retval = -1;
    ODCKSessionPriv *_session = ODCK_SESSION_PRIV(session);
    char *error = 0;

    ODCK_PARAM_ASSERT(session != 0);

    error = ODKGetError(ODCK_ODKSESSION(_session));
    if (error)
        _session->error = strdup(error);

    (void)ODCKDeleteSessionFlushable(&_session->flushable);

    retval = 0;
done:
    return retval;
}

int
ODCKGetServerChallenge(ODCKSession *session, const char *authMethod, const char *userName, char **serverChallengeOut, unsigned int *serverChallengeLenOut)
{
    ODCKSessionPriv *_session = ODCK_SESSION_PRIV(session);
    int retval = -1;
    CFStringRef cfAuthMethod = NULL;
    CFStringRef cfUserName = NULL;
    CFDataRef cfServerChallenge = NULL;

    ODCK_PARAM_ASSERT(serverChallengeOut != 0);
    ODCK_PARAM_ASSERT(serverChallengeLenOut != 0);

    *serverChallengeOut = 0;
    *serverChallengeLenOut = 0;

    if (_session->flushable == 0) {
        if (ODCKCreateSessionFlushable(_session, &_session->flushable) != 0)
            goto done;
    }

    if (ODCKMaybeCreateString(session, &cfAuthMethod, authMethod) != 0
     || ODCKMaybeCreateString(session, &cfUserName, userName))
        goto done;

    if (ODKCopyServerChallenge(ODCK_ODKSESSION(_session), cfAuthMethod, cfUserName, &cfServerChallenge) != 0)
        goto done;

    if (ODCKGetData(ODCK_SERVER_CHALLENGE_CSTR(_session),
                    sizeof(ODCK_SERVER_CHALLENGE_CSTR(_session)),
                    serverChallengeLenOut,
                    cfServerChallenge) != 0)
        goto done;

    *serverChallengeOut = ODCK_SERVER_CHALLENGE_CSTR(_session);

    retval = 0;

done:
    CF_SAFE_RELEASE(cfServerChallenge);
    CF_SAFE_RELEASE(cfUserName);
    CF_SAFE_RELEASE(cfAuthMethod);

    return retval;
}

int
ODCKSetServerChallenge(ODCKSession *session, const char *authMethod, const char *userName, const char *serverChallenge, unsigned int serverChallengeLen)
{
    ODCKSessionPriv *_session = ODCK_SESSION_PRIV(session);
    int retval = -1;
    CFStringRef cfAuthMethod = NULL;
    CFStringRef cfUserName = NULL;
    CFDataRef cfServerChallenge = NULL;
    unsigned int ignore;

    if (_session->flushable == 0) {
        if (ODCKCreateSessionFlushable(_session, &_session->flushable) != 0)
            goto done;
    }

    if (ODCKMaybeCreateString(session, &cfAuthMethod, authMethod) != 0
     || ODCKMaybeCreateString(session, &cfUserName, userName) != 0
     || ODCKMaybeCreateData(session, &cfServerChallenge, serverChallenge, serverChallengeLen) != 0)
        goto done;

    if (ODCKGetData(ODCK_SERVER_CHALLENGE_CSTR(_session),
                    sizeof(ODCK_SERVER_CHALLENGE_CSTR(_session)),
                    &ignore,
                    cfServerChallenge) != 0)
        goto done;

    if (ODKSetServerChallenge(ODCK_ODKSESSION(_session), cfAuthMethod, cfUserName, cfServerChallenge) != 0)
        goto done;

    retval = 0;

done:
    CF_SAFE_RELEASE(cfServerChallenge);
    CF_SAFE_RELEASE(cfUserName);
    CF_SAFE_RELEASE(cfAuthMethod);

    return retval;
}

int
ODCKVerifyClientRequest(ODCKSession *session, const char *clientRequest, unsigned int clientRequestLen)
{
    ODCKSessionPriv *_session = ODCK_SESSION_PRIV(session);
    int retval = -1;
    CFDataRef cfClientRequest = NULL;

    if (_session->flushable == 0) {
        _session->badParameter = 1;
        goto done;
    }

    if (ODCKMaybeCreateData(session, &cfClientRequest,
                            clientRequest, clientRequestLen))
        goto done;

    if (ODKVerifyClientRequest(ODCK_ODKSESSION(_session), cfClientRequest) != 0)
        goto done;

    retval = 0;

done:
    CF_SAFE_RELEASE(cfClientRequest);

    return retval;
}

int
ODCKGetServerResponse(ODCKSession *session, char **userNameOut, char **serverResponseOut, unsigned int *serverResponseLenOut)
{
    ODCKSessionPriv *_session = ODCK_SESSION_PRIV(session);
    int retval = -1;
    CFStringRef cfUserName = NULL;
    CFDataRef cfServerResponse = NULL;

    ODCK_PARAM_ASSERT(userNameOut != 0);
    ODCK_PARAM_ASSERT(serverResponseOut != 0);
    ODCK_PARAM_ASSERT(serverResponseLenOut != 0);

    *userNameOut = 0;
    *serverResponseOut = 0;
    *serverResponseLenOut = 0;

    if (_session->flushable == 0) {
        _session->badParameter = 1;
        goto done;
    }

    if (ODKCopyServerResponse(ODCK_ODKSESSION(_session), &cfUserName, &cfServerResponse))
        goto done;

    if (ODCKGetData(ODCK_SERVER_RESPONSE_CSTR(_session),
                    sizeof(ODCK_SERVER_RESPONSE_CSTR(_session)),
                    serverResponseLenOut,
                    cfServerResponse) != 0)
        goto done;
    *serverResponseOut = ODCK_SERVER_RESPONSE_CSTR(_session);

    CFStringGetCString(cfUserName,
                       ODCK_USERNAME_CSTR(_session),
                       sizeof(ODCK_USERNAME_CSTR(_session)),
                       kCFStringEncodingUTF8);
    *userNameOut = ODCK_USERNAME_CSTR(_session);

    retval = 0;

done:
    CF_SAFE_RELEASE(cfServerResponse);
    CF_SAFE_RELEASE(cfUserName);

    return retval;
}

char *
ODCKGetError(ODCKSession *session)
{
    ODCKSessionPriv *_session = ODCK_SESSION_PRIV(session);
    if (_session == 0)
        return "Null parameter";
    else if (_session->badParameter)
        return "Bad parameter";
    else if (_session->error)
        return _session->error;
    else if (_session->flushable)
        return ODKGetError(ODCK_ODKSESSION(_session));

    return "Bad parameter";
}

