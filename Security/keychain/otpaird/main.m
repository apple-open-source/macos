#import <TargetConditionals.h>
#import <Foundation/Foundation.h>
#import <Security/SecXPCHelper.h>
#import <xpc/xpc.h>
#if TARGET_OS_WATCH
#import <NanoRegistry/NanoRegistry.h>
#import <xpc/private.h>
#endif /* TARGET_OS_WATCH */

#import "OTPairingService.h"
#import "OTPairingConstants.h"

#if TARGET_OS_WATCH
static void create_xpc_listener(xpc_handler_t handler);
static bool should_retry(NSError *error);
static void pairing_retry_register(bool checkin);
static void pairing_retry_unregister(void);
#endif /* TARGET_OS_WATCH */

int
main()
{
    static OTPairingService *service;

    @autoreleasepool {
        service = [OTPairingService sharedService];
    }

#if TARGET_OS_WATCH
    /* Check in; handle a possibly-pending retry. */
    pairing_retry_register(true);

    create_xpc_listener(^(xpc_object_t message) {
        xpc_object_t reply;

        reply = xpc_dictionary_create_reply(message);

        /* Received an explicit pairing request; remove retry activity if one exists. */
        pairing_retry_unregister();

        [service initiatePairingWithCompletion:^(bool success, NSError *error) {
            xpc_connection_t connection;

            if (success) {
                os_log(OS_LOG_DEFAULT, "xpc-initiated pairing succeeded");
            } else {
                os_log(OS_LOG_DEFAULT, "xpc-initiated pairing failed: %@", error);
            }

            xpc_dictionary_set_bool(reply, OTPairingXPCKeySuccess, success);
            if (error) {
                NSData *errdata = [SecXPCHelper encodedDataFromError:error];
                xpc_dictionary_set_data(reply, OTPairingXPCKeyError, errdata.bytes, errdata.length);
            }
            connection = xpc_dictionary_get_remote_connection(reply);
            xpc_connection_send_message(connection, reply);

            // Retry on failure - unless we were already in
            if (!success && should_retry(error)) {
                pairing_retry_register(false);
            }
        }];
    });
#endif /* TARGET_OS_WATCH */

    dispatch_main();
}

#if TARGET_OS_WATCH
static void
create_xpc_listener(xpc_handler_t handler)
{
    static xpc_connection_t listener;

    listener = xpc_connection_create_mach_service(OTPairingMachServiceName, NULL, XPC_CONNECTION_MACH_SERVICE_LISTENER);
    xpc_connection_set_event_handler(listener, ^(xpc_object_t peer) {
        if (xpc_get_type(peer) != XPC_TYPE_CONNECTION) {
            return;
        }

        // TODO: entitlement check

        xpc_connection_set_event_handler(peer, ^(xpc_object_t message) {
            if (xpc_get_type(message) != XPC_TYPE_DICTIONARY) {
                return;
            }

            char *desc = xpc_copy_description(message);
            os_log(OS_LOG_DEFAULT, "received xpc message: %s", desc);
            free(desc);

            @autoreleasepool {
                handler(message);
            }
        });
        xpc_connection_activate(peer);
    });
    xpc_connection_activate(listener);
}

static bool
should_retry(NSError *error)
{
    bool retry;

    if ([error.domain isEqualToString:OTPairingErrorDomain]) {
        switch (error.code) {
        case OTPairingErrorTypeAlreadyIn:
        case OTPairingErrorTypeBusy:
            retry = false;
            break;
        default:
            retry = true;
            break;
        }
    } else {
        retry = true;
    }

    return retry;
}

static void
pairing_retry_register(bool checkin)
{
    xpc_object_t criteria;

    if (checkin) {
        criteria = XPC_ACTIVITY_CHECK_IN;
    } else {
        os_log(OS_LOG_DEFAULT, "scheduling pairing retry");

        pairing_retry_unregister();

        criteria = xpc_dictionary_create(NULL, NULL, 0);
        xpc_dictionary_set_string(criteria, XPC_ACTIVITY_PRIORITY, XPC_ACTIVITY_PRIORITY_MAINTENANCE);
        xpc_dictionary_set_int64(criteria, XPC_ACTIVITY_INTERVAL, OTPairingXPCActivityInterval);
        xpc_dictionary_set_bool(criteria, XPC_ACTIVITY_REPEATING, true);
        xpc_dictionary_set_bool(criteria, XPC_ACTIVITY_ALLOW_BATTERY, true);
        xpc_dictionary_set_bool(criteria, XPC_ACTIVITY_REQUIRES_CLASS_A, true);
        xpc_dictionary_set_bool(criteria, XPC_ACTIVITY_COMMUNICATES_WITH_PAIRED_DEVICE, true);
    }

    xpc_activity_register(OTPairingXPCActivityIdentifier, criteria, ^(xpc_activity_t activity) {
        xpc_activity_state_t state = xpc_activity_get_state(activity);
        if (state == XPC_ACTIVITY_STATE_RUN) {
            os_log(OS_LOG_DEFAULT, "triggered pairing attempt via XPC Activity");
            OTPairingService *service = [OTPairingService sharedService];
            [service initiatePairingWithCompletion:^(bool success, NSError *error) {
                if (success) {
                    os_log(OS_LOG_DEFAULT, "Pairing retry succeeded");
                    pairing_retry_unregister();
                } else {
                    os_log(OS_LOG_DEFAULT, "Pairing retry failed: %@", error);
                    // Activity repeats...
                }
            }];
        }
    });
}

static void
pairing_retry_unregister(void)
{
    xpc_activity_unregister(OTPairingXPCActivityIdentifier);
}
#endif /* TARGET_OS_WATCH */
