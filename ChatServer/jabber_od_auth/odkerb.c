/*
 *  odkerb.c
 *
 *  obtain IM handle using DS/OD
 *
 *  korver@apple.com
 *
 *  Copyright (c) 2009, Apple Inc. All rights reserved.
 *
 */

#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>
#include <sys/errno.h>
#include <OpenDirectory/OpenDirectory.h>
#include <OpenDirectory/OpenDirectoryPriv.h>
#include <DirectoryService/DirectoryService.h>
#include "odkerb.h"
#include "dserr.h"

static ODNodeRef gSearchNode = NULL;

static Boolean odkerb_CFStringHasPrefixWithOptions(CFStringRef theString, CFStringRef prefix, CFOptionFlags searchOptions);
static Boolean odkerb_CFStringHasSuffixWithOptions(CFStringRef theString, CFStringRef suffix, CFOptionFlags searchOptions);

static CFStringRef odkerb_create_alleged_principal_id(char *service_principal_id);
static CFStringRef odkerb_create_config_record_name(CFStringRef cfPrincipalID);
static CFStringRef odkerb_create_short_name(char *service_principal_id);
static CFStringRef odkerb_create_alleged_alt_security_identity(CFStringRef principalID);

static int odkerb_has_foreign_realm(char *service_principal_id);

static int odkerb_copy_user_record_with_alt_security_identity(CFStringRef principalID, ODRecordRef *out);
static int odkerb_copy_search_node_with_config_record_name(CFStringRef configRecordName, ODNodeRef *out);
static int odkerb_copy_user_record_with_short_name(CFStringRef shortName, ODNodeRef searchNode, ODRecordRef *out);
static int odkerb_get_im_handle_with_user_record(ODRecordRef userRecord, CFStringRef imType, CFStringRef realm, CFStringRef allegedShortName, char im_handle[], size_t im_handle_size);
static int odkerb_get_fabricated_im_handle(ODRecordRef userRecord, CFStringRef allegedShortName, CFStringRef realm, char im_handle[], size_t im_handle_size);

static void odkerb_log_debug(char *fmt, ...);
static void odkerb_log(int priority, char *fmt, ...);
static void odkerb_log_cferror(int priority, char *message, CFErrorRef error);
static void odkerb_log_cfstring(int priority, char *message, CFStringRef string);


#define ODKERB_PARAM_ASSERT(expression) \
        do { if (!(expression)) { ODKERB_LOG(LOG_ERR, "Bad parameter"); return -1; } } while(0)

#define CF_SAFE_RELEASE(cfobj) \
        do { if ((cfobj) != NULL) CFRelease((cfobj)); cfobj = NULL; } while (0)

#define ODKERB_LOG(priority, message) \
        odkerb_log(priority, "%s", message)

#define ODKERB_LOG_CFERROR(priority, message, error) \
        odkerb_log_cferror(priority, message, error)

#define ODKERB_LOG_CFSTRING(priority, message, string) \
        odkerb_log_cfstring(priority, message, string)

#define ODKERB_LOG_ERRNO(priority, errnum) \
        odkerb_log(priority, "%s", strerror(errnum));

/* the printf() calls are so that gcc will warn when/if the format string has issues */
#define ODKERB_LOG1(priority, fmt, arg0) \
        do { odkerb_log(priority, fmt, arg0); if (0) printf(fmt, arg0); } while (0)


void
odkerb_log(int priority, char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsyslog(priority, fmt, ap);
    va_end(ap);
}

void
odkerb_log_cferror(int priority, char *message, CFErrorRef error)
{
    if (error == NULL) {
        odkerb_log(priority, message);
    }
    else {
        char buf[256];
        CFStringRef errorString = CFErrorCopyFailureReason(error);
        CFStringGetCString(errorString, buf, sizeof(buf), kCFStringEncodingUTF8);
        odkerb_log(priority, "%s: %s", message, buf);
    }
}

