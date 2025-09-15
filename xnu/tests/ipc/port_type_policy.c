/*
 * Copyright (c) 2025 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#include <darwintest.h>
#include <mach/mach.h>
#include <stdlib.h>
#include <unistd.h>
#include <mach/mk_timer.h>
#include <sys/sysctl.h>
#include <sys/code_signing.h>

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.ipc"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("IPC"),
	T_META_TIMEOUT(10),
	T_META_IGNORECRASHES(".*port_type_policy.*"),
	T_META_RUN_CONCURRENTLY(TRUE));

/* in xpc/launch_private.h */
#define XPC_DOMAIN_SYSTEM 1

#define countof(arr) (sizeof(arr) / sizeof((arr)[0]))


static void
expect_sigkill(
	void (^fn)(void),
	const char *format_description, ...)
{
	char description[0x100];

	va_list args;
	va_start(args, format_description);
	vsnprintf(description, sizeof(description), format_description, args);
	va_end(args);

	pid_t pid = fork();
	T_QUIET; T_ASSERT_POSIX_SUCCESS(pid, "fork");

	if (pid == 0) {
		fn();
		T_ASSERT_FAIL("%s: did not receive SIGKILL", description);
	} else {
		int status = 0;
		T_QUIET; T_ASSERT_POSIX_SUCCESS(waitpid(pid, &status, 0), "waitpid");
		T_EXPECT_EQ(WTERMSIG(status), SIGKILL,
		    "%s exited with %d, expect SIGKILL", description, WTERMSIG(status));
	}
}

struct msg_complex_port {
	mach_msg_base_t         base;
	mach_msg_port_descriptor_t dsc;
	mach_msg_max_trailer_t  trailer;
};

#define OOL_PORT_COUNTS 2

struct msg_complex_port_array {
	mach_msg_base_t         base;
	mach_msg_ool_ports_descriptor_t dsc;
	mach_msg_max_trailer_t  trailer;
	mach_port_name_t        array[OOL_PORT_COUNTS];
};

struct msg_complex_port_two_arrays {
	mach_msg_header_t header;
	mach_msg_base_t         base;
	mach_msg_ool_ports_descriptor_t dsc1;
	mach_msg_ool_ports_descriptor_t dsc2;
	mach_msg_max_trailer_t  trailer;
	mach_port_name_t        array[OOL_PORT_COUNTS];
};

static kern_return_t
send_msg(
	mach_port_t       dest_port,
	mach_msg_header_t *msg,
	mach_msg_size_t   size)
{
	mach_msg_option64_t     opts;

	opts = MACH64_SEND_MSG | MACH64_SEND_MQ_CALL | MACH64_SEND_TIMEOUT;

	msg->msgh_bits = MACH_MSGH_BITS_SET(MACH_MSG_TYPE_MAKE_SEND, 0, 0,
	    MACH_MSGH_BITS_COMPLEX);
	msg->msgh_size = size;
	msg->msgh_remote_port = dest_port;
	msg->msgh_local_port = MACH_PORT_NULL;
	msg->msgh_voucher_port = MACH_PORT_NULL;
	msg->msgh_id = 42;
	return mach_msg2(msg, opts, *msg, size, 0, 0, 0, 0);
}

static kern_return_t
send_port_descriptor(
	mach_port_t             dest_port,
	mach_port_t             dsc_port,
	int                     disp)
{
	struct msg_complex_port complex_msg;
	mach_msg_header_t *msg;
	mach_msg_size_t size;

	complex_msg = (struct msg_complex_port){
		.base.body.msgh_descriptor_count = 1,
		.dsc = {
			.type        = MACH_MSG_PORT_DESCRIPTOR,
			.disposition = disp,
			.name        = dsc_port,
		},
	};

	msg = &complex_msg.base.header;
	size = (mach_msg_size_t)((char *)&complex_msg.trailer - (char *)&complex_msg.base);
	return send_msg(dest_port, msg, size);
}

static mach_port_t
recv_port_descriptor(mach_port_t dst_port)
{
	struct msg_complex_port msg;

	kern_return_t kr = mach_msg2(&msg, MACH64_RCV_MSG, MACH_MSG_HEADER_EMPTY,
	    0, sizeof(msg), dst_port, 0, 0);
	T_ASSERT_MACH_SUCCESS(kr, "mach_msg2 receive port descriptor");

	/* extract and return the received port name */
	return msg.dsc.name;
}

