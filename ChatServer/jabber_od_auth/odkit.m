/*
 *  odkit.c
 *
 *  korver@apple.com
 *
 *  Copyright (c) 2009, Apple Inc. All rights reserved.
 */

#include "odkit.h"
#include "dserr.h"
#include "cyrus-sasl-digestmd5-parse.h"

#include <pwd.h>
#include <sasl/sasl.h>
#include <sasl/saslutil.h>
#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <DirectoryService/DirServicesConstPriv.h>
#include <OpenDirectory/OpenDirectory.h>
#include <netdb.h>
#include <ServerFoundation/ServerFoundation.h>
#include <Foundation/Foundation.h>

typedef enum {
    kODKActiveDirectoryUnknown  = 0,
    kODKActiveDirectoryAbsent   = 1,
    kODKActiveDirectoryPresent  = 2
} ODKHasAD;

typedef struct ODKSessionPriv {
    ODKHasAD       useActiveDirectory;
    ODNodeRef      adNode;
    CFStringRef    authMethod;
    CFStringRef    userName;
    ODRecordRef    userRecord;
    CFDataRef      serverChallenge;
    CFDataRef      serverResponse;
    char           buffer[1024];
    char           error[512];
} ODKSessionPriv;

typedef struct ODKADInfo {
    ODNodeRef      searchNode;
    CFArrayRef     subNodeArray;
    ODKHasAD       hasActiveDirectory;
    ODNodeRef      adNode;
} ODKADInfo;

/* search nodes which are cached for performance */
static ODNodeRef gODSearchNode = NULL;
static ODKADInfo gADInfo = { NULL, NULL, kODKActiveDirectoryUnknown, NULL };

#define ODK_SESSION_PRIV(session) \
        ODKSessionOpaque(session)

#define ODK_LOG(level, string) \
        ODKLog(session, (level), __PRETTY_FUNCTION__, (string))

#define ODK_LOG_CFERROR(level, string, err) \
        ODKLogCFError(session, (level), __PRETTY_FUNCTION__, (string), (err))

#define ODK_PARAM_ASSERT(expression) \
        do { if (!(expression)) { ODK_LOG(LOG_ERR, "Bad parameter"); return -1; } } while(0)

#define CF_SAFE_RELEASE(cfobj) \
        do { if ((cfobj) != NULL) CFRelease((cfobj)); cfobj = NULL; } while (0)


#pragma mark -
#pragma mark Prototypes
#pragma mark -

static ODKSessionPriv *ODKSessionOpaque(ODKSession *session);
static ODKADInfo *ODKGetADInfo(ODKSession *session);
static int ODKIsSameString(CFStringRef str1, CFStringRef str2);
static int ODKSearchPolicyHasChanged(ODKSession *session);
static int ODKConfigureADInfo(ODKSession *session);
static int ODKResetADInfo(ODKSession *session);
#if 0
static int ODKGetHostFromSystemConfiguration( char *inOutHostStr, CFIndex maxHostStrLen );
//static int ODKGetHostFromAnywhere( char *inOutHostStr, size_t maxHostStrLen );
#endif
static CFStringRef ODKCopyAttrFromRecord(ODRecordRef record, CFStringRef attribute);
static int ODKConfigureUserRecord(ODKSession *session);
static int ODKConfigureServerChallenge(ODKSession *session);
static int ODKVerifyClientRequestFixed(ODKSession *session, CFStringRef clientRequest);
static int ODKUseActiveDirectory(ODKSession *session);
static int ODKHasUserRecord(ODKSession *session);
static int ODKUserRecordIsActiveDirectory(ODKSession *session);
static int ODKIsActiveDirectoryRecord(ODRecordRef record);
static int ODKParseUsername(ODKSession *session, CFStringRef client, CFStringRef *userNameOut);
static void ODKLog(ODKSession *session, int priority, const char *function, const char *message);
static void ODKLogCFError(ODKSession *session, int priority, const char *function, const char *message, CFErrorRef cfError);
static int ODKRecordVerifyPasswordExtended(ODKSession *session, ODRecordRef userRecord, CFStringRef authMethod, CFArrayRef inItems, CFArrayRef *outItems, ODContextRef *outContext, CFErrorRef *outError);

#pragma mark -
#pragma mark Errors
#pragma mark -

char *
ODKGetError(ODKSession *session)
{
    ODKSessionPriv *_session = ODK_SESSION_PRIV(session);
    if (_session == 0)
        return "Bad parameter";

    if (*_session->error == 0)
        return 0;

    return _session->error;
}

