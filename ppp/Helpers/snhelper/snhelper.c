/*
 * Copyright (c) 2013 Apple Inc.
 * All rights reserved.
 */

#include <errno.h>
#include <syslog.h>

#include <xpc/xpc.h>
#include <xpc/private.h>
#include <sandbox.h>

#include <SystemConfiguration/SNHelperPrivate.h>

#include "snhelper.h"
#include "flow_divert.h"
#include "launch_services.h"

static bool
has_entitlement(xpc_connection_t connection)
{
	bool result;
	xpc_object_t entitlement = xpc_connection_copy_entitlement_value(connection, "com.apple.private.snhelper");
	result = (isa_xpc_bool(entitlement) && xpc_bool_get_value(entitlement));
	if (entitlement != NULL) {
		xpc_release(entitlement);
	}
	return result;
}

static void
handle_message(xpc_connection_t connection, xpc_object_t message)
{
	static dispatch_once_t init;
	static dispatch_queue_t handler_queue = NULL;

	dispatch_once(&init,
		^{
			handler_queue = dispatch_queue_create("snhelper message handler queue", NULL);
		});

	xpc_retain(connection);
	xpc_retain(message);

	dispatch_async(handler_queue,
		^{
			switch (xpc_dictionary_get_uint64(message, kSNHelperMessageType)) {
				case kSNHelperMessageTypeFlowDivertUUIDAdd:
					handle_flow_divert_uuid_add(connection, message);
					break;
				case kSNHelperMessageTypeFlowDivertUUIDRemove:
					handle_flow_divert_uuid_remove(connection, message);
					break;
				case kSNHelperMessageTypeFlowDivertUUIDClear:
					handle_flow_divert_uuid_clear(connection, message);
					break;
#if TARGET_OS_EMBEDDED
				case kSNHelperMessageTypeGetUUIDForApp:
					handle_get_uuid_for_app(connection, message);
					break;
#endif
				default:
					send_reply(connection, message, EINVAL, NULL);
					break;
			}

			xpc_release(connection);
			xpc_release(message);
		});
}

static bool
init_xpc_service(void)
{
	xpc_track_activity();

	xpc_connection_t listener = xpc_connection_create_mach_service(kSNHelperService, NULL, XPC_CONNECTION_MACH_SERVICE_LISTENER);
	if (!isa_xpc_connection(listener)) {
		return false;
	}

	xpc_connection_set_event_handler(listener,
		^(xpc_object_t message) {
			if (isa_xpc_connection(message)) {
				__block xpc_connection_t connection = message;
				if (!has_entitlement(connection)) {
					xpc_connection_cancel(connection);
					syslog(LOG_ERR, "Caller is missing the snhelper entitlement");
					return;
				}

				xpc_retain(connection);

				xpc_connection_set_event_handler(connection,
					^(xpc_object_t message) {
						if (isa_xpc_dictionary(message)) {
							handle_message(connection, message);
						} else if (isa_xpc_error(message)) {
							if (connection != NULL) {
								xpc_release(connection);
								connection = NULL;
								xpc_transaction_end();
							}
						}
					});
				xpc_connection_resume(connection);

				xpc_transaction_begin();
			} else if (isa_xpc_error(message)) {
				exit(EXIT_SUCCESS);
			}
		});
	xpc_connection_resume(listener);

	return true;
}

#ifndef kSNHelperMessageResultData
#define kSNHelperMessageResultData "result-data"
#endif
void
send_reply(xpc_connection_t connection, xpc_object_t request, int64_t result, xpc_object_t result_data)
{
	xpc_object_t reply = xpc_dictionary_create_reply(request);
	if (reply != NULL) {
		xpc_dictionary_set_int64(reply, kSNHelperMessageResult, result);
		if (result_data) {
			xpc_dictionary_set_value(reply, kSNHelperMessageResultData, result_data);
		}
		xpc_connection_send_message(connection, reply);
		xpc_release(reply);
	}
}

int
main(int argc, char *argv[])
{
	char *sberr = NULL;

	if (sandbox_init("com.apple.snhelper", SANDBOX_NAMED, &sberr) < 0) {
		syslog(LOG_ERR, "sandbox_init failed: %s", sberr);
		sandbox_free_error(sberr);
		return EXIT_FAILURE;
	}

	if (!init_xpc_service()) {
		return EXIT_FAILURE;
	}

	dispatch_main();
	return EXIT_SUCCESS;
}
