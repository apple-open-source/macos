//
//  MessagePrinting.cpp
//  msa
//
//  Created by James McIlree on 2/5/14.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#include "global.h"

const char* qos_to_string(uint32_t qos) {
	static_assert(THREAD_QOS_LAST == 7, "QOS tiers need updating");

	switch (qos) {
		case THREAD_QOS_UNSPECIFIED:
			return "unspecified";

		case THREAD_QOS_MAINTENANCE:
			return "maintenance";

		case THREAD_QOS_BACKGROUND:
			return "background";

		case THREAD_QOS_UTILITY:
			return "utility";

		case THREAD_QOS_LEGACY:
			return "legacy";

		case THREAD_QOS_USER_INITIATED:
			return "user-initiated";

		case THREAD_QOS_USER_INTERACTIVE:
			return "user-interactive";

		default:
			ASSERT(false, "Unhandled QoS");
			return "QOS_???";
	}
}

const char* qos_to_short_string(uint32_t qos) {
	static_assert(THREAD_QOS_LAST == 7, "QOS tiers need updating");

	switch (qos) {
		case THREAD_QOS_UNSPECIFIED:
			return "Unspec";

		case THREAD_QOS_MAINTENANCE:
			return "Maint";

		case THREAD_QOS_BACKGROUND:
			return "BG";

		case THREAD_QOS_UTILITY:
			return "Util";

		case THREAD_QOS_LEGACY:
			return "Legacy";

		case THREAD_QOS_USER_INITIATED:
			return "UInit";

		case THREAD_QOS_USER_INTERACTIVE:
			return "UI";

		default:
			ASSERT(false, "Unhandled QoS");
			return "???";
	}
}

const char* role_to_short_string(uint32_t role) {
	switch (role) {
			// This is seen when apps are terminating
		case TASK_UNSPECIFIED:
			return "unspec";

		case TASK_FOREGROUND_APPLICATION:
			return "fg";

		case TASK_BACKGROUND_APPLICATION:
			return "bg";

		case TASK_CONTROL_APPLICATION:
		case TASK_GRAPHICS_SERVER:
		case TASK_THROTTLE_APPLICATION:
		case TASK_NONUI_APPLICATION:
			ASSERT(false, "These should be obsolete");
			return "obsolete";

		case TASK_DEFAULT_APPLICATION:
			// Is this obsolete too?
			return "defapp";

		default:
			ASSERT(false, "Unexpected app role");
			return "???";
	}
}

const char* role_to_string(uint32_t role) {
	switch (role) {
			// This is seen when apps are terminating
		case TASK_UNSPECIFIED:
			return "unspecified";

		case TASK_FOREGROUND_APPLICATION:
			return "foreground";

		case TASK_BACKGROUND_APPLICATION:
			return "background";

		case TASK_CONTROL_APPLICATION:
			return "control-application";

		case TASK_GRAPHICS_SERVER:
			return "graphics-server";

		case TASK_THROTTLE_APPLICATION:
			return "throttle-app";

		case TASK_NONUI_APPLICATION:
			return "nonui-app";

		case TASK_DEFAULT_APPLICATION:
			// Is this obsolete too?
			return "default-app";

		default:
			ASSERT(false, "Unexpected app role");
			return "???";
	}
}

void print_base_empty(PrintBuffer& buffer,
		      const Globals& globals,
		      uintptr_t event_index,
		      const char* type,
		      bool should_newline)
{
	// Base Header is... (32)
	//
	//         Time(µS)                    Type      Thread     ThreadVoucher            AppType                   Process  ;;
	// 123456789abcdef0  1234567890123456789012  1234567890  123456789abcdef0  12345678901234567  123456789012345678901234  12
	//            14.11           mach_msg_send        18FB       voucher-133     AdaptiveDaemon            TextEdit (231)  ;;
	//            18.11           mach_msg_recv        18FB                 0  InteractiveDaemon           configd (19981)  ;;

	// Base Header is... (64)
	//
	//         Time(µS)                    Type      Thread     ThreadVoucher            AppType                   Process  ;;
	// 123456789abcdef0  1234567890123456789012  1234567890  123456789abcdef0  12345678901234567  123456789012345678901234  12
	//            14.11           mach_msg_send        18FB       voucher-133     AdaptiveDaemon            TextEdit (231)  ;;
	//            18.11           mach_msg_recv        18FB                 0  InteractiveDaemon           configd (19981)  ;;

	//
	// [Index]
	//
	if (globals.should_print_event_index()) {
		buffer.printf("%8llu ", (uint64_t)event_index);
	}

	//
	// Time Type Code Thread ThreadVoucher AppType Process
	//
	// This assert doesn't handle utf8...
	ASSERT(strlen(type) <= 22, "Sanity");

	buffer.printf("%16s  %3s  %22s  %10s  %16s  %17s  %24s  ;;", "-", "-", type, "-", "-", "-", "- (-)");

	//
	// Process
	//
	if (should_newline)
		buffer.printf("\n");
	else
		buffer.printf("  ");
}