#pragma mark -
#pragma mark Logging
#pragma mark -

void
ODKLog(ODKSession *session, int priority, const char *function, const char *message)
{
    ODKSessionPriv *_session = ODK_SESSION_PRIV(session);
    if (_session != 0)
        snprintf(_session->error, sizeof(_session->error)-1, "%s: %s", function, message);

    syslog(priority, "%s: %s", function, message);
}

void
ODKLogCFError(ODKSession *session, int priority, const char *function, const char *message, CFErrorRef cfError)
{
    if (cfError == NULL) {
        ODKLog(session, priority, function, message);
    }
    else {
        char buf1[128];
        char buf2[256];
        CFStringRef cfErr = CFErrorCopyFailureReason(cfError);
        CFStringGetCString(cfErr, buf1, sizeof(buf1), kCFStringEncodingUTF8);
        snprintf(buf2, sizeof(buf2)-1, "%s: %s", message, buf1);
        ODKLog(session, priority, function, buf2);
    }
}

#pragma mark -
#pragma mark Helpful Routines
#pragma mark -

ODKSessionPriv*
ODKSessionOpaque(ODKSession *session)
{
    ODKSessionPriv *priv = (ODKSessionPriv*)session;
    return priv;
}

int
ODKMaybeCreateString(ODKSession *session, CFStringRef *dst, const char *src)
{
    int retval = -1;

    if (src) {
        *dst = CFStringCreateWithCString(kCFAllocatorDefault, src, kCFStringEncodingUTF8);
        if (*dst == 0) {
            ODK_LOG(LOG_ERR, "Out of memory");
            goto done;
        }
    }
    else {
        *dst = NULL;
    }

    retval = 0;

done:
    return retval;
}

int
ODKMaybeCreateData(ODKSession *session, CFDataRef *dst, const char *src, const unsigned int srclen)
{
    int retval = -1;

    if (src) {
        *dst = CFDataCreate(kCFAllocatorDefault, (UInt8*)src, (CFIndex)srclen);
        if (*dst == 0) {
            ODK_LOG(LOG_ERR, "Out of memory");
            goto done;
        }
    }
    else {
        *dst = NULL;
    }

    retval = 0;
done:
    return retval;
}

int
ODKNodeIsStale(ODNodeRef node) 
{
    return (ODNodeGetName(node) == NULL);
}

#pragma mark -
#pragma mark Active Directory Node/User Status
#pragma mark -

ODKADInfo*
ODKGetADInfo(ODKSession *session)
{
    if (gADInfo.hasActiveDirectory == kODKActiveDirectoryUnknown
     || ODKSearchPolicyHasChanged(session)
     || ODKNodeIsStale(gADInfo.adNode)) {
        ODKResetADInfo(session);
        ODKConfigureADInfo(session);
    }

    return &gADInfo;
}

int
ODKIsSameString(CFStringRef str1, CFStringRef str2)
{
    if (CFStringGetLength(str1) != CFStringGetLength(str2))
        return 0;

    return CFStringHasPrefix(str1, str2);
}

int
ODKSearchPolicyHasChanged(ODKSession *session)
{
    static time_t then = 0;
    time_t now;
    int hasChanged = 1;
    CFArrayRef cfSubNodeArray = NULL;
    CFErrorRef cfError = NULL;
    CFIndex i;

    /* This function polls, which is ugly, but it is the best solution for the moment.
     * These calls are really cheap (especially since we perform them rarely), and
     * jabberd doesn't have an event loop that can be easily hooked leveraged to
     * be notified of config changes */

    if (gADInfo.subNodeArray == NULL || gADInfo.searchNode == NULL)
        goto done;

    /* don't bother talking to DirectoryService more than once every 30 seconds */
    now = time(0);
    if (difftime(now, then) < 30.0)
        goto has_not_changed;  /* returning "has not changed" */

    then = now;  /* keep track of the last time this check was attempted */

    /* get search node's list of nodes */
    cfSubNodeArray = ODNodeCopySubnodeNames(gADInfo.searchNode, &cfError);
    if (cfSubNodeArray == 0) {
        ODK_LOG_CFERROR(LOG_ERR, "Unable to get the list of subnodes on the search node", cfError);
        goto done;
    }

    if (CFArrayGetCount(gADInfo.subNodeArray) != CFArrayGetCount(cfSubNodeArray))
        goto done;

    for (i = 0; i < CFArrayGetCount(cfSubNodeArray); ++i) {
        CFStringRef nodeName1 = CFArrayGetValueAtIndex(cfSubNodeArray, i);
        CFStringRef nodeName2 = CFArrayGetValueAtIndex(gADInfo.subNodeArray, i);
        if (! ODKIsSameString(nodeName1, nodeName2))
            goto done;
    }

has_not_changed:
    hasChanged = 0;
done:
    CF_SAFE_RELEASE(cfSubNodeArray);
    CF_SAFE_RELEASE(cfError);

    return hasChanged;
}