static mach_port_t
get_send_receive_right(void)
{
	kern_return_t kr;
	mach_port_t port;

	kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "mach_port_allocate");

	kr = mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "mach_port_insert_right");

	return port;
}

static kern_return_t
send_ool_port_array(
	mach_port_t dest_port,
	mach_msg_type_name_t disp)
{
	struct msg_complex_port_array complex_msg;
	mach_msg_header_t *msg;
	mach_msg_size_t size;

	complex_msg = (struct msg_complex_port_array){
		.base.body.msgh_descriptor_count = 1,
		.dsc = {
			.type        = MACH_MSG_OOL_PORTS_DESCRIPTOR,
			.disposition = disp,
			.address     = &complex_msg.array,
			.count       = OOL_PORT_COUNTS,
			.deallocate  = false,
		},
	};

	for (size_t i = 0; i < OOL_PORT_COUNTS; ++i) {
		complex_msg.array[i] = get_send_receive_right();
	}

	msg = &complex_msg.base.header;
	size = (mach_msg_size_t)((char *)&complex_msg.trailer - (char *)&complex_msg.base);
	return send_msg(dest_port, msg, size);
}

static kern_return_t
send_ool_port_multiple_arrays(
	mach_port_t dest_port,
	mach_msg_type_name_t disp)
{
	struct msg_complex_port_two_arrays complex_msg;
	mach_msg_header_t *msg;
	mach_msg_size_t size;

	complex_msg = (struct msg_complex_port_two_arrays){
		.base.body.msgh_descriptor_count = 2,
		.dsc1 = {
			.type        = MACH_MSG_OOL_PORTS_DESCRIPTOR,
			.disposition = disp,
			.address     = &complex_msg.array,
			.count       = OOL_PORT_COUNTS,
			.deallocate  = false,
		},
		.dsc2 = {
			.type        = MACH_MSG_OOL_PORTS_DESCRIPTOR,
			.disposition = disp,
			.address     = &complex_msg.array,
			.count       = OOL_PORT_COUNTS,
			.deallocate  = false,
		},
	};

	for (size_t i = 0; i < OOL_PORT_COUNTS; ++i) {
		complex_msg.array[i] = get_send_receive_right();
	}

	msg = &complex_msg.base.header;
	size = (mach_msg_size_t)((char *)&complex_msg.trailer - (char *)&complex_msg.base);
	return send_msg(dest_port, msg, size);
}

/*
 * Helper constructor functions to create different types of ports.
 */
static mach_port_t
create_conn_with_port_array_port(void)
{
	kern_return_t kr;
	mach_port_t port;

	mach_port_options_t opts = {.flags = MPO_CONNECTION_PORT_WITH_PORT_ARRAY, };

	kr = mach_port_construct(mach_task_self(), &opts, 0x0, &port);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "mach_port_construct");

	return port;
}

static mach_port_t
create_exception_port(void)
{
	kern_return_t kr;
	mach_port_t port;

	mach_port_options_t opts = {.flags = MPO_EXCEPTION_PORT, };

	kr = mach_port_construct(mach_task_self(), &opts, 0x0, &port);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "mach_port_construct");

	return port;
}

static mach_port_t
create_connection_port(void)
{
	kern_return_t kr;
	mach_port_t conn_port;

	mach_port_options_t opts = {
		.flags = MPO_CONNECTION_PORT | MPO_INSERT_SEND_RIGHT,
		.service_port_name = MPO_ANONYMOUS_SERVICE,
	};

	kr = mach_port_construct(mach_task_self(), &opts, 0x0, &conn_port);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "mach_port_construct");

	return conn_port;
}

static mach_port_t
create_reply_port(void)
{
	kern_return_t kr;
	mach_port_t port;

	mach_port_options_t opts = {
		.flags = MPO_REPLY_PORT,
	};

	kr = mach_port_construct(mach_task_self(), &opts, 0x0, &port);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "mach_port_construct");

	return port;
}

static mach_port_t
create_provisional_reply_port(void)
{
	kern_return_t kr;
	mach_port_t port;

	mach_port_options_t opts = {
		.flags = MPO_PROVISIONAL_REPLY_PORT,
	};

	kr = mach_port_construct(mach_task_self(), &opts, 0x0, &port);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "mach_port_construct");

	return port;
}

