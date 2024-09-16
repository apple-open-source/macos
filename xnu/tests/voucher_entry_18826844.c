/*
 * Test that sending a message to a voucher with the same voucher as the voucher port
 * with only one send right count with move send before the copy send doesn't panic.
 *
 * clang -o voucherentry voucherentry.c -ldarwintest -Weverything -Wno-gnu-flexible-array-initializer
 *
 * <rdar://problem/18826844>
 */

#include <mach/mach.h>
#include <darwintest.h>

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.ipc"),
	T_META_RUN_CONCURRENTLY(TRUE),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("IPC"));

T_DECL(voucher_entry, "voucher_entry", T_META_CHECK_LEAKS(false), T_META_ALL_VALID_ARCHS(true), T_META_TAG_VM_PREFERRED)
{
	kern_return_t kr        = KERN_SUCCESS;
	mach_voucher_t voucher  = MACH_VOUCHER_NULL;
	mach_port_name_t reply;
	mach_port_options_t opts = {
		.flags = MPO_REPLY_PORT,
	};

	/*
	 * The bank voucher already exists in this process, so using it doesn't
	 * actually test the problem. Use an importance voucher instead.
	 */
	mach_voucher_attr_recipe_data_t recipe = {
		.key                = MACH_VOUCHER_ATTR_KEY_IMPORTANCE,
		.command            = MACH_VOUCHER_ATTR_IMPORTANCE_SELF,
		.previous_voucher   = MACH_VOUCHER_NULL,
		.content_size       = 0,
	};

	kr = host_create_mach_voucher(mach_host_self(),
	    (mach_voucher_attr_raw_recipe_array_t)&recipe,
	    sizeof(recipe), &voucher);

	T_ASSERT_MACH_SUCCESS(kr, "host_create_mach_voucher");

	T_ASSERT_NOTNULL(voucher, "voucher must not be null");

	mach_port_urefs_t refs = 0;

	kr = mach_port_get_refs(mach_task_self(), voucher, MACH_PORT_RIGHT_SEND, &refs);

	T_ASSERT_MACH_SUCCESS(kr, "mach_port_get_refs");

	T_ASSERT_EQ(refs, (mach_port_urefs_t)1, "voucher must have only one ref");

	T_ASSERT_MACH_SUCCESS(mach_port_construct(mach_task_self(),
	    &opts, 0, &reply), "make reply port");

	/* First, try with two moves (must fail because there's only one ref) */
	mach_msg_header_t request_msg_1 = {
		.msgh_remote_port   = voucher,
		.msgh_local_port    = reply,
		.msgh_voucher_port  = voucher,
		.msgh_bits          = MACH_MSGH_BITS_SET(MACH_MSG_TYPE_MOVE_SEND,
	    MACH_MSG_TYPE_MAKE_SEND_ONCE, MACH_MSG_TYPE_MOVE_SEND, 0),
		.msgh_id            = 0xDEAD,
		.msgh_size          = sizeof(request_msg_1),
	};

	kr = mach_msg2(&request_msg_1, MACH64_SEND_MSG | MACH64_RCV_MSG | MACH64_SEND_KOBJECT_CALL,
	    request_msg_1, request_msg_1.msgh_size, 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, 0);

	T_ASSERT_MACH_ERROR(MACH_SEND_INVALID_DEST, kr, "send with two moves should fail with invalid dest");

	T_ASSERT_MACH_SUCCESS(mach_port_destruct(mach_task_self(), reply, 0, 0),
	    "destroy reply port");

	T_ASSERT_MACH_SUCCESS(mach_port_construct(mach_task_self(),
	    &opts, 0, &reply), "make reply port");

	/* Next, try with a move and a copy (will succeed and destroy the last ref) */
	union {
		mach_msg_header_t hdr;
		struct {
			mig_reply_error_t err;
			mach_msg_trailer_t trailer;
		};
	} request_msg_2 = {
		.hdr = {
			.msgh_remote_port   = voucher,
			.msgh_local_port    = reply,
			.msgh_voucher_port  = voucher,
			.msgh_bits          = MACH_MSGH_BITS_SET(MACH_MSG_TYPE_MOVE_SEND,
	    MACH_MSG_TYPE_MAKE_SEND_ONCE, MACH_MSG_TYPE_COPY_SEND, 0),
			.msgh_id            = 0xDEAD,
			.msgh_size          = sizeof(request_msg_2),
		}
	};

	/* panic happens here */
	kr = mach_msg2(&request_msg_2, MACH64_SEND_MSG | MACH64_RCV_MSG | MACH64_SEND_KOBJECT_CALL,
	    request_msg_2.hdr, request_msg_2.hdr.msgh_size, sizeof(request_msg_2),
	    reply, MACH_MSG_TIMEOUT_NONE, 0);

	T_ASSERT_MACH_SUCCESS(kr, "send with move and copy succeeds");

	T_ASSERT_MACH_SUCCESS(mach_port_destruct(mach_task_self(), reply, 0, 0),
	    "destroy reply port");

	kr = mach_port_get_refs(mach_task_self(), voucher, MACH_PORT_RIGHT_SEND, &refs);

	T_ASSERT_MACH_ERROR(KERN_INVALID_NAME, kr, "voucher should now be invalid name");
}