int
ODKResetADInfo(ODKSession *session)
{
    int retval = -1;

    CF_SAFE_RELEASE(gADInfo.adNode);
    gADInfo.adNode = NULL;

    CF_SAFE_RELEASE(gADInfo.searchNode);
    gADInfo.searchNode = NULL;

    CF_SAFE_RELEASE(gADInfo.subNodeArray);
    gADInfo.subNodeArray = NULL;

    gADInfo.hasActiveDirectory = kODKActiveDirectoryUnknown;

    /* ignore the adNode in the session, because this session has failed and will
     * be cleaned up soon anyways */

    retval = 0;
//done:
    return retval;
}

int
ODKPossiblyResetADInfo(ODKSession *session, CFErrorRef error)
{
    ODKSessionPriv *_session = ODK_SESSION_PRIV(session);

    if (error != NULL && _session->adNode != NULL) {
        if (! IS_EXPECTED_DS_ERROR(CFErrorGetCode(error))) {
            /* this is an unexpected error, let's flush the search node */
            ODK_LOG_CFERROR(LOG_DEBUG, "Flushing AD node because of unexpected error", error);
            ODKResetADInfo(session);
        }
    }

    return 0;
}

int
ODKConfigureADInfo(ODKSession *session)
{
    int retval = -1;
    int adNodeOnSearchPolicy = 0;
    CFErrorRef cfError = NULL;
    CFIndex i;

    assert(gADInfo.hasActiveDirectory == kODKActiveDirectoryUnknown);

    /* get search node */
    gADInfo.searchNode = ODNodeCreateWithNodeType(kCFAllocatorDefault, kODSessionDefault, kODNodeTypeAuthentication, &cfError);
    if (gADInfo.searchNode == 0) {
        ODK_LOG_CFERROR(LOG_ERR, "Unable to get a reference to the search node", cfError);
        goto done;
    }

    /* get search node's list of nodes */
    gADInfo.subNodeArray = ODNodeCopySubnodeNames(gADInfo.searchNode, &cfError);
    if (gADInfo.subNodeArray == 0) {
        ODK_LOG_CFERROR(LOG_ERR, "Unable to get the list of subnodes on the search node", cfError);
        goto done;
    }

    /* check for AD node on the search policy */
    for (i = 0; i < CFArrayGetCount(gADInfo.subNodeArray); ++i) {
        CFStringRef nodeName = CFArrayGetValueAtIndex(gADInfo.subNodeArray, i);
        if (CFStringHasPrefix(nodeName, CFSTR("/Active Directory/"))) {
            adNodeOnSearchPolicy = 1;
            break;
        }
    }

    /* get AD node (if present) */
    if (adNodeOnSearchPolicy) {
        gADInfo.adNode = ODNodeCreateWithName(kCFAllocatorDefault, kODSessionDefault, CFSTR("/Active Directory"), &cfError);
        if (gADInfo.adNode == 0) {
            ODK_LOG_CFERROR(LOG_ERR, "Unable to get a reference to the AD node", cfError);
            goto done;
        }

        gADInfo.hasActiveDirectory = kODKActiveDirectoryPresent;
    }

    if (gADInfo.hasActiveDirectory != kODKActiveDirectoryPresent)
        gADInfo.hasActiveDirectory = kODKActiveDirectoryAbsent;

    retval = 0;
done:
    if (retval != 0)
        ODKResetADInfo(session);

    CF_SAFE_RELEASE(cfError);

    return retval;
}

#pragma mark -
#pragma mark Hostname Help
#pragma mark -

#if 0
//-----------------------------------------------------------------------------
//    GetHostFromSystemConfiguration
//-----------------------------------------------------------------------------

