/* Copyright (c) 2012-2013 Apple Inc. All Rights Reserved. */

#include "Authorization.h"
#include "authd_private.h"
#include "authutilities.h"
#include "debugging.h"

#include <Security/AuthorizationPriv.h>
#include <Security/AuthorizationDB.h>
#include <Security/AuthorizationTags.h>
#include <Security/AuthorizationTagsPriv.h>
#include <xpc/xpc.h>
#include <xpc/private.h>
#include <mach/mach.h>
#include <syslog.h>
#include <AssertMacros.h>
#include <CoreFoundation/CFXPCBridge.h>

static dispatch_queue_t
get_authorization_dispatch_queue()
{
    static dispatch_once_t onceToken = 0;
    static dispatch_queue_t connection_queue = NULL;
 
    dispatch_once(&onceToken, ^{
        connection_queue = dispatch_queue_create("authorization-connection-queue", DISPATCH_QUEUE_SERIAL);
    });
    
    return connection_queue;
}

static xpc_connection_t
get_authorization_connection()
{
    static xpc_connection_t connection = NULL;
    
    dispatch_sync(get_authorization_dispatch_queue(), ^{
        if (connection == NULL) {
            connection = xpc_connection_create(SECURITY_AUTH_NAME, NULL);

            if (!connection) {
                syslog(LOG_ERR, "Authorization, failed to create xpc connection to %s", SECURITY_AUTH_NAME);
                connection = xpc_connection_create(SECURITY_AUTH_NAME, NULL);
            }
            
            xpc_connection_set_event_handler(connection, ^(xpc_object_t event) {
                if (xpc_get_type(event) == XPC_TYPE_ERROR) {
                    if (event == XPC_ERROR_CONNECTION_INVALID) {
                        syslog(LOG_ERR, "Authorization, server not available");
                    }
                    // XPC_ERROR_CONNECTION_INTERRUPTED
                    // XPC_ERROR_TERMINATION_IMMINENT
                } else {
                    char * desc = xpc_copy_description(event);
                    syslog(LOG_ERR, "Authorization, we should never get messages on this connection: %s", desc);
                    free(desc);
                }
            });
            
            xpc_connection_resume(connection);
            
            // Send
            xpc_object_t message = xpc_dictionary_create(NULL, NULL, 0);
            xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_SETUP);
            mach_port_t bootstrap = MACH_PORT_NULL;
            task_get_bootstrap_port(mach_task_self(), &bootstrap);
            xpc_dictionary_set_mach_send(message, AUTH_XPC_BOOTSTRAP, bootstrap);
            xpc_object_t reply = xpc_connection_send_message_with_reply_sync(connection, message);
            xpc_release_safe(message);
            xpc_release_safe(reply);
        }
    });
    
    return connection;
}

static void
setItemSet(xpc_object_t message, const char * key, const AuthorizationItemSet * itemSet)
{
    xpc_object_t serialized = SerializeItemSet(itemSet);
    if (serialized) {
        xpc_dictionary_set_value(message, key, serialized);
        xpc_release(serialized);
    }
}

OSStatus AuthorizationCreate(const AuthorizationRights *rights,
                    const AuthorizationEnvironment *environment,
                    AuthorizationFlags flags,
                    AuthorizationRef *authorization)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t message = NULL;
    xpc_object_t reply = NULL;

//    require_action(!(rights == NULL && authorization == NULL), done, status = errAuthorizationInvalidSet);
    
    // Send
    message = xpc_dictionary_create(NULL, NULL, 0);
    require_action(message != NULL, done, status = errAuthorizationInternal);
    
    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_CREATE);
    setItemSet(message, AUTH_XPC_RIGHTS, rights);
    setItemSet(message, AUTH_XPC_ENVIROMENT, environment);
    xpc_dictionary_set_uint64(message, AUTH_XPC_FLAGS, flags | (authorization ? 0 : kAuthorizationFlagNoData));
    
    // Reply
    reply = xpc_connection_send_message_with_reply_sync(get_authorization_connection(), message);
    require_action(reply != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal);

    // Status
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);
    
    // Out
    if (authorization && status == errAuthorizationSuccess) {
        size_t len;
        const void * data = xpc_dictionary_get_data(reply, AUTH_XPC_BLOB, &len);
        require_action(data != NULL, done, status = errAuthorizationInternal);
        assert(len == sizeof(AuthorizationBlob));
        
        AuthorizationBlob * blob = (AuthorizationBlob*)calloc(1u, sizeof(AuthorizationBlob));
        require_action(blob != NULL, done, status = errAuthorizationInternal);
        *blob = *(AuthorizationBlob*)data;
        
        *authorization = (AuthorizationRef)blob;
    }