void
odkerb_log_cfstring(int priority, char *message, CFStringRef string)
{
    if (string == NULL) {
        odkerb_log(priority, "%s", message);
    }
    else {
        char buf[256];
        if (CFStringGetCString(string, buf, sizeof(buf), kCFStringEncodingUTF8))
            odkerb_log(priority, "%s: %s", message, buf);
        else
            odkerb_log(priority, "%s", message);
    }
}


Boolean
odkerb_CFStringHasPrefixWithOptions(CFStringRef theString, CFStringRef prefix, CFOptionFlags searchOptions)
{
    /* restrict search options to those that make sense */
    searchOptions &= (kCFCompareCaseInsensitive | kCFCompareNonliteral | kCFCompareLocalized | kCFCompareDiacriticInsensitive | kCFCompareWidthInsensitive);

    /* anchor search to the start of the string */
    searchOptions |= kCFCompareAnchored;

    return CFStringFindWithOptions(theString, prefix, CFRangeMake(0, CFStringGetLength(theString)),
                                   searchOptions, NULL);
}

Boolean
odkerb_CFStringHasSuffixWithOptions(CFStringRef theString, CFStringRef suffix, CFOptionFlags searchOptions)
{
    /* restrict search options to those that make sense */
    searchOptions &= (kCFCompareCaseInsensitive | kCFCompareNonliteral | kCFCompareLocalized | kCFCompareDiacriticInsensitive | kCFCompareWidthInsensitive);

    /* make the suffix search faster, anchor it to the end of the string */
    searchOptions |= (kCFCompareBackwards | kCFCompareAnchored);

    return CFStringFindWithOptions(theString, suffix, CFRangeMake(0, CFStringGetLength(theString)),
                                   searchOptions, NULL);
}

int
odkerb_has_foreign_realm(char *service_principal_id)
{
    /* if it has 2 '@' signs */

    char *c = strchr(service_principal_id, '@');
    if (c) {
        c = strchr(c+1, '@');
        if (c)
            return 1;
    }

    return 0;
}

CFStringRef
odkerb_create_alleged_principal_id(char *service_principal_id)
{
    CFStringRef cfPrincipalID = NULL;

    assert(service_principal_id != 0);

    char *s = strdup(service_principal_id);
    if (s) {
        char *c = strrchr(s, '@');
        if (c)
            *c = '\0';
        else
            ODKERB_LOG1(LOG_WARNING, "Bad service principal: %s", service_principal_id);

        cfPrincipalID = CFStringCreateWithCString(kCFAllocatorDefault, s, kCFStringEncodingUTF8);

        free(s);
    }

    if (cfPrincipalID == NULL)
        ODKERB_LOG_ERRNO(LOG_ERR, ENOMEM);

    return cfPrincipalID;
}

CFStringRef
odkerb_create_alleged_alt_security_identity(CFStringRef principalID)
{
    CFStringRef cfAltSecurityIdentity = NULL;

    assert(principalID != NULL);

    cfAltSecurityIdentity = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                                     CFSTR("Kerberos:%@"), principalID);
    if (cfAltSecurityIdentity == NULL)
        ODKERB_LOG_ERRNO(LOG_ERR, ENOMEM);

    return cfAltSecurityIdentity;
}

CFStringRef
odkerb_create_config_record_name(CFStringRef principalID)
{
    CFStringRef cfConfigRecordName = NULL;
    char buffer[1024];

    assert(principalID != NULL);

    if (CFStringGetCString(principalID, buffer, sizeof(buffer)-1, kCFStringEncodingUTF8) == FALSE)
        return NULL;

    /* use everything after the '@' sign */
    char *c = strchr(buffer, '@');
    if (c != 0) {
        ++c;

        cfConfigRecordName = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                                      CFSTR("Kerberos:%s"), c);
        if (cfConfigRecordName == NULL)
           ODKERB_LOG_ERRNO(LOG_ERR, ENOMEM);
    }
    else {
        ODKERB_LOG_CFSTRING(LOG_WARNING, "Bad principal", principalID);
    }

    return cfConfigRecordName;
}