int
ODKGetHostFromSystemConfiguration( char *inOutHostStr, CFIndex maxHostStrLen )
{
    int result = -1;
    SCPreferencesRef scpRef = NULL;

    do
    {
        scpRef = SCPreferencesCreate( NULL, CFSTR("DirectoryService"), 0 );
        if ( scpRef == NULL )
            break;

        CFDictionaryRef sysDict = (CFDictionaryRef) SCPreferencesGetValue( scpRef, CFSTR("System") );
        if ( sysDict == NULL )
            break;

        CFDictionaryRef sys2Dict = (CFDictionaryRef) CFDictionaryGetValue( sysDict, CFSTR("System") );
        if ( sys2Dict == NULL )
            break;

        CFStringRef hostString = (CFStringRef) CFDictionaryGetValue( sys2Dict, CFSTR("HostName") );
        if ( hostString == NULL )
            break;

        if ( CFStringGetCString(hostString, inOutHostStr, maxHostStrLen, kCFStringEncodingUTF8) )
            result = 0;
    }
    while (0);

    if ( scpRef != NULL )
        CFRelease( scpRef );

    return result;
}

/*
int
ODKGetHostFromAnywhere( char *inOutHostStr, size_t maxHostStrLen )
{
    int idx = 0;
    int result = ODKGetHostFromSystemConfiguration( inOutHostStr, (CFIndex)maxHostStrLen );
    if ( result != 0 )
    {
        // try DNS
        in_addr_t *ipList = NULL;
        struct hostent *hostEnt = NULL;
        struct sockaddr_in addr = { sizeof(struct sockaddr_in), AF_INET, 0 };
        int error_num = 0;

        if ( pwsf_LocalIPList(&ipList) == kCPSUtilOK )
        {
            inOutHostStr[0] = 0;
            for ( idx = 0; ipList[idx] != 0 && inOutHostStr[0] == 0; idx++ )
            {
                addr.sin_addr.s_addr = htonl( ipList[idx] );
                hostEnt = getipnodebyaddr( &addr, sizeof(struct sockaddr_in), AF_INET, &error_num );
                if ( hostEnt != NULL ) {
                    if ( hostEnt->h_name != NULL ) {
                        strlcpy( inOutHostStr, hostEnt->h_name, maxHostStrLen );
                        result = 0;
                    }
                    freehostent( hostEnt );
                }
            }
        }
    }

    // last resort
    if ( result != 0 )
        result = gethostname( inOutHostStr, maxHostStrLen );

    return result;
}
*/
#endif


#pragma mark -
#pragma mark DIGEST-MD5 Server Challenge
#pragma mark -

CFStringRef
ODKCopyAttrFromRecord(ODRecordRef record, CFStringRef attribute)
{
    CFErrorRef error = NULL;
    CFArrayRef values = ODRecordCopyValues(record, attribute, &error);
    CFStringRef result = NULL;

    if (values) {
        if (CFArrayGetCount(values) == 1) {
            result = CFArrayGetValueAtIndex(values, 0);
            CFRetain(result);
        }
        CFRelease(values);
    }

    CF_SAFE_RELEASE(error);
    return result;
}

int
ODKCreateSession(/*CFStringRef applicationName, CFStringRef serviceName, CFStringRef hostName,*/ ODKSession **out)
{
    ODKSessionPriv *session = 0;

#if 0
    ODK_PARAM_ASSERT(applicationName != NULL);
    ODK_PARAM_ASSERT(serviceName != NULL);
#endif
    if (out == 0)
        return -1;

    *out = 0;

    session = (void*)calloc(1, sizeof(*session));
    if (session == 0) {
         /* Out of memory */
        return -1;
    }
    else {
#if 0
        session->applicationName = CFRetain(applicationName);
        session->serviceName = CFRetain(serviceName);
        if (hostName != NULL)
            session->hostName = CFRetain(hostName);
#endif

        session->useActiveDirectory = kODKActiveDirectoryUnknown;

        session->adNode = ODKGetADInfo((ODKSession*)session)->adNode;
        if (session->adNode)
            CFRetain(session->adNode);

        *out = (ODKSession*)session;
    }

    return 0;
}

int
ODKFlushSession(ODKSession *session)
{
    int retval = -1;
    ODKSessionPriv *_session = ODK_SESSION_PRIV(session);

    ODK_PARAM_ASSERT(session != 0);

    CF_SAFE_RELEASE(_session->userRecord);
    assert(_session->userRecord == 0);

    CF_SAFE_RELEASE(_session->adNode);
    assert(_session->adNode == 0);

    retval = 0;
done:
    return retval;    
}