static mach_port_t
create_service_port(void)
{
	kern_return_t kr;
	mach_port_t port;

	struct mach_service_port_info sp_info = {
		.mspi_string_name = "com.apple.testservice",
		.mspi_domain_type = XPC_DOMAIN_SYSTEM,
	};

	mach_port_options_t opts = {
		.flags = MPO_STRICT_SERVICE_PORT,
		.service_port_info = &sp_info,
	};

	kr = mach_port_construct(mach_task_self(), &opts, 0x0, &port);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "mach_port_construct");

	return port;
}

static void
destruct_generic_port(mach_port_t port)
{
	kern_return_t kr;
	mach_port_type_t type = 0;

	kr = mach_port_type(mach_task_self(), port, &type);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "mach_port_type");

	kr = mach_port_destruct(mach_task_self(),
	    port,
	    (type & MACH_PORT_TYPE_SEND) ? -1 : 0,
	    0);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "mach_port_destruct");
}
/*
 * Helper functions and types to help making test output nice and readable.
 */
static const char*
get_disp_name(mach_msg_type_name_t disp)
{
	switch (disp) {
	case MACH_MSG_TYPE_MOVE_SEND:
		return "MOVE_SEND";
	case MACH_MSG_TYPE_MAKE_SEND:
		return "MAKE_SEND";
	case MACH_MSG_TYPE_MOVE_SEND_ONCE:
		return "MOVE_SEND_ONCE";
	case MACH_MSG_TYPE_MAKE_SEND_ONCE:
		return "MAKE_SEND_ONCE";
	case MACH_MSG_TYPE_COPY_SEND:
		return "COPY_SEND";
	case MACH_MSG_TYPE_MOVE_RECEIVE:
		return "MOVE_RECEIVE";
	default:
		T_ASSERT_FAIL("Invalid disp");
	}
}

static const char*
get_notification_name(mach_msg_id_t notification_id)
{
	switch (notification_id) {
	case MACH_NOTIFY_PORT_DESTROYED:
		return "PORT_DESTROY";
		break;
	case MACH_NOTIFY_NO_SENDERS:
		return "NO_MORE_SENDERS";
		break;
	case MACH_NOTIFY_SEND_POSSIBLE:
		return "SEND_POSSIBLE";
		break;
	default:
		T_ASSERT_FAIL("Invalid notification id");
	}
}

typedef struct {
	mach_port_t (*port_ctor)(void);
	char *port_type_name;
	bool is_reply_port;
} port_type_desc;

const port_type_desc IOT_PORT_DESC = {
	.port_ctor = get_send_receive_right,
	.port_type_name = "IOT_PORT",
	.is_reply_port = false,
};
const port_type_desc REPLY_PORT_DESC = {
	.port_ctor = create_reply_port,
	.port_type_name = "IOT_REPLY_PORT",
	.is_reply_port = true,
};
const port_type_desc CONNECTION_PORT_DESC = {
	.port_ctor = create_connection_port,
	.port_type_name = "IOT_CONNECTION_PORT",
	.is_reply_port = false,
};
const port_type_desc EXCEPTION_PORT_DESC = {
	.port_ctor = create_exception_port,
	.port_type_name = "IOT_EXCEPTION_PORT",
	.is_reply_port = false,
};
const port_type_desc PROVISIONAL_REPLY_PORT_DESC = {
	.port_ctor = create_provisional_reply_port,
	.port_type_name = "IOT_PROVISIONAL_REPLY_PORT",
	.is_reply_port = false,
};
const port_type_desc CONNECTION_PORT_WITH_PORT_ARRAY_DESC = {
	.port_ctor = create_conn_with_port_array_port,
	.port_type_name = "IOT_CONNECTION_PORT_WITH_PORT_ARRAY",
	.is_reply_port = false,
};
const port_type_desc TIMER_PORT_DESC = {
	.port_ctor = mk_timer_create,
	.port_type_name = "IOT_TIMER_PORT",
	.is_reply_port = false,
};
const port_type_desc SPECIAL_REPLY_PORT_DESC = {
	.port_ctor = thread_get_special_reply_port,
	.port_type_name = "IOT_SPECIAL_REPLY_PORT",
	.is_reply_port = true,
};
const port_type_desc SERVICE_PORT_DESC = {
	.port_ctor = create_service_port,
	.port_type_name = "IOT_SERVICE_PORT",
	.is_reply_port = false,
};