CFStringRef
odkerb_create_short_name(char *service_principal_id)
{
    CFStringRef cfShortName = NULL;

    assert(service_principal_id != 0);

    /* chop off everything after the first '@' sign */
    char *s = strdup(service_principal_id);
    if (s != 0) {
        char *c = strchr(s, '@');
        if (c != 0)
            *c = '\0';

        cfShortName = CFStringCreateWithCString(kCFAllocatorDefault, s, kCFStringEncodingUTF8);

        free(s);
    }

    if (cfShortName == NULL)
        ODKERB_LOG_ERRNO(LOG_ERR, ENOMEM);

    return cfShortName;
}

int
odkerb_configure_search_node(void)
{
    int retval = -1;
    CFErrorRef cfError = NULL;

    if (gSearchNode != NULL) {
        if (ODNodeGetName(gSearchNode) == NULL) {
            ODKERB_LOG(LOG_DEBUG, "Flushing search node");
            CF_SAFE_RELEASE(gSearchNode);
            gSearchNode = NULL;
        }
    }
    
    if (gSearchNode == NULL) {
        gSearchNode = ODNodeCreateWithNodeType(kCFAllocatorDefault, kODSessionDefault,
                                               kODNodeTypeAuthentication, &cfError);
        if (gSearchNode == NULL || cfError != NULL) {
            ODKERB_LOG_CFERROR(LOG_ERR, "Unable to get a reference to the search node", cfError);
            goto failure;
        }
    }

    retval = 0;
failure:
    CF_SAFE_RELEASE(cfError);
    return retval;
}

int
odkerb_possibly_reset_search_node(CFErrorRef error)
{
    if (error != NULL && gSearchNode != NULL) {
        if (! IS_EXPECTED_DS_ERROR(CFErrorGetCode(error))) {
            /* this is an unexpected error, let's flush the search node */
            ODKERB_LOG_CFERROR(LOG_DEBUG, "Flushing search node because of unexpected error", error);
            CF_SAFE_RELEASE(gSearchNode);
            gSearchNode = NULL;
        }
    }

    return 0;
}

int
odkerb_copy_user_record_with_alt_security_identity(CFStringRef principalID, ODRecordRef *out)
{
    int retval = -1;
    CFStringRef cfAltSecurityIdentity = NULL;
    CFErrorRef cfError = NULL;
    ODQueryRef cfQueryRef = NULL;
    CFArrayRef cfUserRecords = NULL;

    ODKERB_PARAM_ASSERT(principalID != NULL);
    ODKERB_PARAM_ASSERT(out != 0);

    *out = NULL;

    if (odkerb_configure_search_node() != 0)
        goto failure;

    cfAltSecurityIdentity = odkerb_create_alleged_alt_security_identity(principalID);
    if (cfAltSecurityIdentity == NULL)
        goto failure;

    cfQueryRef = ODQueryCreateWithNode(kCFAllocatorDefault,
                                       gSearchNode,
                                       kODRecordTypeUsers,
                                       kODAttributeTypeAltSecurityIdentities,
                                       kODMatchInsensitiveEqualTo,
                                       cfAltSecurityIdentity,
                                       kODAttributeTypeRecordName,
                                       2, &cfError);
    if (cfQueryRef == NULL || cfError != NULL) {
        ODKERB_LOG_CFERROR(LOG_ERR, "Unable to query the search node", cfError);
        goto failure;
    }

    cfUserRecords = ODQueryCopyResults(cfQueryRef, false, &cfError);
    if (cfUserRecords == NULL || cfError != NULL) {
        ODKERB_LOG_CFERROR(LOG_ERR, "Unable to find user record", cfError);
        goto failure;
    }
    else if (CFArrayGetCount(cfUserRecords) == 0) {
        ODKERB_LOG_CFSTRING(LOG_INFO, "Unable to find user record", cfAltSecurityIdentity);
        goto failure;
    }
    else if (CFArrayGetCount(cfUserRecords) > 1) {
        ODKERB_LOG_CFSTRING(LOG_DEBUG, "Too many user records, using the first one", cfAltSecurityIdentity);
    }

    ODRecordRef cfUserRecord = (ODRecordRef)CFArrayGetValueAtIndex(cfUserRecords, 0);
    *out = (ODRecordRef)CFRetain(cfUserRecord);

    retval = 0;
failure:
    if (cfError != NULL)
        odkerb_possibly_reset_search_node(cfError);
    CF_SAFE_RELEASE(cfError);
    CF_SAFE_RELEASE(cfAltSecurityIdentity);
    CF_SAFE_RELEASE(cfQueryRef);
    CF_SAFE_RELEASE(cfUserRecords);

    return retval;
}