done:
    xpc_release_safe(message);
    xpc_release_safe(reply);
    return status;
}

OSStatus AuthorizationCreateWithAuditToken(audit_token_t token,
                                 const AuthorizationEnvironment *environment,
                                 AuthorizationFlags flags,
                                 AuthorizationRef *authorization)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t message = NULL;
    xpc_object_t reply = NULL;
    
    require_action(authorization != NULL, done, status = errAuthorizationInvalidPointer);
    
    // Send
    message = xpc_dictionary_create(NULL, NULL, 0);
    require_action(message != NULL, done, status = errAuthorizationInternal);
    
    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_CREATE_WITH_AUDIT_TOKEN);
    xpc_dictionary_set_data(message, AUTH_XPC_DATA, &token, sizeof(token));
    setItemSet(message, AUTH_XPC_ENVIROMENT, environment);
    xpc_dictionary_set_uint64(message, AUTH_XPC_FLAGS, flags);
    
    // Reply
    reply = xpc_connection_send_message_with_reply_sync(get_authorization_connection(), message);
    require_action(reply != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal);
    
    // Status
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);
    
    // Out
    if (status == errAuthorizationSuccess) {
        size_t len;
        const void * data = xpc_dictionary_get_data(reply, AUTH_XPC_BLOB, &len);
        require_action(data != NULL, done, status = errAuthorizationInternal);
        assert(len == sizeof(AuthorizationBlob));
        
        AuthorizationBlob * blob = (AuthorizationBlob*)calloc(1u, sizeof(AuthorizationBlob));
        require_action(blob != NULL, done, status = errAuthorizationInternal);
        *blob = *(AuthorizationBlob*)data;
        
        *authorization = (AuthorizationRef)blob;
    }
    
done:
    xpc_release_safe(message);
    xpc_release_safe(reply);
    return status;
}

OSStatus AuthorizationFree(AuthorizationRef authorization, AuthorizationFlags flags)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t message = NULL;
    xpc_object_t reply = NULL;
    AuthorizationBlob *blob = NULL;

    require_action(authorization != NULL, done, status = errAuthorizationInvalidRef);
    blob = (AuthorizationBlob *)authorization;
    
    // Send
    message = xpc_dictionary_create(NULL, NULL, 0);
    require_action(message != NULL, done, status = errAuthorizationInternal);
    
    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_FREE);
    xpc_dictionary_set_data(message, AUTH_XPC_BLOB, blob, sizeof(AuthorizationBlob));
    xpc_dictionary_set_uint64(message, AUTH_XPC_FLAGS, flags);

    // Reply
    reply = xpc_connection_send_message_with_reply_sync(get_authorization_connection(), message);
    require_action(reply != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal);
    
    // Status
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);
    
    // Free
    free(blob);
    
done:
    xpc_release_safe(message);
    xpc_release_safe(reply);
    return status;
}

static OSStatus
_AuthorizationCopyRights_send_message(xpc_object_t message, AuthorizationRights **authorizedRights)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t reply = NULL;

    // Send
    require_action(message != NULL, done, status = errAuthorizationInternal);

    // Reply
    reply = xpc_connection_send_message_with_reply_sync(get_authorization_connection(), message);
    require_action(reply != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal);

    // Status
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);

    // Out
    if (authorizedRights && status == errAuthorizationSuccess) {
        xpc_object_t tmpItems = xpc_dictionary_get_value(reply, AUTH_XPC_OUT_ITEMS);
        AuthorizationRights * grantedRights = DeserializeItemSet(tmpItems);
        require_action(grantedRights != NULL, done, status = errAuthorizationInternal);

        *authorizedRights = grantedRights;
    }

done:
    xpc_release_safe(reply);
    return status;
}