const port_type_desc PORT_TYPE_DESC_ARRAY[] = {
	IOT_PORT_DESC,
	REPLY_PORT_DESC,
	CONNECTION_PORT_DESC,
	EXCEPTION_PORT_DESC,
	PROVISIONAL_REPLY_PORT_DESC,
	CONNECTION_PORT_WITH_PORT_ARRAY_DESC,
	TIMER_PORT_DESC,
	SPECIAL_REPLY_PORT_DESC,
	SERVICE_PORT_DESC
};

/*
 * Helper functions to test MachIPC functionalities.
 */
static void
test_disallowed_register_mach_notification(
	const port_type_desc *port_desc,
	mach_msg_id_t notify_id)
{
	expect_sigkill(^{
		mach_port_t port, notify_port, previous;

		/* construct a receive right to send the port as descriptor to */
		notify_port = get_send_receive_right();

		port = port_desc->port_ctor();
		(void)mach_port_request_notification(mach_task_self(),
		port,
		notify_id,
		0,
		notify_port,
		MACH_MSG_TYPE_MAKE_SEND_ONCE,
		&previous);

		/* Unreachable; ports will be destructed when IPC space is destroyed */
	}, "%s failed with mach notification %s", port_desc->port_type_name, get_notification_name(notify_id));
}

/*
 * In this helper function we cover two properties:
 *     - we make sure these ports are immovable-receive by trying to
 *       send them in a message with MACH_MSG_PORT_DESCRIPTOR descriptor;
 *     - we attempt to register them for a PD notification.
 *
 * This seems redundent since it is not possible to register immovable-receive
 * ports to PD notification by construction. However, we want our tests
 * to cover everything, and this link between immovable-receive and
 * PD notifications, no matter how trivial, should be question as well.
 *
 * Note: this intentionally does NOT use get status trap
 *       and test for MACH_PORT_STATUS_FLAG_GUARD_IMMOVABLE_RECEIVE,
 *       because the purpose of these tests is to ensure the overall security
 *       properties are respected (immovability, Guard, fatal exception, etc.).
 */
static void
test_receive_immovability(const port_type_desc *port_desc)
{
	expect_sigkill(^{
		mach_port_t dst_port, port;

		/* construct a receive right to send the port as descriptor to */
		dst_port = get_send_receive_right();

		/*
		 * construct the port to test immovability, and send it as port
		 * descriptor with RECEIVE right.
		 */
		port = port_desc->port_ctor();
		(void)send_port_descriptor(dst_port, port, MACH_MSG_TYPE_MOVE_RECEIVE);

		/* Unreachable; ports will be destructed when IPC space is destroyed */
	}, "%s failed immovable-receive", port_desc->port_type_name);

	test_disallowed_register_mach_notification(port_desc,
	    MACH_NOTIFY_PORT_DESTROYED);
}

/*
 * We have port types which their receive right is allowed to be move
 * ONCE, and then they become immovable-receive for the rest of their
 * lifetime.
 *
 * This helper function tests that property.
 */
static void
test_receive_immovability_move_once(const port_type_desc *port_desc)
{
	expect_sigkill(^{
		kern_return_t kr;
		mach_port_t dst_port, port;

		/* construct a receive right to send the port as descriptor to */
		dst_port = get_send_receive_right();

		/* construct the port for our test, and send it as port descriptor */
		port = port_desc->port_ctor();
		kr = send_port_descriptor(dst_port, port, MACH_MSG_TYPE_MOVE_RECEIVE);
		T_ASSERT_MACH_SUCCESS(kr, "send_port_descriptor");

		/* we moved the receive right out of our IPC space */
		port = MACH_PORT_NULL;

		/*
		 * receive the port we sent to ourselves.
		 *
		 * From now on, this port is expected to be immovable-receive
		 * for the rest of its lifetime.
		 */
		port = recv_port_descriptor(dst_port);

		/*
		 * this should raise a fatal Guard exception
		 * on immovability violation
		 */
		(void)send_port_descriptor(dst_port, port, MACH_MSG_TYPE_MOVE_RECEIVE);

		/* Unreachable; ports will be destructed when IPC space is destroyed */
	}, "%s is allowed to be move ONCE", port_desc->port_type_name);
}