int
odkerb_copy_search_node_with_config_record_name(CFStringRef configRecordName, ODNodeRef *out)
{
    static CFTypeRef cfVals[2];
    static CFArrayRef cfReqAttrs = NULL;
    int retval = -1;
    ODRecordRef cfConfigRecord = NULL;
    CFArrayRef cfOriginalNodeNames = NULL;
    CFErrorRef cfError = NULL;

    ODKERB_PARAM_ASSERT(configRecordName != NULL);
    ODKERB_PARAM_ASSERT(out != 0);

    *out = NULL;

    if (odkerb_configure_search_node() != 0)
        goto failure;

    if (cfReqAttrs == NULL) {
        /* hint for should be fetched */
        cfVals[0] = kODAttributeTypeOriginalNodeName;
        cfVals[1] = kODAttributeTypeIMHandle;
        cfReqAttrs = CFArrayCreate(NULL, cfVals, 1, &kCFTypeArrayCallBacks);
    }

    cfConfigRecord = ODNodeCopyRecord(gSearchNode, kODRecordTypeConfiguration,
                                      configRecordName, cfReqAttrs, &cfError);
    if (cfConfigRecord == NULL || cfError != NULL) {
        ODKERB_LOG_CFERROR(LOG_ERR, "Unable to find the configuration record", cfError);
        goto failure;
    }

    cfOriginalNodeNames = ODRecordCopyValues(cfConfigRecord, kODAttributeTypeOriginalNodeName, &cfError);
    if (cfOriginalNodeNames == NULL || cfError != NULL) {
        ODKERB_LOG_CFERROR(LOG_ERR, "Unable to find the original node name", cfError);
        goto failure;
    }
    else if (CFArrayGetCount(cfOriginalNodeNames) == 0) {
        ODKERB_LOG_CFSTRING(LOG_ERR, "Unable to find the original node name", configRecordName);
        goto failure;
    }
    else if (CFArrayGetCount(cfOriginalNodeNames) > 1) {
        ODKERB_LOG_CFSTRING(LOG_DEBUG, "Too many original node names, using the first", configRecordName);
    }

    CFStringRef cfNodeName = CFArrayGetValueAtIndex(cfOriginalNodeNames, 0);
    if (cfNodeName == NULL) {
        ODKERB_LOG(LOG_ERR, "Missing original node name");
        goto failure;
    }

    ODNodeRef cfSearchNode2 = ODNodeCreateWithName(kCFAllocatorDefault, kODSessionDefault,
                                                   cfNodeName, &cfError);
    if (cfSearchNode2 == NULL || cfError != NULL) {
        ODKERB_LOG_CFERROR(LOG_ERR, "Unable to create the search record", cfError);
        goto failure;
    }

    *out = cfSearchNode2;

    retval = 0;
failure:
    if (cfError != NULL)
        odkerb_possibly_reset_search_node(cfError);
    CF_SAFE_RELEASE(cfError);
    CF_SAFE_RELEASE(cfOriginalNodeNames);
    CF_SAFE_RELEASE(cfConfigRecord);

    return retval;
}

