#include "ipc_utils.h"
#include <darwintest.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysctl.h>

/*
 * Implementation of common IPC utilities for XNU tests
 *
 * Note: This implementation is designed specifically for test environments
 * and uses darwintest T_QUIET macros for internal validations.
 */

/*
 * Port Management Functions
 */

mach_port_t
ipc_create_receive_port(void)
{
	return ipc_create_receive_port_with_options(0);
}

mach_port_t
ipc_create_receive_port_with_options(uint32_t mpo_flags)
{
	mach_port_options_t opts = {
		.flags = mpo_flags,
	};
	mach_port_name_t port;
	kern_return_t kr;

	kr = mach_port_construct(mach_task_self(), &opts, 0, &port);
	T_ASSERT_MACH_SUCCESS(kr, "ipc_create_receive_port_with_options");

	return port;
}

void
ipc_deallocate_port(mach_port_t port)
{
	if (port != MACH_PORT_NULL) {
		kern_return_t kr = mach_port_deallocate(mach_task_self(), port);
		T_ASSERT_MACH_SUCCESS(kr, "ipc_deallocate_port");
	}
}

kern_return_t
ipc_insert_send_right(mach_port_t receive_port)
{
	kern_return_t kr = mach_port_insert_right(mach_task_self(), receive_port,
	    receive_port, MACH_MSG_TYPE_MAKE_SEND);
	T_ASSERT_MACH_SUCCESS(kr, "ipc_insert_send_right");
	return kr;
}

/*
 * Single Port Messaging Functions
 */

kern_return_t
ipc_send_port(mach_port_t destination, mach_port_t port, mach_msg_type_name_t disposition)
{
	ipc_single_port_msg_t msg = {
		.header = {
			.msgh_remote_port = destination,
			.msgh_local_port = MACH_PORT_NULL,
			.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0) | MACH_MSGH_BITS_COMPLEX,
			.msgh_size = sizeof(ipc_single_port_msg_t),
			.msgh_id = 0x1001  /* Single port message ID */
		},
		.body = {
			.msgh_descriptor_count = 1
		},
		.port = {
			.name = port,
			.disposition = disposition,
			.type = MACH_MSG_PORT_DESCRIPTOR
		}
	};

	return ipc_send_message(&msg.header);
}

kern_return_t
ipc_receive_port(mach_port_t destination, mach_port_t *port)
{
	ipc_receive_msg_t msg;
	kern_return_t kr;

	kr = ipc_receive_message(destination, &msg, sizeof(ipc_receive_msg_t));
	if (kr == KERN_SUCCESS) {
		if (msg.header.msgh_id == 0x1001 && msg.body.msgh_descriptor_count == 1) {
			*port = msg.data.single.port.name;
		} else {
			kr = KERN_INVALID_ARGUMENT;
		}
	}

	return kr;
}


/*
 * Port Array Messaging Functions
 */

kern_return_t
ipc_send_port_array(mach_port_t destination,
    mach_port_t *ports, mach_msg_type_number_t count,
    mach_msg_type_name_t disposition)
{
	ipc_port_array_msg_t msg = {
		.header = {
			.msgh_remote_port = destination,
			.msgh_local_port = MACH_PORT_NULL,
			.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0) | MACH_MSGH_BITS_COMPLEX,
			.msgh_size = sizeof(ipc_port_array_msg_t),
			.msgh_id = 0x1004  /* Port array message ID */
		},
		.body = {
			.msgh_descriptor_count = 1
		},
		.ports_descriptor = {
			.address = (void*)ports,
			.count = count,
			.deallocate = FALSE,
			.disposition = disposition,
			.type = MACH_MSG_OOL_PORTS_DESCRIPTOR
		}
	};

	return ipc_send_message(&msg.header);
}

kern_return_t
ipc_receive_port_array(mach_port_t destination,
    mach_port_t **ports, mach_msg_type_number_t *count)
{
	ipc_receive_msg_t msg;
	kern_return_t kr;

	kr = ipc_receive_message(destination, &msg, sizeof(ipc_receive_msg_t));
	if (kr == KERN_SUCCESS) {
		if (msg.header.msgh_id == 0x1004 && msg.body.msgh_descriptor_count == 1) {
			*ports = (mach_port_t*)msg.data.array.ports_descriptor.address;
			*count = msg.data.array.ports_descriptor.count;
		} else {
			kr = KERN_INVALID_ARGUMENT;
		}
	}

	return kr;
}

/*
 * Generic Messaging Functions
 */

kern_return_t
ipc_send_message(mach_msg_header_t *msg)
{
	kern_return_t kr = mach_msg(msg, MACH_SEND_MSG, msg->msgh_size, 0,
	    MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	return kr;
}

kern_return_t
ipc_receive_message(mach_port_t destination, ipc_receive_msg_t *msg, mach_msg_size_t max_size)
{
	kern_return_t kr = mach_msg(&msg->header, MACH_RCV_MSG, 0, max_size,
	    destination, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	return kr;
}


/*
 * Security and Code Signing Utilities
 */

bool
ipc_hardening_disabled(void)
{
#if TARGET_OS_OSX || TARGET_OS_BRIDGE
	/*
	 * CS_CONFIG_GET_OUT_OF_MY_WAY (enabled via AMFI boot-args)
	 * disables IPC security features. Unfortunately,
	 * BATS runs with this boot-arg enabled very frequently.
	 */
	code_signing_config_t cur_cs_config = 0;
	size_t cs_config_size = sizeof(cur_cs_config);
	int result = sysctlbyname("security.codesigning.config", &cur_cs_config,
	    &cs_config_size, NULL, 0);
	if (result != 0) {
		T_QUIET; T_LOG("ipc_hardening_disabled: failed to get codesigning config, assuming not disabled");
		return false;
	}
	return (cur_cs_config & CS_CONFIG_GET_OUT_OF_MY_WAY) != 0;
#else /* TARGET_OS_OSX || TARGET_OS_BRIDGE */
	/* mach hardening is only disabled by boot-args on macOS */
	return false;
#endif
}