static void
test_send_immovability_move_so(const port_type_desc *port_desc)
{
	expect_sigkill(^{
		mach_port_t dst_port, port, so_right;
		mach_msg_type_name_t disp;
		kern_return_t kr;

		dst_port = get_send_receive_right();
		port = port_desc->port_ctor();

		/* create a send-once right for the port */
		kr = mach_port_extract_right(mach_task_self(), port,
		MACH_MSG_TYPE_MAKE_SEND_ONCE, &so_right, &disp);

		T_ASSERT_MACH_SUCCESS(kr, "mach_port_extract_right with %s", port_desc->port_type_name);

		(void)send_port_descriptor(dst_port, so_right, MACH_MSG_TYPE_MOVE_SEND_ONCE);

		/* Unreachable; ports will be destructed when IPC space is destroyed */
	}, "%s immovable-send failed with MOVE_SEND_ONCE", port_desc->port_type_name);
}

static void
test_send_immovability(const port_type_desc *port_desc)
{
	expect_sigkill(^{
		mach_msg_type_name_t disp;
		mach_port_name_t name;

		mach_port_t port = port_desc->port_ctor();
		(void)mach_port_extract_right(mach_task_self(), port,
		MACH_MSG_TYPE_MOVE_SEND, &name, &disp);

		/* Unreachable; ports will be destructed when IPC space is destroyed */
	}, "%s immovable-send failed extract_right MOVE_SEND", port_desc->port_type_name);

	expect_sigkill(^{
		mach_port_t dst_port, port;

		/* construct a receive right to send the port as descriptor to */
		dst_port = get_send_receive_right();

		port = port_desc->port_ctor();
		(void)send_port_descriptor(dst_port, port, MACH_MSG_TYPE_MOVE_SEND);

		/* Unreachable; ports will be destructed when IPC space is destroyed */
	}, "%s immovable-send failed with MOVE_SEND", port_desc->port_type_name);

	expect_sigkill(^{
		mach_port_t dst_port, port;

		/* construct a receive right to send the port as descriptor to */
		dst_port = get_send_receive_right();

		port = port_desc->port_ctor();
		(void)send_port_descriptor(dst_port, port, MACH_MSG_TYPE_COPY_SEND);

		/* Unreachable; ports will be destructed when IPC space is destroyed */
	}, "%s immovable-send failed with COPY_SEND", port_desc->port_type_name);

	/*
	 * Do not attempt to extract SEND_ONCE for reply port types. Such behavior
	 * should be covered by the reply_port_defense test.
	 */
	if (!port_desc->is_reply_port) {
		test_send_immovability_move_so(port_desc);
	}
}

static void
test_ool_port_array(
	const port_type_desc *port_desc,
	mach_msg_type_name_t disp)
{
	expect_sigkill(^{
		mach_port_t dst_port;

		/* construct a receive right to send the port as descriptor to */
		dst_port = port_desc->port_ctor();

		(void)send_ool_port_array(dst_port, disp);

		/* Unreachable; ports will be destructed when IPC space is destroyed */
	}, "sending OOL port array to %s with %s", port_desc->port_type_name, get_disp_name(disp));
}

/*
 * Because of mach hardening opt out, group
 * reply port tests together and skip them.
 */
T_DECL(reply_port_policies,
    "Reply port policies tests") {
#if TARGET_OS_OSX || TARGET_OS_BRIDGE
	T_SKIP("Test disabled on macOS due to mach hardening opt out");
#endif /* TARGET_OS_OSX || TARGET_OS_BRIDGE */

	test_receive_immovability(&REPLY_PORT_DESC);

	test_send_immovability(&REPLY_PORT_DESC);

	test_disallowed_register_mach_notification(&REPLY_PORT_DESC,
	    MACH_NOTIFY_NO_SENDERS);
}

T_DECL(immovable_receive_port_types,
    "Port types we expect to be immovable-receive") {
	test_receive_immovability(&CONNECTION_PORT_WITH_PORT_ARRAY_DESC);

	test_receive_immovability(&EXCEPTION_PORT_DESC);

	test_receive_immovability(&TIMER_PORT_DESC);

	test_receive_immovability(&SPECIAL_REPLY_PORT_DESC);

	/*
	 * kGUARD_EXC_KERN_FAILURE is not fatal on Bridge OS because
	 * we don't set TASK_EXC_GUARD_MP_FATAL by default/
	 */
#if !TARGET_OS_BRIDGE
	test_receive_immovability(&SERVICE_PORT_DESC);
#endif /* !TARGET_OS_BRIDGE */
}