int
ODKDeleteSession(ODKSession **out)
{
    assert(out != 0);
    if (out != 0) {
        ODKSessionPriv *session = *(ODKSessionPriv**)out;
        if (session != 0) {
#if 0
            CF_SAFE_RELEASE(session->applicationName);
            CF_SAFE_RELEASE(session->serviceName);
            CF_SAFE_RELEASE(session->hostName);
#endif

            CF_SAFE_RELEASE(session->authMethod);
            CF_SAFE_RELEASE(session->userName);
            CF_SAFE_RELEASE(session->userRecord);
            CF_SAFE_RELEASE(session->serverChallenge);
            CF_SAFE_RELEASE(session->serverResponse);
            CF_SAFE_RELEASE(session->adNode);

            memset(session, 0, sizeof(*session));

            free(session);
        }
        *out = 0;
    }

    return 0;
}

int
ODKResetODSearchNode(ODKSession *session)
{
    CF_SAFE_RELEASE(gODSearchNode);
    gODSearchNode = NULL;
    return 0;
}

int
ODKPossiblyResetODSearchNode(ODKSession *session, CFErrorRef error)
{
    if (error != NULL && gODSearchNode != NULL) {
        if (! IS_EXPECTED_DS_ERROR(CFErrorGetCode(error))) {
            /* this is an unexpected error, let's flush the search node */
            ODK_LOG_CFERROR(LOG_DEBUG, "Flushing OD search node because of unexpected error", error);
            ODKResetODSearchNode(session);
        }
    }

    return 0;
}

int
ODKConfigureUserRecord(ODKSession *session)
{
    static CFTypeRef vals[1];
    static CFArrayRef reqAttrs = NULL;
    ODKSessionPriv *_session = ODK_SESSION_PRIV(session);
    int retval = -1;
    CFErrorRef cfError = NULL;

    ODK_PARAM_ASSERT(session != 0);
    ODK_PARAM_ASSERT(_session->userName != NULL);

    if (_session->userRecord != NULL) {
        ODK_LOG(LOG_DEBUG, "User record already configured");
        goto done;
    }

    if (reqAttrs == NULL) {
        vals[0] = CFSTR(kDSAttributesStandardAll);
        reqAttrs = CFArrayCreate(NULL, vals, 1, &kCFTypeArrayCallBacks);
        if (reqAttrs == 0) {
            ODK_LOG(LOG_ERR, "Out of memory");
            goto done;
        }
    }

    if (gODSearchNode != NULL) {
        /* check that the cached node is still good, that DirectoryService hasn't crashed */
        if (ODKNodeIsStale(gODSearchNode)) {
            ODK_LOG(LOG_DEBUG, "Resetting invalid OD search node");
            ODKResetODSearchNode(session);
            /* gODSearchNode will be re-created below */
        }
    }

    if (gODSearchNode == NULL) {
        gODSearchNode = ODNodeCreateWithNodeType(kCFAllocatorDefault, kODSessionDefault, kODNodeTypeAuthentication, &cfError);
        if (gODSearchNode == NULL) {
            ODK_LOG(LOG_ERR, "Failed to obtain search node");
            goto done;
        }
    }

    _session->userRecord = ODNodeCopyRecord(gODSearchNode,  CFSTR(kDSStdRecordTypeUsers), _session->userName, reqAttrs, &cfError);
    if (cfError != NULL) {
        ODKPossiblyResetODSearchNode(session, cfError);
        ODK_LOG_CFERROR(LOG_NOTICE, "Failed to find user", cfError);
        goto done;
    }
    else if (_session->userRecord == NULL) {
        ODK_LOG(LOG_NOTICE, "Failed to find user");
        goto done;
    }

    retval = 0;
done:
    CF_SAFE_RELEASE(cfError);

    return retval;
}

int
ODKIsActiveDirectoryRecord(ODRecordRef record)
{
    int isActiveDirectory = 0;

    if (record != 0) {
        CFStringRef metaNodeValue = ODKCopyAttrFromRecord(record, CFSTR(kDSNAttrMetaNodeLocation));
        if (metaNodeValue != NULL) {
            isActiveDirectory = CFStringHasPrefix(metaNodeValue, CFSTR("/Active Directory/"));
            CFRelease(metaNodeValue);
        }
    }

    return isActiveDirectory;
}