static OSStatus
_AuthorizationCopyRights_prepare_message(AuthorizationRef authorization, const AuthorizationRights *rights, const AuthorizationEnvironment *environment, AuthorizationFlags flags, xpc_object_t *message_out)
{
    OSStatus status = errAuthorizationInternal;
    AuthorizationBlob *blob = NULL;
    xpc_object_t message = xpc_dictionary_create(NULL, NULL, 0);
    require_action(message != NULL, done, status = errAuthorizationInternal);

    require_action(authorization != NULL, done, status = errAuthorizationInvalidRef);
    blob = (AuthorizationBlob *)authorization;

    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_COPY_RIGHTS);
    xpc_dictionary_set_data(message, AUTH_XPC_BLOB, blob, sizeof(AuthorizationBlob));
    setItemSet(message, AUTH_XPC_RIGHTS, rights);
    setItemSet(message, AUTH_XPC_ENVIROMENT, environment);
    xpc_dictionary_set_uint64(message, AUTH_XPC_FLAGS, flags);

    *message_out = message;
    message = NULL;
    status = errAuthorizationSuccess;

done:
    xpc_release_safe(message);
    return status;
}

OSStatus AuthorizationCopyRights(AuthorizationRef authorization,
                        const AuthorizationRights *rights,
                        const AuthorizationEnvironment *environment,
                        AuthorizationFlags flags,
                        AuthorizationRights **authorizedRights)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t message = NULL;

    require_noerr(status = _AuthorizationCopyRights_prepare_message(authorization, rights, environment, flags, &message), done);
    require_noerr(status = _AuthorizationCopyRights_send_message(message, authorizedRights), done);

done:
    xpc_release_safe(message);
    return status;
}


void AuthorizationCopyRightsAsync(AuthorizationRef authorization,
                         const AuthorizationRights *rights,
                         const AuthorizationEnvironment *environment,
                         AuthorizationFlags flags,
                         AuthorizationAsyncCallback callbackBlock)
{
    OSStatus prepare_status = errAuthorizationInternal;
    __block xpc_object_t message = NULL;

    prepare_status = _AuthorizationCopyRights_prepare_message(authorization, rights, environment, flags, &message);
    if (prepare_status != errAuthorizationSuccess) {
        callbackBlock(prepare_status, NULL);
    }

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
		AuthorizationRights *blockAuthorizedRights = NULL;
        OSStatus status = _AuthorizationCopyRights_send_message(message, &blockAuthorizedRights);
        callbackBlock(status, blockAuthorizedRights);
        xpc_release_safe(message);
	});
}

OSStatus AuthorizationDismiss()
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t message = NULL;
    xpc_object_t reply = NULL;

    // Send
    message = xpc_dictionary_create(NULL, NULL, 0);
    require(message != NULL, done);
    
    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_DISMISS);
    
    // Reply
    reply = xpc_connection_send_message_with_reply_sync(get_authorization_connection(), message);
    require_action(reply != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal);
    
    // Status
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);
    
done:
    xpc_release_safe(message);
    xpc_release_safe(reply);
    return status;
}

OSStatus AuthorizationCopyInfo(AuthorizationRef authorization,
                      AuthorizationString tag,
                      AuthorizationItemSet **info)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t message = NULL;
    xpc_object_t reply = NULL;
    AuthorizationBlob *blob = NULL;
    
    require_action(info != NULL, done, status = errAuthorizationInvalidSet);
    require_action(authorization != NULL, done, status = errAuthorizationInvalidRef);
    blob = (AuthorizationBlob *)authorization;
    
    // Send
    message = xpc_dictionary_create(NULL, NULL, 0);
    require_action(message != NULL, done, status = errAuthorizationInternal);
    
    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_COPY_INFO);
    xpc_dictionary_set_data(message, AUTH_XPC_BLOB, blob, sizeof(AuthorizationBlob));
    if (tag) {
        xpc_dictionary_set_string(message, AUTH_XPC_TAG, tag);
    }
    
    // Reply
    reply = xpc_connection_send_message_with_reply_sync(get_authorization_connection(), message);
    require_action(reply != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal);
    
    // Status
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);

    // Out
    if (info && status == errAuthorizationSuccess) {
        xpc_object_t tmpItems = xpc_dictionary_get_value(reply, AUTH_XPC_OUT_ITEMS);
        AuthorizationRights * outInfo = DeserializeItemSet(tmpItems);
        require_action(outInfo != NULL, done, status = errAuthorizationInternal);
        
        *info = outInfo;
    }
    