T_DECL(immovable_receive_move_once_port_types,
    "Port types we expect to be immovable-receive") {
	test_receive_immovability_move_once(&CONNECTION_PORT_DESC);
}

T_DECL(immovable_send_port_types,
    "Port types we expect to be immovable-send")
{
	test_send_immovability(&CONNECTION_PORT_DESC);

	test_send_immovability(&SPECIAL_REPLY_PORT_DESC);
}

T_DECL(ool_port_array_policies,
    "OOL port array policies")
{
#if TARGET_OS_VISION
	T_SKIP("OOL port array enforcement is disabled");
#else
	if (ipc_hardening_disabled()) {
		T_SKIP("hardening disabled due to boot-args");
	}

	/*
	 * The only port type allowed to receive the MACH_MSG_OOL_PORTS_DESCRIPTOR
	 * descriptor is IOT_CONNECTION_PORT_WITH_PORT_ARRAY.
	 *
	 * Attempt sending MACH_MSG_OOL_PORTS_DESCRIPTOR to any other port type
	 * result in a fatal Guard exception.
	 */
	test_ool_port_array(&IOT_PORT_DESC,
	    MACH_MSG_TYPE_COPY_SEND);

	test_ool_port_array(&REPLY_PORT_DESC,
	    MACH_MSG_TYPE_COPY_SEND);

	test_ool_port_array(&SPECIAL_REPLY_PORT_DESC,
	    MACH_MSG_TYPE_COPY_SEND);

	test_ool_port_array(&CONNECTION_PORT_DESC,
	    MACH_MSG_TYPE_COPY_SEND);

	test_ool_port_array(&EXCEPTION_PORT_DESC,
	    MACH_MSG_TYPE_COPY_SEND);

	test_ool_port_array(&PROVISIONAL_REPLY_PORT_DESC,
	    MACH_MSG_TYPE_COPY_SEND);

	test_ool_port_array(&TIMER_PORT_DESC,
	    MACH_MSG_TYPE_COPY_SEND);

	/*
	 * Now try to send to IOT_CONNECTION_PORT_WITH_PORT_ARRAY ports,
	 * but use disallowed dispositions.
	 *
	 * The only allowed disposition is COPY_SEND.
	 */
	test_ool_port_array(&CONNECTION_PORT_WITH_PORT_ARRAY_DESC,
	    MACH_MSG_TYPE_MOVE_SEND);

	test_ool_port_array(&CONNECTION_PORT_WITH_PORT_ARRAY_DESC,
	    MACH_MSG_TYPE_MAKE_SEND);

	test_ool_port_array(&CONNECTION_PORT_WITH_PORT_ARRAY_DESC,
	    MACH_MSG_TYPE_MOVE_SEND_ONCE);

	test_ool_port_array(&CONNECTION_PORT_WITH_PORT_ARRAY_DESC,
	    MACH_MSG_TYPE_MAKE_SEND_ONCE);

	test_ool_port_array(&CONNECTION_PORT_WITH_PORT_ARRAY_DESC,
	    MACH_MSG_TYPE_MOVE_RECEIVE);

	/*
	 * Finally, try sending OOL port array to IOT_CONNECTION_PORT_WITH_PORT_ARRAY,
	 * with (the only) allowed disposition, but send two arrays in one kmsg.
	 */
	expect_sigkill(^{
		mach_port_t dst_port;

		/* construct a receive right to send the port as descriptor to */
		dst_port = create_conn_with_port_array_port();

		(void)send_ool_port_multiple_arrays(dst_port, MACH_MSG_TYPE_COPY_SEND);

		/* Unreachable; ports will be destructed when IPC space is destroyed */
	}, "sending two OOL port arrays");
#endif /* TARGET_OS_VISION */
}

T_DECL(disallowed_no_more_senders_port_destroy_port_types,
    "Port types we disallow no-more-senders notifications for")
{
	test_disallowed_register_mach_notification(&SPECIAL_REPLY_PORT_DESC,
	    MACH_NOTIFY_NO_SENDERS);
}