int
ODKUseActiveDirectory(ODKSession *session)
{
    ODKSessionPriv *_session = ODK_SESSION_PRIV(session);

    ODK_PARAM_ASSERT(session != 0);

    if (_session->useActiveDirectory == kODKActiveDirectoryUnknown) {
        if (ODKHasUserRecord(session)
         && ODKUserRecordIsActiveDirectory(session)) {
            _session->useActiveDirectory = kODKActiveDirectoryPresent;
        }
        else if (_session->adNode != NULL) {
            _session->useActiveDirectory = kODKActiveDirectoryPresent;
        }
        else {
            _session->useActiveDirectory = kODKActiveDirectoryAbsent;
        }
    }

    return _session->useActiveDirectory == kODKActiveDirectoryPresent;
}

int
ODKHasUserRecord(ODKSession *session)
{
    ODKSessionPriv *_session = ODK_SESSION_PRIV(session);
    return _session->userRecord != NULL;
}

int
ODKUserRecordIsActiveDirectory(ODKSession *session)
{
    ODKSessionPriv *_session = ODK_SESSION_PRIV(session);
    assert(_session->userRecord != NULL);
    if (ODKIsActiveDirectoryRecord(_session->userRecord)) {
        _session->useActiveDirectory = kODKActiveDirectoryPresent;
        return 1;
    }
    return 0;
}

int
ODKConfigureServerChallenge(ODKSession *session)
{
    int retval = -1;
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    ODKSessionPriv *_session = ODK_SESSION_PRIV(session);

    ODK_PARAM_ASSERT(session != 0);

    if (_session->serverChallenge != NULL) {
        ODK_LOG(LOG_DEBUG, "Server challenge already configured, skip");
        retval = 0;
        goto done;
    }

    NSFileHandle *fileHandle = [NSFileHandle fileHandleForReadingAtPath:@"/dev/random"];
    if (fileHandle == nil) {
        ODK_LOG(LOG_ERR, "Error opening /dev/random, cannot generate digest challenge");
        goto done;
    }
    NSData *randomData = [fileHandle readDataOfLength:36];
    [fileHandle closeFile];

    NSString *nonce = [randomData base64Encoding];
    NSString *realm = [XSAuthenticator defaultRealm];

    NSString *chal = [NSString stringWithFormat:@"nonce=\"%@\",realm=\"%@\",charset=utf-8,qop=\"auth\",algorithm=md5-sess", nonce, realm];
    CFDataRef cfServerResp = (CFDataRef)[chal dataUsingEncoding: NSUTF8StringEncoding];

    if (cfServerResp == NULL)
        goto done;

    _session->serverChallenge = CFRetain(cfServerResp);

    retval = 0;

done:
    [pool drain];
    return retval;
}

int
ODKCopyServerChallenge(ODKSession *session, CFStringRef authMethod, CFStringRef userName, CFDataRef *out)
{
    int retval = -1;
    ODKSessionPriv *_session = ODK_SESSION_PRIV(session);

    ODK_PARAM_ASSERT(session != 0);
    ODK_PARAM_ASSERT(authMethod != NULL);
    ODK_PARAM_ASSERT(out != 0);

    *out = 0;

    CF_SAFE_RELEASE(_session->authMethod);  /* possibly already set */
    _session->authMethod = CFRetain(authMethod);

    if (userName != NULL) {
        CF_SAFE_RELEASE(_session->userName);  /* possibly already set */
        _session->userName = CFRetain(userName);
        if (ODKConfigureUserRecord(session) != 0)
            goto done;
    }

    if (ODKConfigureServerChallenge(session))
        goto done;

    *out = CFRetain(_session->serverChallenge);

    retval = 0;

done:
    return retval;
}

int
ODKSetServerChallenge(ODKSession *session, CFStringRef authMethod, CFStringRef userName, CFDataRef serverChallenge)
{
    ODKSessionPriv *_session = ODK_SESSION_PRIV(session);
    int retval = -1;

    ODK_PARAM_ASSERT(session != 0);
    ODK_PARAM_ASSERT(authMethod != NULL);
    ODK_PARAM_ASSERT(serverChallenge != NULL);

    CF_SAFE_RELEASE(_session->authMethod);  /* possibly already set */
    _session->authMethod = CFRetain(authMethod);

    if (userName) {
        CF_SAFE_RELEASE(_session->userName);  /* possibly already set */
        _session->userName = CFRetain(userName);
        if (ODKConfigureUserRecord(session) != 0)
            goto done;
    }

    CF_SAFE_RELEASE(_session->serverChallenge);  /* possibly already set */
    _session->serverChallenge = CFRetain(serverChallenge);

    retval = 0;

done:
    return retval;
}