int
odkerb_copy_user_record_with_short_name(CFStringRef shortName, ODNodeRef searchNode, ODRecordRef *out)
{
    static CFTypeRef cfVals[2];
    static CFArrayRef cfReqAttrs = NULL;
    int retval = -1;
    CFErrorRef cfError = NULL;

    ODKERB_PARAM_ASSERT(shortName != NULL);
    ODKERB_PARAM_ASSERT(searchNode != NULL);
    ODKERB_PARAM_ASSERT(out != 0);

    *out = NULL;

    if (cfReqAttrs == NULL) {
        /* hint for should be fetched */
        cfVals[0] = kODAttributeTypeOriginalNodeName;
        cfVals[1] = kODAttributeTypeIMHandle;
        cfReqAttrs = CFArrayCreate(NULL, cfVals, 1, &kCFTypeArrayCallBacks);
    }

    ODRecordRef cfUserRecord = ODNodeCopyRecord(searchNode, kODRecordTypeUsers,
                                                shortName, cfReqAttrs, &cfError);
    if (cfUserRecord == NULL || cfError != NULL) {
        ODKERB_LOG_CFERROR(LOG_DEBUG, "Unable to find the user record", cfError);
        goto failure;
    }

    *out = cfUserRecord;

    retval = 0;
failure:
    CF_SAFE_RELEASE(cfError);

    return retval;
}

int
odkerb_get_im_handle_with_user_record(ODRecordRef userRecord, CFStringRef imType, CFStringRef realm, CFStringRef allegedShortName, char im_handle[], size_t im_handle_size)
{
    int retval = -1;
    CFArrayRef cfIMHandles = NULL;
    CFErrorRef cfError = NULL;
    CFMutableArrayRef cfMatches = NULL;
    CFStringRef cfRealID = NULL;
    int i;

    ODKERB_PARAM_ASSERT(userRecord != NULL);
    ODKERB_PARAM_ASSERT(allegedShortName != NULL);
    ODKERB_PARAM_ASSERT(im_handle != 0);
    ODKERB_PARAM_ASSERT(im_handle_size > 0);

    *im_handle = '\0';

    cfIMHandles = ODRecordCopyValues(userRecord, kODAttributeTypeIMHandle, &cfError);
    if (cfIMHandles == NULL || cfError != NULL) {
        ODKERB_LOG_CFERROR(LOG_ERR, "Unable to obtain IM handles", cfError);
        goto failure;
    }
    else if (CFArrayGetCount(cfIMHandles) == 0) {
        ODKERB_LOG_CFSTRING(LOG_DEBUG, "No IM handles", allegedShortName);
        goto failure;
    }

    /* there could be many IM handles that look plausible, so we heuristically determine which
     * one is the most likely to be the correct one.  imagine, for instance, that the following
     * ones are available:
     *    JABBER: briank@ichatserver.apple.com
     *    JABBER: briank@jabber.com
     *    JABBER: bkorver@ichatserver.apple.com
     *    YAHOO:  bkorver@yahoo.com
     */

    /* first, remove those of the wrong type or realm because they can't possibly be right */

    cfMatches = CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, cfIMHandles);
    if (cfMatches == NULL) {
        ODKERB_LOG_ERRNO(LOG_ERR, ENOMEM);
        goto failure;
    }

    for (i = CFArrayGetCount(cfMatches) - 1; i >= 0; --i) {
        CFStringRef cfID = CFArrayGetValueAtIndex(cfMatches, i);

        if (cfID != NULL
         && odkerb_CFStringHasPrefixWithOptions(cfID, imType, kCFCompareCaseInsensitive)
         && odkerb_CFStringHasSuffixWithOptions(cfID, realm, kCFCompareCaseInsensitive))
            continue;

        /* isn't a match, so remove it from the list */
        CFArrayRemoveValueAtIndex(cfMatches, i);
    }

    CFStringRef match = NULL;

    if (CFArrayGetCount(cfMatches) == 0) {
        ODKERB_LOG_CFSTRING(LOG_INFO, "No IM handles matching type and realm", allegedShortName);
        goto failure;
    }
    else if (CFArrayGetCount(cfMatches) == 1) {
        match = CFArrayGetValueAtIndex(cfMatches, 0);
        goto found_match;
    }

    /* second, attempt to use the short name to disasmbiguate among the several choices */

    for (i = 0; i < CFArrayGetCount(cfMatches); ++i) {
        CFStringRef cfID = CFArrayGetValueAtIndex(cfMatches, i);
        if (cfID == NULL)
            continue;

        CFRange where = CFStringFind(cfID, allegedShortName, kCFCompareCaseInsensitive | kCFCompareDiacriticInsensitive);
        if (where.location != kCFNotFound) {
            match = cfID;
            goto found_match;
        }
    }

    /* at this point, there are several possibilities, but none of them contain the
     * short name, so just choose the first one */
    match = CFArrayGetValueAtIndex(cfMatches, 0);