T_DECL(provisional_reply_port,
    "Provisional reply ports have no restrictions")
{
	mach_port_t prp, remote_port, recv_port;
	kern_return_t kr;

	prp = create_provisional_reply_port();
	remote_port = get_send_receive_right();

	kr = mach_port_insert_right(mach_task_self(), prp, prp,
	    MACH_MSG_TYPE_MAKE_SEND);
	T_ASSERT_MACH_SUCCESS(kr, "mach_port_insert_right");

	/* send a send right to the provisional reply port*/
	kr = send_port_descriptor(remote_port, prp, MACH_MSG_TYPE_MOVE_SEND);
	T_ASSERT_MACH_SUCCESS(kr, "send_port_descriptor");

	/* receive that port descriptor, which has to have the same name */
	recv_port = recv_port_descriptor(remote_port);
	T_QUIET; T_ASSERT_EQ(prp, recv_port, "recv_port_descriptor send");

	/* drop only the send right of the provisional reply port */
	kr = mach_port_mod_refs(mach_task_self(), prp, MACH_PORT_RIGHT_SEND, -1);

	/* send a receive right to the provisional reply port */
	kr = send_port_descriptor(remote_port, prp, MACH_MSG_TYPE_MOVE_RECEIVE);
	T_ASSERT_MACH_SUCCESS(kr, "send_port_descriptor");

	recv_port = recv_port_descriptor(remote_port);
	T_ASSERT_NE(recv_port, MACH_PORT_NULL, "recv_port_descriptor receive");

	/* cleanup, destruct the ports we used */
	kr = mach_port_destruct(mach_task_self(), recv_port, 0, 0);
	T_ASSERT_MACH_SUCCESS(kr, "mach_port_destruct recv_port");

	kr = mach_port_destruct(mach_task_self(), remote_port, 0, 0);
	T_ASSERT_MACH_SUCCESS(kr, "mach_port_destruct remote_port");
}

T_DECL(mktimer_traps,
    "Test mktimer traps")
{
	kern_return_t kr;
	mach_port_t port;
	uint64_t result_time;

	/*
	 * Enumerate all port types, makes sure mk_timer_arm
	 * fails on every single one besides IOT_TIMER_PORT
	 */
	for (uint32_t i = 0; i < countof(PORT_TYPE_DESC_ARRAY); ++i) {
		if (PORT_TYPE_DESC_ARRAY[i].port_ctor == mk_timer_create) {
			continue;
		}

		/* Create a non-timer port type */
		port = PORT_TYPE_DESC_ARRAY[i].port_ctor();
		T_QUIET; T_ASSERT_NE(port, MACH_PORT_NULL,
		    "constructing a port type %s",
		    PORT_TYPE_DESC_ARRAY[i].port_type_name);

		kr = mk_timer_arm(port, 1);
		T_ASSERT_MACH_ERROR(kr,
		    KERN_INVALID_ARGUMENT,
		    "mk_timer_arm failed on non timer port type (%s)",
		    PORT_TYPE_DESC_ARRAY[i].port_type_name);

		kr = mk_timer_cancel(port, &result_time);
		T_ASSERT_MACH_ERROR(kr,
		    KERN_INVALID_ARGUMENT,
		    "mk_timer_cancel failed on non timer port type (%s)",
		    PORT_TYPE_DESC_ARRAY[i].port_type_name);

		kr = mk_timer_destroy(port);
		T_ASSERT_MACH_ERROR(kr,
		    KERN_INVALID_ARGUMENT,
		    "mk_timer_destroy failed on non timer port type (%s)",
		    PORT_TYPE_DESC_ARRAY[i].port_type_name);

		/* Destroy the port we created */
		destruct_generic_port(port);
	}

	/* Verify mk_timer_arm succeed with actual timer */
	port = TIMER_PORT_DESC.port_ctor();
	T_QUIET; T_ASSERT_NE(port, MACH_PORT_NULL,
	    "constructing a timer (%s)",
	    TIMER_PORT_DESC.port_type_name);

	kr = mk_timer_arm(port, 1);
	T_ASSERT_MACH_SUCCESS(kr, "mk_timer_arm on actual timer");

	kr = mk_timer_cancel(port, &result_time);
	T_ASSERT_MACH_SUCCESS(kr, "mk_timer_cancel on actual timer");

	kr = mk_timer_destroy(port);
	T_ASSERT_MACH_SUCCESS(kr, "mk_timer_destroy");
}