int
ODKParseUsername(ODKSession *session, CFStringRef client, CFStringRef *userNameOut)
{
    ODKSessionPriv *_session = ODK_SESSION_PRIV(session);
    int retval = -1;
    char *in = 0;

    *userNameOut = 0;

    _session->buffer[0] = '\0';

    if (!CFStringGetCString(client, _session->buffer, sizeof(_session->buffer), kCFStringEncodingUTF8)) {
        ODK_LOG(LOG_ERR, "Parse failure");
        goto done;
    }

    in = _session->buffer;

    /* parse what we got */
    while (in[0] != '\0') {
        char *name = 0;
        char *value = 0;

        ODKGetPair(&in, &name, &value);
        if (name == 0)
            break;

        if (value == 0)
            continue;

        if (strcasecmp(name, "username") == 0) {
            *userNameOut  = CFStringCreateWithCString(kCFAllocatorDefault, value, kCFStringEncodingUTF8);
            if (*userNameOut == NULL) {
                ODK_LOG(LOG_ERR, "Re-encoding failure");
                goto done;
            }

#if 1
            break;
#else
        } else if (strcasecmp(name, "authzid") == 0) {
            *authzidOut = strdup(value);
        } else if (strcasecmp(name, "algorithm") == 0) {
            *algorithmOut = strdup(value);
#endif
        }
    }

    retval = 0;
done:
    return retval;
}

int
ODKRecordVerifyPasswordExtended(ODKSession *session,
                                ODRecordRef userRecord, CFStringRef authMethod, CFArrayRef inItems,
                                CFArrayRef *outItems, ODContextRef *outContext, CFErrorRef *outError)
{
    int success = 0;
    CFStringRef cfNodeName = NULL;
    char nodename[512];
    char username[512];
    char serverchall[512];
    char clientresp[512];
    char *serverrespstr = 0;

    cfNodeName = ODKCopyAttrFromRecord(userRecord, CFSTR(kDSNAttrMetaNodeLocation));
    if (cfNodeName == NULL) {
        // shouldn't happen, but use the normal ODRecordVerifyPassword if
        // the node name isn't available
        return ODRecordVerifyPasswordExtended(userRecord, authMethod, inItems, outItems, outContext, outError);
    }

    if (outItems) *outItems = NULL;
    if (outContext) *outContext = NULL;
    if (outError) *outError = NULL;

    CFStringGetCString(cfNodeName, nodename, sizeof(nodename)-1, kCFStringEncodingUTF8);
    CFStringGetCString(CFArrayGetValueAtIndex(inItems, 0), username, sizeof(username)-1, kCFStringEncodingUTF8);
    strlcpy(serverchall, (char*)CFDataGetBytePtr(CFArrayGetValueAtIndex(inItems, 1)), sizeof(serverchall));
    CFStringGetCString(CFArrayGetValueAtIndex(inItems, 2), clientresp, sizeof(clientresp)-1, kCFStringEncodingUTF8);

    if (FasterAuthentication(nodename, username, serverchall, clientresp, &serverrespstr) != 0)
        goto done;

    CFMutableArrayRef results = CFArrayCreateMutable(NULL, 1, &kCFTypeArrayCallBacks);
    CFDataRef serverresp = NULL;
    ODKMaybeCreateData(session, &serverresp, serverrespstr, strlen(serverrespstr));
    CFArrayAppendValue(results, serverresp);
    CFRelease(serverresp);

    *outItems = results;

    success = 1;
done:
    CF_SAFE_RELEASE(cfNodeName);
    if (serverrespstr) free(serverrespstr);
    return success;
}