done:
    xpc_release_safe(message);
    xpc_release_safe(reply);
    return status;
}

OSStatus AuthorizationMakeExternalForm(AuthorizationRef authorization,
                              AuthorizationExternalForm *extForm)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t message = NULL;
    xpc_object_t reply = NULL;
    AuthorizationBlob *blob = NULL;
    
    require_action(extForm != NULL, done, status = errAuthorizationInvalidPointer);
    require_action(authorization != NULL, done, status = errAuthorizationInvalidRef);
    blob = (AuthorizationBlob *)authorization;
    
    // Send
    message = xpc_dictionary_create(NULL, NULL, 0);
    require_action(message != NULL, done, status = errAuthorizationInternal);
    
    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_MAKE_EXTERNAL_FORM);
    xpc_dictionary_set_data(message, AUTH_XPC_BLOB, blob, sizeof(AuthorizationBlob));
    
    // Reply
    reply = xpc_connection_send_message_with_reply_sync(get_authorization_connection(), message);
    require_action(reply != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal);
    
    // Status
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);
    
    // out
    if (status == errAuthorizationSuccess) {
        size_t len;
        const void * data = xpc_dictionary_get_data(reply, AUTH_XPC_EXTERNAL, &len);
        require_action(data != NULL, done, status = errAuthorizationInternal);
        assert(len == sizeof(AuthorizationExternalForm));
        
        *extForm = *(AuthorizationExternalForm*)data;
    }
    
done:
    xpc_release_safe(message);
    xpc_release_safe(reply);
    return status;
}

OSStatus AuthorizationCreateFromExternalForm(const AuthorizationExternalForm *extForm,
                                    AuthorizationRef *authorization)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t message = NULL;
    xpc_object_t reply = NULL;
    
    require_action(extForm != NULL, done, status = errAuthorizationInvalidPointer);
    require_action(authorization != NULL, done, status = errAuthorizationInvalidRef);
    
    // Send
    message = xpc_dictionary_create(NULL, NULL, 0);
    require_action(message != NULL, done, status = errAuthorizationInternal);
    
    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_CREATE_FROM_EXTERNAL_FORM);
    xpc_dictionary_set_data(message, AUTH_XPC_EXTERNAL, extForm, sizeof(AuthorizationExternalForm));
    
    // Reply
    reply = xpc_connection_send_message_with_reply_sync(get_authorization_connection(), message);
    require_action(reply != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal);
    
    // Status
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);
    
    // Out
    if (authorization && status == errAuthorizationSuccess) {
        size_t len;
        const void * data = xpc_dictionary_get_data(reply, AUTH_XPC_BLOB, &len);
        require_action(data != NULL, done, status = errAuthorizationInternal);
        assert(len == sizeof(AuthorizationBlob));
        
        AuthorizationBlob * blob = (AuthorizationBlob*)calloc(1u, sizeof(AuthorizationBlob));
        require_action(blob != NULL, done, status = errAuthorizationInternal);
        *blob = *(AuthorizationBlob*)data;
        
        *authorization = (AuthorizationRef)blob;
    }
    
done:
    xpc_release_safe(message);
    xpc_release_safe(reply);
    return status;
}

OSStatus AuthorizationFreeItemSet(AuthorizationItemSet *set)
{
    FreeItemSet(set);
    return errAuthorizationSuccess;
}

OSStatus AuthorizationEnableSmartCard(AuthorizationRef authRef, Boolean enable)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t message = NULL;
    xpc_object_t reply = NULL;
    AuthorizationBlob *blob = NULL;

    // Send
    message = xpc_dictionary_create(NULL, NULL, 0);
    require_action(message != NULL, done, status = errAuthorizationInternal);
    require_action(authRef != NULL, done, status = errAuthorizationInvalidRef);
    blob = (AuthorizationBlob *)authRef;
    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_ENABLE_SMARTCARD);
    xpc_dictionary_set_data(message, AUTH_XPC_BLOB, blob, sizeof(AuthorizationBlob));
    xpc_dictionary_set_bool(message, AUTH_XPC_DATA, enable);
    
    // Reply
    reply = xpc_connection_send_message_with_reply_sync(get_authorization_connection(), message);
    require_action(reply != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal);
    
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);