found_match:
    assert(match != NULL);

    /* the ID is the substring following the IM type specifier prefix (kIMTypeJABBER) */

    assert(CFStringGetLength(match) > CFStringGetLength(imType));

    cfRealID = CFStringCreateWithSubstring(kCFAllocatorDefault, match,
                                           CFRangeMake(CFStringGetLength(imType), CFStringGetLength(match)-CFStringGetLength(imType)));
    if (cfRealID == NULL) {
        ODKERB_LOG_ERRNO(LOG_ERR, ENOMEM);
        goto failure;
    }

    if (CFStringGetCString(cfRealID, im_handle, im_handle_size-1, kCFStringEncodingUTF8) == FALSE) {
        ODKERB_LOG_CFSTRING(LOG_ERR, "Cannot obtain IM handle string", cfRealID);
        goto failure;
    }

    retval = 0;
failure:
    CF_SAFE_RELEASE(cfError);
    CF_SAFE_RELEASE(cfRealID);
    CF_SAFE_RELEASE(cfMatches);
    CF_SAFE_RELEASE(cfIMHandles);

    return retval;
}

int
odkerb_get_fabricated_im_handle(ODRecordRef userRecord, CFStringRef allegedShortName, CFStringRef realm, char im_handle[], size_t im_handle_size)
{
    int retval = -1;
    CFArrayRef cfShortNames = NULL;
    CFStringRef cfIMHandle = NULL;
    CFErrorRef cfError = NULL;
    CFStringRef shortName = allegedShortName;

    ODKERB_PARAM_ASSERT(allegedShortName != NULL);
    ODKERB_PARAM_ASSERT(realm != NULL);
    ODKERB_PARAM_ASSERT(im_handle != 0);
    ODKERB_PARAM_ASSERT(im_handle_size > 0);

    *im_handle = '\0';

    if (userRecord != NULL) {
        /* attempt to get the primary short name from the user record */

        cfShortNames = ODRecordCopyValues(userRecord, kODAttributeTypeRecordName, &cfError);
        if (cfShortNames == NULL || cfError != NULL) {
            ODKERB_LOG_CFERROR(LOG_DEBUG, "Unable to find the short names", cfError);
            /* ignore this error, use the passed in allegedShortName parameter */
        }
        else if (CFArrayGetCount(cfShortNames) == 0) {
            ODKERB_LOG_CFSTRING(LOG_DEBUG, "Unable to find the short names", allegedShortName);
            /* ignore this error, use the passed in allegedShortName parameter */
        }
        else {
            CFStringRef cfShortName = CFArrayGetValueAtIndex(cfShortNames, 0);
            assert(cfShortName != 0);

            shortName = cfShortName;
        }
    }

    cfIMHandle = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@@%@"), shortName, realm);
    if (cfIMHandle == NULL) {
        ODKERB_LOG_ERRNO(LOG_ERR, ENOMEM);
        goto failure;
    }

    if (CFStringGetCString(cfIMHandle, im_handle, im_handle_size-1, kCFStringEncodingUTF8) == FALSE)
        goto failure;

    retval = 0;
failure:
    CF_SAFE_RELEASE(cfError);
    CF_SAFE_RELEASE(cfIMHandle);
    CF_SAFE_RELEASE(cfShortNames);

    return retval;
}

