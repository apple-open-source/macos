//
//  seccode.c
//  secseccodeapitest
//
#include "authd_private.h"

#include <stdio.h>
#include <xpc/xpc.h>
#include <Security/SecCode.h>
#include <libkern/OSAtomic.h>
#include <AssertMacros.h>

#define BEGIN()                                             \
({                                                          \
    fprintf(stdout, "[BEGIN] %s\n", __FUNCTION__);          \
})

#define INFO(fmt, ...)                                      \
({                                                          \
    fprintf(stdout, fmt "\n", ##__VA_ARGS__);               \
})

#define PASS(fmt, ...)                                                      \
({                                                                          \
    fprintf(stdout, "[PASS] %s " fmt "\n", __FUNCTION__, ##__VA_ARGS__);    \
})

#define FAIL(fmt, ...)                                                      \
({                                                                          \
    fprintf(stdout, "[FAIL] %s " fmt "\n", __FUNCTION__, ##__VA_ARGS__);    \
})

#define SAFE_RELEASE(x)                                     \
({                                                          \
    if (x) {                                                \
        CFRelease(x);                                       \
        x = NULL;                                           \
    }                                                       \
})

enum xpcConnectionStates {
    kXPCConnectionStateNotCancelled = 0,
    kXPCConnectionStateCancelled,
    kXPCConnectionStateOkayToExit,
    kXPCConnectionStateServerNotAvailable,
};

static int
_validatePathFromSecCode(SecCodeRef processRef, const char *path)
{
    int ret = -1;
    OSStatus status;
    SecStaticCodeRef staticProcessRef = NULL;
    CFURLRef pathURL = NULL;
    CFStringRef pathString = NULL;

    /* Get the StaticCodeRef for this SecCodeRef */
    status = SecCodeCopyStaticCode(processRef, kSecCSDefaultFlags, &staticProcessRef);
    require_noerr_action(status, exit, ret = -1);

    INFO("Successfully created a SecStaticCodeRef");

    /* Copy the path of requested service */
    status = SecCodeCopyPath(staticProcessRef, kSecCSDefaultFlags, &pathURL);
    require_noerr_action(status, exit, ret = -1);

    INFO("Successfully created a CFURLRef");

    /* Get the CFStringRef from the CFURLRef */
    pathString = CFURLGetString(pathURL);
    require_action(pathString, exit, ret = -1);

    INFO("Successfully created a CFStingRef");

    if (!strncmp(path, CFStringGetCStringPtr(pathString, kCFStringEncodingUTF8), strlen(path))) {
        INFO("Successfully confirmed the location of requested service");
        ret = 0;
    } else {
        INFO("Location of service incorrect: %s", CFStringGetCStringPtr(pathString, kCFStringEncodingUTF8));
        ret = -1;
    }

exit:
    SAFE_RELEASE(pathURL);
    SAFE_RELEASE(staticProcessRef);

    return ret;
}

static int
CheckCreateWithXPCMessage(void)
{
    BEGIN();

    int ret;
    OSStatus status;
    xpc_connection_t connection = NULL;
    xpc_object_t message = NULL, reply = NULL;
    SecCodeRef processRef = NULL;
    volatile static int xpcState = kXPCConnectionStateNotCancelled;

    connection = xpc_connection_create(SECURITY_AUTH_NAME, NULL);
    if (NULL == connection) {
        FAIL("Unable to create an XPC connection with %s", SECURITY_AUTH_NAME);
        return -1;
    }

    INFO("XPC Connection with %s created", SECURITY_AUTH_NAME);

    xpc_connection_set_event_handler(connection, ^(xpc_object_t event) {
        if (xpc_get_type(event) == XPC_TYPE_ERROR && event == XPC_ERROR_CONNECTION_INVALID) {
            if (OSAtomicCompareAndSwapInt(kXPCConnectionStateCancelled, kXPCConnectionStateOkayToExit, &xpcState)) {
                INFO("XPC Connection Cancelled");
            } else {
                xpcState = kXPCConnectionStateServerNotAvailable;
                FAIL("Authorization server not available");
            }
        }
    });

    xpc_connection_resume(connection);

    INFO("XPC Connection resumed");

    /* Create an empty dictionary */
    message = xpc_dictionary_create(NULL, NULL, 0);

    /*
     * Set _type to something invalid. This is done because authd will simply
     * return an "invalid type" for this case, which means no state changes in
     * the authd daemon.
     */
    xpc_dictionary_set_uint64(message, AUTH_XPC_TYPE, AUTHORIZATION_COPY_RIGHT_PROPERTIES+512);

    /* Send object and wait for response */
    reply = xpc_connection_send_message_with_reply_sync(connection, message);
    xpc_release(message);

    INFO("XPC Message received");

    /* Create a SecCode using the XPC Message */
    status = SecCodeCreateWithXPCMessage(reply, kSecCSDefaultFlags, &processRef);
    if (status) {
        FAIL("Unable to create a SecCodeRef from message reply [%d]", status);
        xpc_release(reply);
        return -1;
    }
    xpc_release(reply);

    INFO("Successfully created a SecCodeRef");

    const char *authdLocation = "file:///System/Library/Frameworks/Security.framework/Versions/A/XPCServices/authd.xpc/";
    if (_validatePathFromSecCode(processRef, authdLocation)) {
        FAIL("Unable to verify authd location");
        ret = -1;
    } else {
        PASS("authd location successfully verified");
        ret = 0;
    }

    SAFE_RELEASE(processRef);

    // Potential race condition in getting an actual XPC_TYPE_ERROR vs getting
    // a connection cancelled. We are okay with this since this is extremely unlikely...
    if (OSAtomicCompareAndSwapInt(kXPCConnectionStateNotCancelled, kXPCConnectionStateCancelled, &xpcState)) {
        xpc_connection_cancel(connection);
    }

    while (xpcState != kXPCConnectionStateOkayToExit) {
        if (xpcState == kXPCConnectionStateServerNotAvailable) {
            break;
        }
        usleep(1000 * 1);
    }

    return ret;
}

static int
CheckCreateWithXPCMessage_invalidXPCObject(void)
{
    BEGIN();

    OSStatus status;
    xpc_object_t invalidObject = NULL;
    SecCodeRef processRef = NULL;

    /* Create an NULL object */
    invalidObject = xpc_null_create();

    INFO("Created a NULL object");

    /* Try and acquire a SecCodeRef through the NULL object -- should fail with errSecCSInvalidObjectRef */
    status = SecCodeCreateWithXPCMessage(invalidObject, kSecCSDefaultFlags, &processRef);
    if (status != errSecCSInvalidObjectRef) {
        FAIL("Return code unexpected [%d]", status);
        return -1;
    }

    PASS("Got expected return code");
    return 0;
}

static int
CheckCreateWithXPCMessage_NULLConnectionInObject(void)
{
    BEGIN();

    OSStatus status;
    xpc_object_t emptyDictionary = NULL;
    SecCodeRef processRef = NULL;

    /* Create an empty dictionary object */
    emptyDictionary = xpc_dictionary_create_empty();

    INFO("Created an empty dictionary object");

    /* Try and acquire a SecCodeRef through the empty dictionary -- should fail with errSecCSInvalidObjectRef */
    status = SecCodeCreateWithXPCMessage(emptyDictionary, kSecCSDefaultFlags, &processRef);
    if (status != errSecCSInvalidObjectRef) {
        FAIL("Return code unexpected [%d]", status);
        return -1;
    }

    PASS("Got expected return code");
    return 0;
}

static int
CheckValidateWithNoNetwork(void)
{
    BEGIN();

    OSStatus status;
    SecCodeRef processRef = NULL;

    status = SecCodeCopySelf(kSecCSDefaultFlags, &processRef);
    if (status) {
        FAIL("Return code unexpected [%d]", status);
        return -1;
    }

    status = SecCodeCheckValidity(processRef, kSecCSNoNetworkAccess, NULL);
    if (status == errSecCSInvalidFlags) {
        FAIL("SecCodeCheckValidity did not accept kSecCSNoNetworkAccess");
        return -1;
    } else if (status) {
        FAIL("Return code unexpected [%d]", status);
        return -1;
    }

    PASS("Flag was accepted for validation call.");
    return 0;
}

int main(void)
{
    fprintf(stdout, "[TEST] secseccodeapitest\n");

    int i;
    int (*testList[])(void) = {
        CheckCreateWithXPCMessage,
        CheckCreateWithXPCMessage_invalidXPCObject,
        CheckCreateWithXPCMessage_NULLConnectionInObject,
        CheckValidateWithNoNetwork,
    };
    const int numberOfTests = sizeof(testList) / sizeof(*testList);
    int testResults[numberOfTests] = {0};

    for (i = 0; i < numberOfTests; i++) {
        testResults[i] = testList[i]();
    }

    fprintf(stdout, "[SUMMARY]\n");
    for (i = 0; i < numberOfTests; i++) {
        fprintf(stdout, "%d. %s\n", i+1, testResults[i] == 0 ? "Passed" : "Failed");
    }

    return 0;
}