done:
    xpc_release_safe(message);
    xpc_release_safe(reply);
    return status;
}


OSStatus AuthorizationRightGet(const char *rightName,
                      CFDictionaryRef *rightDefinition)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t message = NULL;
    xpc_object_t reply = NULL;
    
    require_action(rightName != NULL, done, status = errAuthorizationInvalidPointer);
    
    // Send
    message = xpc_dictionary_create(NULL, NULL, 0);
    require_action(message != NULL, done, status = errAuthorizationInternal);
    
    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_RIGHT_GET);
    xpc_dictionary_set_string(message, AUTH_XPC_RIGHT_NAME, rightName);
    
    // Reply
    reply = xpc_connection_send_message_with_reply_sync(get_authorization_connection(), message);
    require_action(reply != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal);
    
    // Status
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);
    
    // Out
    if (rightDefinition && status == errAuthorizationSuccess) {
        xpc_object_t value = xpc_dictionary_get_value(reply, AUTH_XPC_DATA);
        require_action(value != NULL, done, status = errAuthorizationInternal);
        require_action(xpc_get_type(value) == XPC_TYPE_DICTIONARY, done, status = errAuthorizationInternal);
        
        CFTypeRef cfdict = _CFXPCCreateCFObjectFromXPCObject(value);
        require_action(cfdict != NULL, done, status = errAuthorizationInternal);
        
        *rightDefinition = cfdict;
    }
    
done:
    xpc_release_safe(message);
    xpc_release_safe(reply);
    return status;
}