int
odkerb_get_im_handle(char *service_principal_id, char *realm, char *im_type, char im_handle[], size_t im_handle_size)
{
    int retval = -1;
    int is_cross_realm;
    CFStringRef cfAllegedShortName = NULL;
    CFStringRef cfRealm = NULL;
    CFStringRef cfPrincipalID = NULL;
    ODRecordRef cfUserRecord = NULL;
    CFStringRef cfConfigRecordName = NULL;
    ODNodeRef cfSearchNode = NULL;
    CFStringRef cfIMType = NULL;

    ODKERB_PARAM_ASSERT(service_principal_id != 0);
    ODKERB_PARAM_ASSERT(realm != 0);
    ODKERB_PARAM_ASSERT(im_type != 0);
    ODKERB_PARAM_ASSERT(im_handle != 0);
    ODKERB_PARAM_ASSERT(im_handle_size > 0);

    *im_handle = '\0';

    /* configure the short name and realm first because they may be used
     * in the failure handler to fabricate the IM handle */

    cfAllegedShortName = odkerb_create_short_name(service_principal_id);
    if (cfAllegedShortName == NULL) {
        ODKERB_LOG_ERRNO(LOG_ERR, ENOMEM);
        goto failure;
    }

    cfRealm = CFStringCreateWithCString(kCFAllocatorDefault, realm, kCFStringEncodingUTF8);
    if (cfRealm == NULL) {
        ODKERB_LOG_ERRNO(LOG_ERR, ENOMEM);
        goto failure;
    }

    is_cross_realm = odkerb_has_foreign_realm(service_principal_id);
    if (is_cross_realm) {
         cfPrincipalID = odkerb_create_alleged_principal_id(service_principal_id);
    }
    else {
         cfPrincipalID = CFStringCreateWithCString(kCFAllocatorDefault, service_principal_id, kCFStringEncodingUTF8);
         if (cfPrincipalID == 0)
            ODKERB_LOG_ERRNO(LOG_ERR, ENOMEM);
    }
    if (cfPrincipalID == 0)
        goto failure;

    if (odkerb_copy_user_record_with_alt_security_identity(cfPrincipalID, &cfUserRecord) != 0) {
        cfConfigRecordName = odkerb_create_config_record_name(cfPrincipalID);

        if (odkerb_copy_search_node_with_config_record_name(cfConfigRecordName, &cfSearchNode) != 0)
            goto failure;

        if (odkerb_copy_user_record_with_short_name(cfAllegedShortName, cfSearchNode, &cfUserRecord) != 0)
            goto failure;
    }

    cfIMType = CFStringCreateWithCString(kCFAllocatorDefault, im_type, kCFStringEncodingUTF8);
    if (cfIMType == NULL) {
        ODKERB_LOG_ERRNO(LOG_ERR, ENOMEM);
        goto failure;
    }

    if (odkerb_get_im_handle_with_user_record(cfUserRecord, cfIMType, cfRealm, cfAllegedShortName, im_handle, im_handle_size) != 0)
        goto failure;

    retval = 0;
failure:
    if (retval != 0) {
        if (is_cross_realm) {
            ODKERB_LOG_CFSTRING(LOG_WARNING, "Unable to construct IM handle for cross-realm user", cfPrincipalID);
        }
        else {
            if (odkerb_get_fabricated_im_handle(cfUserRecord, cfAllegedShortName, cfRealm, im_handle, im_handle_size) == 0)
                retval = 0;
        }
    }

    CF_SAFE_RELEASE(cfIMType);
    CF_SAFE_RELEASE(cfUserRecord);
    CF_SAFE_RELEASE(cfSearchNode);
    CF_SAFE_RELEASE(cfConfigRecordName);
    CF_SAFE_RELEASE(cfPrincipalID);
    CF_SAFE_RELEASE(cfRealm);
    CF_SAFE_RELEASE(cfAllegedShortName);

    return retval;
}