static char* print_base_header(char* buf, char* buf_end, const Globals& globals) {
        // Base Header is... (32)
        //
        //         Time(µS)                    Type      Thread  ThrVoucher                   Process  ;;
        // 123456789abcdef0  1234567890123456789012  1234567890  123456789a  123456789012345678901234  12
        //            14.11           mach_msg_send        18FB    FFFF8E44            TextEdit (231)  ;;
        //            18.11           mach_msg_recv        18FB           0           configd (19981)  ;;

        // Base Header is... (64)
        //
        //         Time(µS)                    Type      Thread     ThreadVoucher                   Process  ;;
        // 123456789abcdef0  1234567890123456789012  1234567890  123456789abcdef0  123456789012345678901234  12
        //            14.11           mach_msg_send        18FB  BBBBAAEE55778234            TextEdit (231)  ;;
        //            18.11           mach_msg_recv        18FB                 0           configd (19981)  ;;

	//
	// If we cannot print successfully, we return the orignal pointer.
	//
	char* orig_buf = buf;

	if (globals.should_print_event_index())
		buf += snprintf(buf, buf_end - buf,"%8s ", "Event#");

	if (buf >= buf_end)
		return orig_buf;

	// The character counting for "Time(µS)" is OBO, it treats the µ as two characters.
	// This means the %16s misaligns. We force it by making the input string 16 printable chars long,
	// which overflows the %16s to the correct actual output length.
	const char* time = globals.should_print_mach_absolute_timestamps() ? "Time(mach-abs)" : "        Time(µS)";

	if (globals.kernel_size() == KernelSize::k32)
		buf += snprintf(buf, buf_end - buf, "%s  %22s  %10s  %10s  %24s  ;;  ", time, "Type", "Thread", "ThrVoucher", "Process");
	else
		buf += snprintf(buf, buf_end - buf, "%s  %22s  %10s  %16s  %24s  ;;  ", time, "Type", "Thread", "ThreadVoucher", "Process");

	return (buf >= buf_end) ? orig_buf : buf;
}

char* print_mach_msg_header(char* buf, char* buf_end, const Globals& globals) {

	// MachMsg Header is... (32)
	//
	// ;;  Message From/To                  MsgID  MsgVoucher   DeliveryTime  FLAGS
	// 12  123456789012345678901234567  123456789  123456789a  1234567890123  ...
	// ;;  -> configd (19981)                 55           -              -  ONEWAY, IMP-DONATING
	// ;;  <- TextEdit (231)                  55    FFFF8E44         120080  VOUCHER-PROVIDED-BY-KERNEL, VOUCHER-REFUSED

	// MachMsg Header is... (64)
	//
	// ;;  Message From/To                  MsgID        MsgVoucher   DeliveryTime  FLAGS
	// 12  123456789012345678901234567  123456789  123456789abcdef0  1234567890123  ...
	// ;;  -> configd (19981)                 55                 -              -  ONEWAY, IMP-DONATING
	// ;;  <- TextEdit (231)                  55  FFFFAAEE55778234         120080  VOUCHER-PROVIDED-BY-KERNEL, VOUCHER-REFUSED

	char* orig_buf = buf;

	//
	// Base Header
	//
	buf = print_base_header(buf, buf_end, globals);

	if  (buf == orig_buf)
		return orig_buf;

	//
	// Mach Msg Header
	//
	if (globals.kernel_size() == KernelSize::k32)
		buf += snprintf(buf, buf_end - buf, "%-27s  %9s  %10s  %13s  %s\n", "Message-From/To", "MsgID", "MsgVoucher", "DeliveryTime", "FLAGS");
	else
		buf += snprintf(buf, buf_end - buf, "%-27s  %9s  %16s  %13s  %s\n", "Message-From/To", "MsgID", "MsgVoucher", "DeliveryTime", "FLAGS");

	return (buf >= buf_end) ? orig_buf : buf;
}