int
ODKVerifyClientRequestFixed(ODKSession *session, CFStringRef clientRequest)
{
    ODKSessionPriv *_session = ODK_SESSION_PRIV(session);
    int retval = -1;
    int wasAnonymous;
    CFMutableArrayRef cfArrayBuf = NULL;
    CFErrorRef cfOutError = NULL;
    CFArrayRef cfOutItems = NULL;
    int success;

    ODK_PARAM_ASSERT(session != 0);
    ODK_PARAM_ASSERT(clientRequest != NULL);
    ODK_PARAM_ASSERT(_session->authMethod != NULL);
    ODK_PARAM_ASSERT(_session->serverChallenge != NULL);

    if (_session->serverResponse != NULL) {
        ODK_LOG(LOG_DEBUG, "Server response already configured");
        goto done;
    }

    wasAnonymous = ! ODKHasUserRecord(session);

    if (wasAnonymous) {
        if (ODKParseUsername(session, clientRequest, &_session->userName) != 0)
            goto done;

        if (_session->userName == NULL) {
            ODK_LOG(LOG_ERR, "Missing username information");
            goto done;
        }

        if (ODKConfigureUserRecord(session) != 0)
            goto done;
    }

    cfArrayBuf = CFArrayCreateMutable(NULL, 3, &kCFTypeArrayCallBacks);
    if (cfArrayBuf == 0) {
        ODK_LOG(LOG_ERR, "Out of memory");
        goto done;
    }

    CFStringRef authMethod = CFStringCreateWithCString(kCFAllocatorDefault, "AUTHENTICATE", kCFStringEncodingUTF8);
    CFArrayAppendValue(cfArrayBuf, _session->userName);
    CFArrayAppendValue(cfArrayBuf, _session->serverChallenge);
    CFArrayAppendValue(cfArrayBuf, clientRequest);
    CFArrayAppendValue(cfArrayBuf, authMethod);

    success = ODRecordVerifyPasswordExtended(_session->userRecord,
                               kODAuthenticationTypeDIGEST_MD5, cfArrayBuf,
                               &cfOutItems, NULL, &cfOutError);
    CF_SAFE_RELEASE(authMethod);

    if (!success) {
        if (ODKIsActiveDirectoryRecord(_session->userRecord))
                ODKPossiblyResetADInfo(session, cfOutError); 

        if (cfOutError)
            ODK_LOG_CFERROR(LOG_INFO, "Unable to authenticate", cfOutError);
        else
            ODK_LOG(LOG_INFO, "Unable to authenticate");
        goto done;
    }

    CFDataRef cfServerResp = CFArrayGetValueAtIndex(cfOutItems, 0);
    if (cfServerResp == 0) {
        ODK_LOG(LOG_ERR, "Missing server response");
        goto done;
    }

    _session->serverResponse = CFRetain(cfServerResp);

    retval = 0;

done:
    CF_SAFE_RELEASE(cfOutError);
    CF_SAFE_RELEASE(cfOutItems);
    CF_SAFE_RELEASE(cfArrayBuf);

    return retval;
}

int
ODKVerifyClientRequest(ODKSession *session, CFDataRef clientRequest)
{
    int retval = -1;
    CFRange range = { 0 };
    CFStringRef fixed = NULL;
    CFStringRef request = CFStringCreateWithBytes(kCFAllocatorDefault,
                                            CFDataGetBytePtr(clientRequest),
                                            CFDataGetLength(clientRequest),
                                            kCFStringEncodingUTF8,
                                            (Boolean)false);
    if (request == NULL) {
        ODK_LOG(LOG_ERR, "Unable to create string from request");
        goto done;
    }

    /* our OD needs "algorithm=md5-sess" present, so make sure it's there */
    range = CFStringFind(request, CFSTR("algorithm="), kCFCompareCaseInsensitive);
    if (range.location == kCFNotFound) {
        fixed = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                         CFSTR("%@,algorithm=md5-sess"),
                                         request);
        if (fixed == NULL) {
            ODK_LOG(LOG_ERR, "Unable to create mutable string from request");
            goto done;
        }

        retval = ODKVerifyClientRequestFixed(session, fixed);
    }
    else {
        retval = ODKVerifyClientRequestFixed(session, request);
    }

done:
    CF_SAFE_RELEASE(fixed);
    CF_SAFE_RELEASE(request);

    return retval;
}

int
ODKCopyServerResponse(ODKSession *session, CFStringRef *userNameOut, CFDataRef *serverResponseOut)
{
    ODKSessionPriv *_session = ODK_SESSION_PRIV(session);

    ODK_PARAM_ASSERT(session != 0);
    ODK_PARAM_ASSERT(userNameOut != 0);
    ODK_PARAM_ASSERT(serverResponseOut != 0);

    *userNameOut = 0;
    *serverResponseOut = 0;

    if (_session->userName == 0) {
        ODK_LOG(LOG_ERR, "Missing user name");
        return -1;
    }

    if (_session->serverResponse == 0) {
        ODK_LOG(LOG_ERR, "Missing server response");
        return -1;
    }

    *userNameOut = CFRetain(_session->userName);
    *serverResponseOut = CFRetain(_session->serverResponse);

    return 0;
}