OSStatus AuthorizationRightSet(AuthorizationRef authRef,
                      const char *rightName,
                      CFTypeRef rightDefinition,
                      CFStringRef descriptionKey,
                      CFBundleRef bundle,
                      CFStringRef tableName)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t message = NULL;
    xpc_object_t reply = NULL;
    AuthorizationBlob *blob = NULL;
    CFMutableDictionaryRef rightDict = NULL;
    CFBundleRef clientBundle = bundle;
    
    if (bundle) {
        CFRetain(bundle);
    }
    
    require_action(rightDefinition != NULL, done, status = errAuthorizationInvalidPointer);
    require_action(rightName != NULL, done, status = errAuthorizationInvalidPointer);
    require_action(authRef != NULL, done, status = errAuthorizationInvalidRef);
    blob = (AuthorizationBlob *)authRef;
    
    // Send
    message = xpc_dictionary_create(NULL, NULL, 0);
    require_action(message != NULL, done, status = errAuthorizationInternal);
    
    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_RIGHT_SET);
    xpc_dictionary_set_data(message, AUTH_XPC_BLOB, blob, sizeof(AuthorizationBlob));
    xpc_dictionary_set_string(message, AUTH_XPC_RIGHT_NAME, rightName);
    
    // Create rightDict
    if (CFGetTypeID(rightDefinition) == CFStringGetTypeID()) {
        rightDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        require_action(rightDict != NULL, done, status = errAuthorizationInternal);
        
        CFDictionarySetValue(rightDict, CFSTR(kAuthorizationRightRule), rightDefinition);
    
    } else if (CFGetTypeID(rightDefinition) == CFDictionaryGetTypeID()) {
        rightDict = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, rightDefinition);
        require_action(rightDict != NULL, done, status = errAuthorizationInternal);
    
    } else {
        status = errAuthorizationInvalidPointer;
        goto done;
    }
    
    // Create locDict
    if (descriptionKey) {
        CFMutableDictionaryRef locDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        require_action(locDict != NULL, done, status = errAuthorizationInternal);
        
        if (clientBundle == NULL) {
            clientBundle = CFBundleGetMainBundle();
            CFRetain(clientBundle);
        }
        
        if (clientBundle) {
            CFArrayRef bundleLocalizations = CFBundleCopyBundleLocalizations(clientBundle);
            if (bundleLocalizations) {
                // for every CFString in localizations do
                CFIndex locIndex, allLocs = CFArrayGetCount(bundleLocalizations);
                for (locIndex = 0; locIndex < allLocs; locIndex++)
                {
                    CFStringRef oneLocalization = (CFStringRef)CFArrayGetValueAtIndex(bundleLocalizations, locIndex);
                    
                    if (!oneLocalization)
                        continue;
                    
                    // @@@ no way to get "Localized" and "strings" as constants?
                    CFURLRef locURL = CFBundleCopyResourceURLForLocalization(clientBundle, tableName ? tableName :  CFSTR("Localizable"), CFSTR("strings"), NULL /*subDirName*/, oneLocalization);
                    
                    if (!locURL)
                        continue;
                    
                    CFDataRef tableData = NULL;
                    SInt32 errCode;
                    CFStringRef errStr = NULL;
                    CFPropertyListRef stringTable = NULL;
                    
                    CFURLCreateDataAndPropertiesFromResource(CFGetAllocator(clientBundle), locURL, &tableData, NULL, NULL, &errCode);
                    CFReleaseSafe(locURL);
                    if (errCode)
                    {
                        CFReleaseSafe(tableData);
                        continue;
                    }
                    
                    stringTable = CFPropertyListCreateFromXMLData(CFGetAllocator(clientBundle), tableData, kCFPropertyListImmutable, &errStr);
                    CFReleaseSafe(errStr);
                    CFReleaseSafe(tableData);
                    
                    CFStringRef value = (CFStringRef)CFDictionaryGetValue(stringTable, descriptionKey);
                    if (value == NULL || CFEqual(value, CFSTR(""))) {
                        CFReleaseSafe(stringTable);
                        continue;
                    } else {
                        // oneLocalization/value into our dictionary 
                        CFDictionarySetValue(locDict, oneLocalization, value);
                        CFReleaseSafe(stringTable);
                    }
                }
                CFReleaseSafe(bundleLocalizations);
            }
        }
        
        // add the description as the default localization into the dictionary
		CFDictionarySetValue(locDict, CFSTR(""), descriptionKey);
		
		// stuff localization table into right definition
		CFDictionarySetValue(rightDict, CFSTR(kAuthorizationRuleParameterDefaultPrompt), locDict);
        CFReleaseSafe(locDict);
    }
    
    xpc_object_t value = _CFXPCCreateXPCObjectFromCFObject(rightDict);
    xpc_dictionary_set_value(message, AUTH_XPC_DATA, value);
    xpc_release_safe(value);
    
    // Reply
    reply = xpc_connection_send_message_with_reply_sync(get_authorization_connection(), message);
    require_action(reply != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal);
    
    // Status
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);
    
done:
    CFReleaseSafe(clientBundle);
    CFReleaseSafe(rightDict);
    xpc_release_safe(message);
    xpc_release_safe(reply);
    return status;
}

OSStatus AuthorizationRightRemove(AuthorizationRef authorization,
                         const char *rightName)
{
    OSStatus status = errAuthorizationInternal;
    xpc_object_t message = NULL;
    xpc_object_t reply = NULL;
    AuthorizationBlob *blob = NULL;
    
    require_action(rightName != NULL, done, status = errAuthorizationInvalidPointer);
    require_action(authorization != NULL, done, status = errAuthorizationInvalidRef);
    blob = (AuthorizationBlob *)authorization;
    
    // Send
    message = xpc_dictionary_create(NULL, NULL, 0);
    require_action(message != NULL, done, status = errAuthorizationInternal);
    
    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_RIGHT_REMOVE);
    xpc_dictionary_set_data(message, AUTH_XPC_BLOB, blob, sizeof(AuthorizationBlob));
    xpc_dictionary_set_string(message, AUTH_XPC_RIGHT_NAME, rightName);
    
    // Reply
    reply = xpc_connection_send_message_with_reply_sync(get_authorization_connection(), message);
    require_action(reply != NULL, done, status = errAuthorizationInternal);
    require_action(xpc_get_type(reply) != XPC_TYPE_ERROR, done, status = errAuthorizationInternal);
    
    // Status
    status = (OSStatus)xpc_dictionary_get_int64(reply, AUTH_XPC_STATUS);
    
done:
    xpc_release_safe(message);
    xpc_release_safe(reply);
    return status;
}
