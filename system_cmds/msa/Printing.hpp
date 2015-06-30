//
//  MessagePrinting.h
//  msa
//
//  Created by James McIlree on 2/5/14.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#ifndef __msa__MessagePrinting__
#define __msa__MessagePrinting__

char* print_mach_msg_header(char*, char*, const Globals&);
char* print_thread_set_voucher_header(char* buf, char* buf_end, const Globals& globals);
const char* qos_to_string(uint32_t qos);
const char* qos_to_short_string(uint32_t qos);
const char* role_to_string(uint32_t role);
const char* role_to_short_string(uint32_t role);
void print_base_empty(PrintBuffer& buffer, const Globals& globals, uintptr_t event_index, const char* type, bool should_newline);

template <typename SIZE>
void print_base(PrintBuffer& buffer,
		const Globals& globals,
		AbsTime timestamp,
		const MachineThread<SIZE>* thread,
		const KDEvent<SIZE>& event,
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
	// Time
	//
	if (globals.should_print_mach_absolute_timestamps()) {
		if (globals.beginning_of_time().value() == 0)
			buffer.printf("%16llX  ", (timestamp - globals.beginning_of_time()).value());
		else
			buffer.printf("%16llu  ", (timestamp - globals.beginning_of_time()).value());
	} else {
		NanoTime ntime = (timestamp - globals.beginning_of_time()).nano_time(globals.timebase());
		buffer.printf("%16.2f  ", (double)ntime.value() / 1000.0);
	}

	//
	// beg/end/---
	//
	buffer.printf("%3s  ", event.is_func_start() ?  "beg" : (event.is_func_end() ? "end" : "---"));

	//
	// Type Code, Thread
	//

	// This assert doesn't handle utf8...
	ASSERT(strlen(type) <= 22, "Sanity");
	if (SIZE::is_64_bit)
		buffer.printf("%22s  %10llX  ", type, (uint64_t)thread->tid());
	else
		buffer.printf("%22s  %10llX  ", type, (uint64_t)thread->tid());

	//
	// ThreadVoucher
	//
	auto thread_voucher = (thread) ? thread->voucher(timestamp) : &Machine<SIZE>::UnsetVoucher;

	if (thread_voucher->is_unset()) {
		buffer.printf("%16s  ", "-");
	} else if (thread_voucher->is_null()) {
		buffer.printf("%16s  ", "0");
	} else {
		char voucher_id[32];
		snprintf(voucher_id, sizeof(voucher_id), "voucher-%u", thread_voucher->id());
		buffer.printf("%16s  ", voucher_id);
	}

	//
	// AppType
	//
	const char* apptype_string = nullptr;
	switch (thread->process().apptype()) {
		case -1:
			apptype_string = "-";
			break;
		case TASK_APPTYPE_NONE:
			apptype_string = "None";
			break;
		case TASK_APPTYPE_DAEMON_INTERACTIVE:
			apptype_string = "InteractiveDaemon";
			break;
		case TASK_APPTYPE_DAEMON_STANDARD:
			apptype_string = "StandardDaemon";
			break;
		case TASK_APPTYPE_DAEMON_ADAPTIVE:
			apptype_string = "AdaptiveDaemon";
			break;
		case TASK_APPTYPE_DAEMON_BACKGROUND:
			apptype_string = "BackgroundDaemon";
			break;
		case TASK_APPTYPE_APP_DEFAULT:
			apptype_string = "App";
			break;
		case TASK_APPTYPE_APP_TAL:
			apptype_string = "TALApp";
			break;
		default:
			apptype_string = "???";
			break;
	}
	buffer.printf("%17s  ", apptype_string);

	//
	// Process
	//
	char process_name[32];

	// Should not ever fail, but...
	if (thread) {
		const MachineProcess<SIZE>& process = thread->process();
		snprintf(process_name, sizeof(process_name), "%s (%d)", process.name(), process.pid());
	} else {
		snprintf(process_name, sizeof(process_name), "???");
	}

	if (should_newline)
		buffer.printf("%24s  ;;\n", process_name);
	else
		buffer.printf("%24s  ;;  ", process_name);
}

template <typename SIZE>
void print_mach_msg(PrintBuffer& buffer,
		    const Globals& globals,
		    const Machine<SIZE>& machine,
		    const KDEvent<SIZE>& event,
		    uintptr_t event_index,
		    bool is_send,
		    const MachineMachMsg<SIZE>* mach_msg)
{
	// Mach Msg Header is... (32)
	//
	// ;;  Message From/To                    MsgID        MsgVoucher   DeliveryTime  FLAGS
	// 12  123456789012345678901234567  123456789ab  123456789abcdef0  1234567890123  ...
	// ;;  -> configd (19981)                    55                 -              -  ONEWAY, IMP-DONATING
	// ;;  <- TextEdit (231)                     55       voucher-133         120080  VOUCHER-PROVIDED-BY-KERNEL, VOUCHER-REFUSED

	// Mach Msg Header is... (64)
	//
	// ;;  Message From/To                    MsgID        MsgVoucher   DeliveryTime  FLAGS
	// 12  123456789012345678901234567  123456789ab  123456789abcdef0  1234567890123  ...
	// ;;  -> configd (19981)                    55                 -              -  ONEWAY, IMP-DONATING
	// ;;  <- TextEdit (231)                     55       voucher-133         120080  VOUCHER-PROVIDED-BY-KERNEL, VOUCHER-REFUSED

	print_base(buffer, globals, event.timestamp(), machine.thread(event.tid(), event.timestamp()), event, event_index, is_send ? "mach_msg_send" : "mach_msg_recv", false);

	//
	// Message From/To
	//
	{
		char from_to_name[32];
		const MachineThread<SIZE>* from_to_thread = NULL;
		const char* from_to_direction;

		if (is_send) {
			from_to_direction = "->";
			if (mach_msg->has_receiver())
				from_to_thread = machine.thread(mach_msg->recv_tid(), mach_msg->recv_time());
		} else {
			from_to_direction = "<-";
			if (mach_msg->has_sender())
				from_to_thread = machine.thread(mach_msg->send_tid(), mach_msg->send_time());
		}

		if (from_to_thread) {
			const MachineProcess<SIZE>& from_to_process = from_to_thread->process();
			snprintf(from_to_name, sizeof(from_to_name), "%s %s (%d)", from_to_direction, from_to_process.name(), from_to_process.pid());
		} else {
			// (???) is a trigraph, break up by escaping one of the ?
			snprintf(from_to_name, sizeof(from_to_name), "%s ??? (??\?)", from_to_direction);
		}

		buffer.printf("%-27s  ", from_to_name);
	}

	//
	// MsgID
	//

	char msg_id[32];
	snprintf(msg_id, sizeof(msg_id), "msg-%u", mach_msg->id());
	buffer.printf("%11s  ", msg_id);

	//
	// MsgVoucher
	//
	// We want to differentiate between sending a NULL voucher and not having msgh_bits set.
	// We will show a NULL voucher as 0, but if msgh_bits says no voucher was sent, we will show "-"
	//

	MachineVoucher<SIZE>* msg_voucher = (is_send) ? mach_msg->send_voucher() : mach_msg->recv_voucher();

	if (msg_voucher->is_unset()) {
		buffer.printf("%16s  ", "-");
	} else if (msg_voucher->is_null()) {
		buffer.printf("%16s  ", "0");
	} else {
		char voucher_id[32];
		snprintf(voucher_id, sizeof(voucher_id), "voucher-%u", msg_voucher->id());
		buffer.printf("%16s  ", voucher_id);
	}

	//
	// DeliveryTime
	//

	if (!is_send) {
		if (mach_msg->has_sender()) {
			NanoTime ntime = (mach_msg->recv_time() - mach_msg->send_time()).nano_time(globals.timebase());
			buffer.printf("%13.2f  ", (double)ntime.value() / 1000.0);
		} else {
			buffer.printf("%13s  ", "?");
		}
	} else {
		buffer.printf("%13s  ", "-");
	}

	//
	// FLAGS
	//
	const char* separator = "";

	if (is_send) {
		if (!MACH_MSGH_BITS_HAS_LOCAL(mach_msg->send_msgh_bits())) {
			buffer.printf("%sONEWAY", separator);
			separator = ", ";
		}

		if (MACH_MSGH_BITS_RAISED_IMPORTANCE(mach_msg->send_msgh_bits())) {
			buffer.printf("%sMSGH_BITS_RAISED_IMPORTANCE", separator);
			separator = ", ";
		}

		if (MACH_MSGH_BITS_HOLDS_IMPORTANCE_ASSERTION(mach_msg->send_msgh_bits())) {
			buffer.printf("%sMSGH_BITS_HOLDS_IMPORTANCE_ASSERTION", separator);
			separator = ", ";
		}
	} else {
		if (mach_msg->is_voucher_refused()) {
			// FIX ME!
			// Need to test this... Can we tell if a voucher was refused without the
			// send voucher?
			//
			if (mach_msg->has_non_null_send_voucher() || mach_msg->has_non_null_recv_voucher()) {
				buffer.printf("%sVOUCHER-REFUSED", separator);
			}

			separator = ", ";
		}
		if (MACH_MSGH_BITS_RAISED_IMPORTANCE(mach_msg->recv_msgh_bits())) {
			buffer.printf("%sMSGH_BITS_RAISED_IMPORTANCE", separator);
			separator = ", ";
		}

		if (MACH_MSGH_BITS_HOLDS_IMPORTANCE_ASSERTION(mach_msg->recv_msgh_bits())) {
			buffer.printf("%sMSGH_BITS_HOLDS_IMPORTANCE_ASSERTION", separator);
			separator = ", ";
		}
	}

	//
	// MsgVoucher transformation
	//
	{
		char transformed_voucher[32];

		if (mach_msg->has_sender() && mach_msg->has_receiver()) {
			auto send_voucher = mach_msg->send_voucher();
			auto recv_voucher = mach_msg->recv_voucher();

			if (send_voucher != recv_voucher) {
				auto changed_voucher = (is_send) ? recv_voucher : send_voucher;
				auto changed_tense = (is_send) ? "becomes" : "was";

				if (changed_voucher->is_unset()) {
					snprintf(transformed_voucher, sizeof(transformed_voucher), "(%s -)", changed_tense);
				} else if (changed_voucher->is_null()) {
					snprintf(transformed_voucher, sizeof(transformed_voucher), "(%s 0)", changed_tense);
				} else {
					snprintf(transformed_voucher, sizeof(transformed_voucher), "(%s voucher-%u)", changed_tense, changed_voucher->id());
				}

				buffer.printf("%sVOUCHER_CHANGED %s", separator, transformed_voucher);
			}
		}
	}
	
	buffer.printf("\n");
}

template <typename SIZE>
void print_boost(PrintBuffer& buffer,
		 const Globals& globals,
		 const Machine<SIZE>& machine,
		 const KDEvent<SIZE>& event,
		 uintptr_t event_index,
		 pid_t boost_receiver_pid,
		 bool is_boost)
{

	// Base Header is... (32)
	//
	// ;;
	// 12
	// ;; BOOST foobard (338)

	// Base Header is... (64)
	//
	// ;;
	// 12
	// ;; BOOST foobard (338)


	print_base(buffer, globals, event.timestamp(), machine.thread(event.tid(), event.timestamp()), event, event_index, is_boost ? "boost" : "unboost", false);

	//
	// Boost target
	//

	const MachineProcess<SIZE>* target = machine.process(boost_receiver_pid, event.timestamp());
	const char* target_name;

	if (target) {
		target_name = target->name();
	} else {
		target_name = "???";
	}

	const char* action = is_boost ? "BOOST" : "UNBOOST";

	buffer.printf("%s %s (%d)\n", action, target_name, boost_receiver_pid);
}

template <typename SIZE>
void print_impdelv(PrintBuffer& buffer,
		   const Globals& globals,
		   const Machine<SIZE>& machine,
		   const KDEvent<SIZE>& event,
		   uintptr_t event_index,
		   pid_t sender_pid,
		   uint32_t importance_delivery_result)
{
	print_base(buffer, globals, event.timestamp(), machine.thread(event.tid(), event.timestamp()), event, event_index, "importance_delivered", false);

	//
	// Importance sender
	//
	const char* sender_name = "???";
	if (const MachineProcess<SIZE>* sender = machine.process(sender_pid, event.timestamp())) {
		sender_name = sender->name();
	}

	// 0: BOOST NOT APPLIED
	// 1: BOOST EXTERNALIZED
	// 2: LIVE_IMPORTANCE_LINKAGE!

	switch (importance_delivery_result) {
		case 0:
			buffer.printf("importance from %s (%d) was not applied\n", sender_name, sender_pid);
			break;
		case 1:
			buffer.printf("importance from %s (%d) was externalized\n", sender_name, sender_pid);
			break;
		case 2:
			buffer.printf("linked to %s (%d)'s live importance chain\n", sender_name, sender_pid);
			break;

		default:
			ASSERT(false, "Unknown importance delivery result value");
			buffer.printf("Unknown importance delivery result value\n");
			break;
	}
}

template <typename SIZE>
void print_generic(PrintBuffer& buffer,
		   const Globals& globals,
		   const Machine<SIZE>& machine,
		   const KDEvent<SIZE>& event,
		   uintptr_t event_index,
		   const char* type)
{
	print_base(buffer, globals, event.timestamp(), machine.thread(event.tid(), event.timestamp()), event, event_index, type, true);
}

template <typename SIZE>
void print_importance_assert(PrintBuffer& buffer,
			     const Globals& globals,
			     const Machine<SIZE>& machine,
			     const KDEvent<SIZE>& event,
			     uintptr_t event_index,
			     const char* type,
			     std::unordered_map<pid_t, std::pair<uint32_t, uint32_t>>& task_importance)
{
	// All callers must have the following trace event data:
	//
	// ignored, target_pid, internal_count, external_count

	// First check if anything changed
	pid_t target_pid = (pid_t)event.arg2();
	if (target_pid < 1)
		return;

	bool must_print = false;
	auto it = task_importance.find(target_pid);
	if (it == task_importance.end()) {
		it = task_importance.emplace(target_pid, std::pair<uint32_t, uint32_t>(0, 0)).first;
		// The very first time we see data for an app, we always want to print it.
		must_print = true;
	}

	auto old_importance = it->second;
	auto new_importance = std::pair<uint32_t, uint32_t>((uint32_t)event.arg3(), (uint32_t)event.arg4());
	if (must_print || old_importance != new_importance) {
		const MachineThread<SIZE>* event_thread = machine.thread(event.tid(), event.timestamp());
		print_base(buffer, globals, event.timestamp(), event_thread, event, event_index, type, false);

		const MachineProcess<SIZE>* target = machine.process(target_pid, event.timestamp());
		const char* target_name;

		if (target) {
			target_name = target->name();
		} else {
			target_name = "???";
		}

		int internal_delta = new_importance.first - old_importance.first;
		int external_delta = new_importance.second - old_importance.second;

		char internal_sign = internal_delta >= 0 ? '+' : '-';
		char external_sign = external_delta >= 0 ? '+' : '-';

		char internal_changed_buf[32];
		char external_changed_buf[32];

		if (internal_delta != 0) {
			snprintf(internal_changed_buf, sizeof(internal_changed_buf), " (%c%u)", internal_sign, abs(internal_delta));
		} else {
			internal_changed_buf[0] = 0;
		}

		if (external_delta != 0) {
			snprintf(external_changed_buf, sizeof(external_changed_buf), " (%c%u)", external_sign, abs(external_delta));
		} else {
			external_changed_buf[0] = 0;
		}

		buffer.printf("%s (%d) internal: %u%s external: %u%s\n",
				target_name, target_pid,
				new_importance.first, internal_changed_buf,
				new_importance.second, external_changed_buf);

		it->second = new_importance;
	}
}

template <typename SIZE>
void print_watchport_importance_transfer(PrintBuffer& buffer,
					 const Globals& globals,
					 const Machine<SIZE>& machine,
					 const KDEvent<SIZE>& event,
					 uintptr_t event_index)
{
	// event data is
	//
	// proc_selfpid(), pid, boost, released_pid, 0);

	// Did any importance transfer?
	if (event.arg3() == 0)
		return;

	// Do we have a valid pid?
	pid_t dest_pid = (pid_t)event.arg2();
	if (dest_pid < 1)
		return;

	const MachineThread<SIZE>* event_thread = machine.thread(event.tid(), event.timestamp());
	print_base(buffer, globals, event.timestamp(), event_thread, event, event_index, "importance_watchport", false);

	const char* dest_name;
	if (const MachineProcess<SIZE>* dest = machine.process(dest_pid, event.timestamp())) {
		dest_name = dest->name();
	} else {
		dest_name = "???";
	}

	buffer.printf("%s (%d) receives %d importance via watchport\n",
		      dest_name, dest_pid, (int)event.arg3());
}

template <typename SIZE>
void print_importance_send_failed(PrintBuffer& buffer,
				  const Globals& globals,
				  const Machine<SIZE>& machine,
				  const KDEvent<SIZE>& event,
				  uintptr_t event_index)
{
	print_base(buffer, globals, event.timestamp(), machine.thread(event.tid(), event.timestamp()), event, event_index, "impsend", false);

	//
	// Currently, the IMP_MSG_SEND trace data is not accurate.
	//

	buffer.printf("Backed out importance (may be resent) - TIMED_OUT, NO_BUFFER, or SEND_INTERRUPTED\n");
}

#if 0

template <typename SIZE>
void print_trequested_task(PrintBuffer& buffer,
			   const Globals& globals,
			   const Machine<SIZE>& machine,
			   const KDEvent<SIZE>& event,
			   uintptr_t event_index,
			   pid_t pid,
			   struct task_requested_policy new_task_requested,
			   struct task_requested_policy original_task_requested)
{
	// Many of these events would print nothing, we want to make sure there is something to print first.

	char description[512];
	char* cursor = description;
	char* cursor_end = cursor + sizeof(description);
	uint32_t description_count = 0;

	if (new_task_requested.t_role != original_task_requested.t_role) {
		const char* role = "???";
		switch (new_task_requested.t_role) {
				// This is seen when apps are terminating
			case TASK_UNSPECIFIED:
				role = "unspecified";
				break;

			case TASK_FOREGROUND_APPLICATION:
				role = "foreground";
				break;

			case TASK_BACKGROUND_APPLICATION:
				role = "background";
				break;

			case TASK_CONTROL_APPLICATION:
				role = "control-application";
				break;

			case TASK_GRAPHICS_SERVER:
				role = "graphics-server";
				break;

			case TASK_THROTTLE_APPLICATION:
				role = "throttle-application";
				break;

			case TASK_NONUI_APPLICATION:
				role = "nonui-application";
				break;

			case TASK_DEFAULT_APPLICATION:
				role = "default-application";
				break;

			default:
				ASSERT(false, "Unexpected app role");
				break;
		}
		cursor += snprintf(cursor, cursor_end - cursor, "%sROLE:%s", description_count++ == 0 ? "" : ", ", role);
		GUARANTEE(cursor < cursor_end);
	}

	if (new_task_requested.int_darwinbg != original_task_requested.int_darwinbg) {
		cursor += snprintf(cursor, cursor_end - cursor, "%s%sINT_DARWINBG", description_count++ == 0 ? "" : ", ", new_task_requested.int_darwinbg ? "" : "!");
		GUARANTEE(cursor < cursor_end);
	}

	if (new_task_requested.ext_darwinbg != original_task_requested.ext_darwinbg) {
		cursor += snprintf(cursor, cursor_end - cursor, "%s%sEXT_DARWINBG", description_count++ == 0 ? "" : ", ", new_task_requested.ext_darwinbg ? "" : "!");
		GUARANTEE(cursor < cursor_end);
	}

	if (new_task_requested.t_int_gpu_deny != original_task_requested.t_int_gpu_deny) {
		cursor += snprintf(cursor, cursor_end - cursor, "%sINT_GPU_DENY", description_count++ == 0 ? "" : ", ");
		GUARANTEE(cursor < cursor_end);
	}

	if (new_task_requested.t_ext_gpu_deny != original_task_requested.t_ext_gpu_deny) {
		cursor += snprintf(cursor, cursor_end - cursor, "%sEXT_GPU_DENY", description_count++ == 0 ? "" : ", ");
		GUARANTEE(cursor < cursor_end);
	}

	if (new_task_requested.t_tal_enabled != original_task_requested.t_tal_enabled) {
		cursor += snprintf(cursor, cursor_end - cursor, "%sTAL_ENABLED", description_count++ == 0 ? "" : ", ");
		GUARANTEE(cursor < cursor_end);
	}

	if (new_task_requested.t_sfi_managed != original_task_requested.t_sfi_managed) {
		cursor += snprintf(cursor, cursor_end - cursor, "%sSFI_MANAGED", description_count++ == 0 ? "" : ", ");
		GUARANTEE(cursor < cursor_end);
	}

	if (new_task_requested.t_sup_active != original_task_requested.t_sup_active) {
		cursor += snprintf(cursor, cursor_end - cursor, "%sAPPNAP", description_count++ == 0 ? "" : ", ");
		GUARANTEE(cursor < cursor_end);
	}

	if (new_task_requested.t_base_latency_qos != original_task_requested.t_base_latency_qos) {
		cursor += snprintf(cursor, cursor_end - cursor, "%sBASE_LATENCY_QOS:%s", description_count++ == 0 ? "" : ", ", qos_to_string(new_task_requested.t_base_latency_qos));
		GUARANTEE(cursor < cursor_end);
	}

	if (new_task_requested.t_over_latency_qos != original_task_requested.t_over_latency_qos) {
		cursor += snprintf(cursor, cursor_end - cursor, "%sOVERRIDE_LATENCY_QOS:%s", description_count++ == 0 ? "" : ", ", qos_to_string(new_task_requested.t_over_latency_qos));
		GUARANTEE(cursor < cursor_end);
	}

	if (new_task_requested.t_base_through_qos != original_task_requested.t_base_through_qos) {
		cursor += snprintf(cursor, cursor_end - cursor, "%sBASE_THROUGHPUT_QOS:%s", description_count++ == 0 ? "" : ", ", qos_to_string(new_task_requested.t_base_through_qos));
		GUARANTEE(cursor < cursor_end);
	}

	if (new_task_requested.t_over_through_qos != original_task_requested.t_over_through_qos) {
		cursor += snprintf(cursor, cursor_end - cursor, "%sOVERRIDE_THROUGHPUT_QOS:%s", description_count++ == 0 ? "" : ", ", qos_to_string(new_task_requested.t_over_through_qos));
		GUARANTEE(cursor < cursor_end);
	}

	if (new_task_requested.t_qos_clamp != original_task_requested.t_qos_clamp) {
		cursor += snprintf(cursor, cursor_end - cursor, "%sQOS_CLAMP:%s", description_count++ == 0 ? "" : ", ", qos_to_string(new_task_requested.t_qos_clamp));
		GUARANTEE(cursor < cursor_end);
	}

	if (description_count) {
		print_base(buffer, globals, event.timestamp(), machine.thread(event.tid(), event.timestamp()), event_index, "task_trequested", false);

		ASSERT(pid != -1, "Sanity");

		const char* target_name;
		if (const MachineProcess<SIZE>* target = machine.process(pid, event.timestamp())) {
			target_name = target->name();
		} else {
			target_name = "???";
		}

		buffer.printf("%s (%d) requests %s\n", target_name, pid, description);
	}
}

struct task_requested_policy {
	/* Task and thread policy (inherited) */
	uint64_t        int_darwinbg        :1,     /* marked as darwinbg via setpriority */
	ext_darwinbg        :1,
	int_iotier          :2,     /* IO throttle tier */
	ext_iotier          :2,
	int_iopassive       :1,     /* should IOs cause lower tiers to be throttled */
	ext_iopassive       :1,
	bg_iotier           :2,     /* what IO throttle tier should apply to me when I'm darwinbg? (pushed to threads) */
	terminated          :1,     /* all throttles should be removed for quick exit or SIGTERM handling */

	/* Thread only policy */
	th_pidbind_bg       :1,     /* thread only: task i'm bound to is marked 'watchbg' */
	th_workq_bg         :1,     /* thread only: currently running a background priority workqueue */
	thrp_qos            :3,     /* thread only: thread qos class */
	thrp_qos_relprio    :4,     /* thread only: thread qos relative priority (store as inverse, -10 -> 0xA) */
	thrp_qos_override   :3,     /* thread only: thread qos class override */

	/* Task only policy */
	t_apptype           :3,     /* What apptype did launchd tell us this was (inherited) */
	t_boosted           :1,     /* Has a non-zero importance assertion count */
	t_int_gpu_deny      :1,     /* don't allow access to GPU */
	t_ext_gpu_deny      :1,
	t_role              :3,     /* task's system role */
	t_tal_enabled       :1,     /* TAL mode is enabled */
	t_base_latency_qos  :3,     /* Timer latency QoS */
	t_over_latency_qos  :3,     /* Timer latency QoS override */
	t_base_through_qos  :3,     /* Computation throughput QoS */
	t_over_through_qos  :3,     /* Computation throughput QoS override */
	t_sfi_managed       :1,     /* SFI Managed task */
	t_qos_clamp         :3,     /* task qos clamp */

	/* Task only: suppression policies (non-embedded only) */
	t_sup_active        :1,     /* Suppression is on */
	t_sup_lowpri_cpu    :1,     /* Wants low priority CPU (MAXPRI_THROTTLE) */
	t_sup_timer         :3,     /* Wanted timer throttling QoS tier */
	t_sup_disk          :1,     /* Wants disk throttling */
	t_sup_cpu_limit     :1,     /* Wants CPU limit (not hooked up yet)*/
	t_sup_suspend       :1,     /* Wants to be suspended */
	t_sup_throughput    :3,     /* Wants throughput QoS tier */
	t_sup_cpu           :1,     /* Wants suppressed CPU priority (MAXPRI_SUPPRESSED) */
	t_sup_bg_sockets    :1,     /* Wants background sockets */

	reserved            :2;
};
#endif

template <typename SIZE>
void print_appnap(PrintBuffer& buffer,
		  const Globals& globals,
		  const Machine<SIZE>& machine,
		  const KDEvent<SIZE>& event,
		  uintptr_t event_index,
		  bool is_appnap_active,
		  std::unordered_map<pid_t, bool>& task_appnap_state,
		  std::unordered_map<pid_t, TaskRequestedPolicy>& task_requested_state)
{
	//
	// event args are:
	//
	// self_pid, audit_token_pid_from_task(task), trequested_0(task, NULL), trequested_1(task, NULL)
	//
	auto pid = (pid_t)event.arg2();
	auto trequested_0 = event.arg3();
	auto trequested_1 = event.arg4();
	auto task_requested = (SIZE::is_64_bit) ? TaskRequestedPolicy(trequested_0) : TaskRequestedPolicy((Kernel32::ptr_t)trequested_0, (Kernel32::ptr_t)trequested_1);
	auto should_print = false;

	ASSERT(pid != -1, "Sanity");

	// If the appnap state changed, we want to print this event.
	auto appnap_it = task_appnap_state.find(pid);
	if (appnap_it == task_appnap_state.end()) {
		should_print = true;
		task_appnap_state.emplace(pid, is_appnap_active);
	} else {
		if (appnap_it->second != is_appnap_active) {
			should_print = true;
			appnap_it->second = is_appnap_active;
		}
	}

	// If the task_requested state changed, we want to print this event.
	auto requested_it = task_requested_state.find(pid);
	if (requested_it == task_requested_state.end()) {
		should_print = true;
		task_requested_state.emplace(pid, task_requested);
	} else {
		if (requested_it->second != task_requested) {
			should_print = true;
			requested_it->second = task_requested;
		}
	}

	if (should_print) {
		print_base(buffer, globals, event.timestamp(), machine.thread(event.tid(), event.timestamp()), event, event_index, "imp_supression", false);

		const char* name;
		if (auto target = machine.process(pid, event.timestamp())) {
			name = target->name();
		} else {
			name = "???";
		}
		buffer.printf("%s (%d) AppNap is %s\n", name, pid, is_appnap_active ? "ON" : "OFF");
		print_trequested_task(buffer, globals, machine, event, event_index, pid, task_requested);
	}
}

template <typename SIZE>
void print_trequested_task(PrintBuffer& buffer,
			   const Globals& globals,
			   const Machine<SIZE>& machine,
			   const KDEvent<SIZE>& event,
			   uintptr_t event_index,
			   pid_t pid,
			   TaskRequestedPolicy task_requested)
{

	ASSERT(pid != -1, "Sanity");
	const char* target_name;
	if (const MachineProcess<SIZE>* target = machine.process(pid, event.timestamp())) {
		target_name = target->name();
	} else {
		target_name = "???";
	}

	struct task_requested_policy trp = task_requested.as_struct();

	print_base_empty(buffer, globals, event_index, "task_trequested", false);
	buffer.printf("%s (%d) requests%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n",
		      target_name, pid,
		      trp.int_darwinbg       ? " IntDBG" : "",
		      trp.ext_darwinbg       ? " ExtDBG" : "",
		      trp.int_iopassive      ? " IntIOPass" : "",
		      trp.ext_iopassive      ? " ExtIOPass" : "",
		      trp.terminated         ? " Term" : "",
		      trp.t_boosted          ? " Boost" : "",
		      trp.t_int_gpu_deny     ? " IntDenyGPU" : "",
		      trp.t_ext_gpu_deny     ? " ExtDenyGPU" : "",
		      trp.t_tal_enabled      ? " TAL" : "",
		      trp.t_sfi_managed      ? " SFI" : "",
		      // Below here is AppNap only...
		      trp.t_sup_active       ? " AppNap" : "",
		      trp.t_sup_lowpri_cpu   ? " SupLowPriCPU" : "",
		      trp.t_sup_disk         ? " SupDisk" : "",
		      trp.t_sup_cpu_limit    ? " SupCPULim" : "",
		      trp.t_sup_suspend      ? " SupSusp" : "",
		      trp.t_sup_cpu          ? " SupCPU" : "",
		      trp.t_sup_bg_sockets   ? " SupBGSck" : "");

	print_base_empty(buffer, globals, event_index, "task_trequested", false);
	buffer.printf("%s (%d) requests QOS (SupTHR/SupTMR/LAT/OVERLAT/THR/OVERTHR/CLAMP) %s/%s/%s/%s/%s/%s/%s int_IOTier:%d ext_IOTier:%d bg_IOTier:%d\n",
		      target_name, pid,
		      qos_to_string(trp.t_sup_throughput),
		      qos_to_string(trp.t_sup_timer),
		      qos_to_string(trp.t_base_latency_qos),
		      qos_to_string(trp.t_over_latency_qos),
		      qos_to_string(trp.t_base_through_qos),
		      qos_to_string(trp.t_over_through_qos),
		      qos_to_string(trp.t_qos_clamp),
		      trp.int_iotier,
		      trp.ext_iotier,
		      trp.bg_iotier);
}

template <typename SIZE>
void print_trequested_thread(PrintBuffer& buffer,
			     const Globals& globals,
			     const Machine<SIZE>& machine,
			     const KDEvent<SIZE>& event,
			     uintptr_t event_index,
			     const MachineThread<SIZE>* thread,
			     struct task_requested_policy new_thread_requested,
			     struct task_requested_policy original_thread_requested)
{
	ASSERT(thread, "Sanity");

	// Many of these events would print nothing, we want to make sure there is something to print first.

	char description[512];
	char* cursor = description;
	char* cursor_end = cursor + sizeof(description);
	uint32_t description_count = 0;

	if (new_thread_requested.int_darwinbg != original_thread_requested.int_darwinbg) {
		cursor += snprintf(cursor, cursor_end - cursor, "%sINT_DARWINBG", description_count++ == 0 ? "" : ", ");
		GUARANTEE(cursor < cursor_end);
	}

	if (new_thread_requested.ext_darwinbg != original_thread_requested.ext_darwinbg) {
		cursor += snprintf(cursor, cursor_end - cursor, "%sEXT_DARWINBG", description_count++ == 0 ? "" : ", ");
		GUARANTEE(cursor < cursor_end);
	}

	if (new_thread_requested.th_pidbind_bg != original_thread_requested.th_pidbind_bg) {
		cursor += snprintf(cursor, cursor_end - cursor, "%sPIDBIND_BG", description_count++ == 0 ? "" : ", ");
		GUARANTEE(cursor < cursor_end);
	}

	if (new_thread_requested.th_workq_bg != original_thread_requested.th_workq_bg) {
		cursor += snprintf(cursor, cursor_end - cursor, "%sWORKQ_BG", description_count++ == 0 ? "" : ", ");
		GUARANTEE(cursor < cursor_end);
	}

	if (new_thread_requested.thrp_qos != original_thread_requested.thrp_qos) {
		cursor += snprintf(cursor, cursor_end - cursor, "%sTHREAD_QOS:%s", description_count++ == 0 ? "" : ", ", qos_to_string(new_thread_requested.thrp_qos));
		GUARANTEE(cursor < cursor_end);
	}

	if (new_thread_requested.thrp_qos_relprio != original_thread_requested.thrp_qos_relprio) {
		cursor += snprintf(cursor, cursor_end - cursor, "%sTHREAD_QOS_RELATIVE_PRIORITY:%d", description_count++ == 0 ? "" : ", ", -new_thread_requested.thrp_qos_relprio);
		GUARANTEE(cursor < cursor_end);
	}

	if (new_thread_requested.thrp_qos_override != original_thread_requested.thrp_qos_override) {
		cursor += snprintf(cursor, cursor_end - cursor, "%sTHREAD_OVERRIDE_QOS:%s", description_count++ == 0 ? "" : ", ", qos_to_string(new_thread_requested.thrp_qos_override));
		GUARANTEE(cursor < cursor_end);
	}

	if (description_count) {
		print_base(buffer, globals, event.timestamp(), machine.thread(event.tid(), event.timestamp()), event_index, "thread_trequested", false);
		ASSERT(thread->process().pid() != -1, "Sanity");
		buffer.printf("%s (%d) %llX requests %s\n", thread->process().name(), thread->process().pid(), (uint64_t)thread->tid(), description);
	}
}

template <typename SIZE>
void print_teffective_task(PrintBuffer& buffer,
			   const Globals& globals,
			   const Machine<SIZE>& machine,
			   const KDEvent<SIZE>& event,
			   uintptr_t event_index,
			   pid_t pid,
			   TaskEffectivePolicy task_effective)
{
	ASSERT(pid != -1, "Sanity");
	const char* target_name;
	if (const MachineProcess<SIZE>* target = machine.process(pid, event.timestamp())) {
		target_name = target->name();
	} else {
		target_name = "???";
	}

	struct task_effective_policy tep = task_effective.as_struct();

	print_base_empty(buffer, globals, event_index, "task_teffective", false);
	buffer.printf("%s (%d) is%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n",
		      target_name, pid,
		      tep.darwinbg ? " DarwinBG" : "",
		      tep.t_sup_active ? " AppNap" : "",
		      tep.lowpri_cpu ? " LowPri" : "",
		      tep.io_passive ? " IOPass" : "",
		      tep.all_sockets_bg ? " ASckBG" : "",
		      tep.new_sockets_bg ? " NSckBG" : "",
		      tep.terminated ? " Term" : "",
		      tep.qos_ui_is_urgent ? " QOSUiIsUrg" : "",
		      tep.t_gpu_deny ? " GPUDeny" : "",
		      tep.t_suspended ? " SupSusp" : "",
		      tep.t_watchers_bg ? " WchrsBG" : "",
		      tep.t_suppressed_cpu ? " SupCPU" : "",
		      tep.t_sfi_managed ? " SFI" : "",
		      tep.t_live_donor ? " LiveImpDnr" : "");

	print_base_empty(buffer, globals, event_index, "task_teffective", false);
	buffer.printf("%s (%d) is Role:%s LAT/THR/CLAMP/CEIL:%s/%s/%s/%s IOTier:%d BG_IOTier:%d\n",
		      target_name, pid,
		      role_to_string(tep.t_role),
		      qos_to_string(tep.t_latency_qos),
		      qos_to_string(tep.t_through_qos),
		      qos_to_string(tep.t_qos_clamp),
		      qos_to_string(tep.t_qos_ceiling),
		      tep.io_tier,
		      tep.bg_iotier);
}

template <typename SIZE>
void print_teffective_thread(PrintBuffer& buffer,
			     const Globals& globals,
			     const Machine<SIZE>& machine,
			     const KDEvent<SIZE>& event,
			     uintptr_t event_index,
			     const MachineThread<SIZE>* thread,
			     TaskEffectivePolicy thread_effective)
{
	ASSERT(thread, "Sanity");

	const char* target_name = thread->process().name();

	struct task_effective_policy tep = thread_effective.as_struct();

	print_base_empty(buffer, globals, event_index, "thread_teffective", false);
	buffer.printf("%s (%d) %llX is%s%s%s%s%s%s%s%s\n",
		      target_name, thread->process().pid(), (uint64_t)thread->tid(),
		      tep.darwinbg ? " DarwinBG" : "",
		      tep.t_sup_active ? " AppNap" : "",
		      tep.lowpri_cpu ? " LowPri" : "",
		      tep.io_passive ? " IOPass" : "",
		      tep.all_sockets_bg ? " ASckBG" : "",
		      tep.new_sockets_bg ? " NSckBG" : "",
		      tep.terminated ? " Term" : "",
		      tep.qos_ui_is_urgent ? " QOSUiIsUrg" : "");

	print_base_empty(buffer, globals, event_index, "thread_teffective", false);
	buffer.printf("%s (%d) %llX is QOS:%s QOS_relprio:%d\n",
		      target_name, thread->process().pid(), (uint64_t)thread->tid(),
		      qos_to_string(tep.thep_qos),
		      tep.thep_qos_relprio);
}

template <typename SIZE>
void print_importance_apptype(PrintBuffer& buffer,
			      const Globals& globals,
			      const Machine<SIZE>& machine,
			      const KDEvent<SIZE>& event,
			      uintptr_t event_index)
{
	print_base(buffer, globals, event.timestamp(), machine.thread(event.tid(), event.timestamp()), event, event_index, "set_apptype", false);

	//
	// trace args are:
	//
	// selfpid, targetpid, trequested(targetpid, NULL), is_importance_receiver

        //
        // QoS clamp
        //
	// Can only be determined on K64, the bits needed are trimmed off in
	// K32 tracepoints.

	char qos_clamp[32];
	qos_clamp[0] = 0;
	if (SIZE::is_64_bit) {
		uint32_t qos_level = (event.arg3() & POLICY_REQ_QOS_CLAMP_MASK) >> POLICY_REQ_QOS_CLAMP_SHIFT;
		if (qos_level != THREAD_QOS_UNSPECIFIED) {
			snprintf(qos_clamp, sizeof(qos_clamp), ", clamped to %s", qos_to_string(qos_level));
		}
	}

	pid_t target_pid = (pid_t)event.arg2();
	const char* target_name;

	if (target_pid != -1 ) {
		if (const MachineProcess<SIZE>* target = machine.process(target_pid, event.timestamp())) {
			target_name = target->name();
		} else {
			target_name = "???";
		}
	} else {
		target_name = "NULL-Task";
	}

	const char* apptype = "???";
	switch (event.dbg_code()) {
		case TASK_APPTYPE_NONE:
			apptype = "None";
			break;
		case TASK_APPTYPE_DAEMON_INTERACTIVE:
			apptype = "InteractiveDaemon";
			break;
		case TASK_APPTYPE_DAEMON_STANDARD:
			apptype = "StandardDaemon";
			break;
		case TASK_APPTYPE_DAEMON_ADAPTIVE:
			apptype = "AdaptiveDaemon";
			break;
		case TASK_APPTYPE_DAEMON_BACKGROUND:
			apptype = "BackgroundDaemon";
			break;
		case TASK_APPTYPE_APP_DEFAULT:
			apptype = "App";
			break;
		case TASK_APPTYPE_APP_TAL:
			apptype = "TALApp";
			break;
		default:
			break;
	}

	const char* imp_recv = "";
	if (event.arg4()) {
		imp_recv = ", receives importance";
	}
        buffer.printf("Set %s (%d) to %s%s%s\n", target_name, target_pid, apptype, imp_recv, qos_clamp);
}

template <typename SIZE>
void print_importance_update_task(PrintBuffer& buffer,
				  const Globals& globals,
				  const Machine<SIZE>& machine,
				  const KDEvent<SIZE>& event,
				  uintptr_t event_index,
				  const char* type,
				  std::unordered_map<pid_t, std::pair<TaskEffectivePolicy, uint32_t>>& task_effective_state)
{
	//
	// event args are:
	//
	// targetpid, teffective_0(task, NULL), teffective_1(task, NULL), tpriority(task, THREAD_NULL)
	//
	auto pid = (pid_t)event.arg1();
	auto teffective_0 = event.arg2();
	auto teffective_1 = event.arg3();
	auto priority = (uint32_t)event.arg4();
	auto task_effective_policy = (SIZE::is_64_bit) ? TaskEffectivePolicy(teffective_0) : TaskEffectivePolicy((Kernel32::ptr_t)teffective_0, (Kernel32::ptr_t)teffective_1);
	auto state = std::pair<TaskEffectivePolicy, uint32_t>(task_effective_policy, priority);
	auto should_print = false;

	ASSERT(pid != -1, "Sanity");

	// Verify that some state changed before printing.
	auto it = task_effective_state.find(pid);
	if (it == task_effective_state.end()) {
		should_print = true;
		task_effective_state.emplace(pid, state);
	} else {
		if (it->second != state) {
			should_print = true;
			it->second = state;
		}
	}

	if (should_print) {
		print_base(buffer, globals, event.timestamp(), machine.thread(event.tid(), event.timestamp()), event, event_index, type, false);

		const char* name;
		if (auto target = machine.process(pid, event.timestamp())) {
			name = target->name();
		} else {
			name = "???";
		}

		buffer.printf("%s (%d) base priority is %d\n", name, pid, priority);

		print_teffective_task(buffer, globals, machine, event, event_index, pid, task_effective_policy);
	}
}

template <typename SIZE>
void print_importance_update_thread(PrintBuffer& buffer,
				    const Globals& globals,
				    const Machine<SIZE>& machine,
				    const KDEvent<SIZE>& event,
				    uintptr_t event_index,
				    const char* type,
				    std::unordered_map<typename SIZE::ptr_t, std::pair<TaskEffectivePolicy, uint32_t>>& thread_effective_state)
{
	//
	// event args are:
	//
	// targettid, teffective_0(task, thread), teffective_1(task, thread), tpriority(task, thread)
	//

	if (const MachineThread<SIZE>* thread = machine.thread(event.arg1(), event.timestamp())) {
		auto pid = thread->process().pid();
		auto teffective_0 = event.arg2();
		auto teffective_1 = event.arg3();
		auto priority = (uint32_t)event.arg4();
		auto thread_effective_policy = (SIZE::is_64_bit) ? TaskEffectivePolicy(teffective_1) : TaskEffectivePolicy((Kernel32::ptr_t)teffective_0, (Kernel32::ptr_t)teffective_1);
		auto state = std::pair<TaskEffectivePolicy, uint32_t>(thread_effective_policy, priority);
		auto should_print = false;

		// Verify that some state changed before printing.
		auto it = thread_effective_state.find(thread->tid());
		if (it == thread_effective_state.end()) {
			should_print = true;
			thread_effective_state.emplace(thread->tid(), state);
		} else {
			if (it->second != state) {
				should_print = true;
				it->second = state;
			}
		}

		if (should_print) {
			print_base(buffer, globals, event.timestamp(), machine.thread(event.tid(), event.timestamp()), event, event_index, type, false);
			buffer.printf("%s (%d) %llX base priority is %d\n", thread->process().name(), pid, (uint64_t)thread->tid(), priority);

			print_teffective_thread(buffer, globals, machine, event, event_index, thread, thread_effective_policy);
		}
	}
}

template <typename SIZE>
void print_fork(PrintBuffer& buffer,
		const Globals& globals,
		const Machine<SIZE>& machine,
		const KDEvent<SIZE>& event,
		uintptr_t event_index,
		const MachineProcess<SIZE>& child_process)
{
	print_base(buffer, globals, event.timestamp(), machine.thread(event.tid(), event.timestamp()), event, event_index, "fork", false);

	//
	// Other process
	//

	buffer.printf("Create %s (%d)\n", child_process.name(), child_process.pid());
}

template <typename SIZE>
void print_exit(PrintBuffer& buffer,
		const Globals& globals,
		const KDEvent<SIZE>& event,
		const MachineThread<SIZE>* thread,
		uintptr_t event_index)
{
	ASSERT(thread, "Sanity");

	print_base(buffer, globals, event.timestamp(), thread, event, event_index, "exit", false);

	//
	// exit code
	//

        int exit_status = thread->process().exit_status();

	if (WIFEXITED(exit_status)) {
		buffer.printf("returned %d\n", WEXITSTATUS(exit_status));
	} else if (WIFSIGNALED(exit_status)) {
		buffer.printf("SIGNAL: %s\n", strsignal(WTERMSIG(exit_status)));
	} else {
		buffer.printf("Unhandled exit status %x\n", (uint32_t)exit_status);
	}
}

template <typename SIZE>
void print_voucher(PrintBuffer& buffer,
		   const Globals& globals,
		   const Machine<SIZE>& machine,
		   const KDEvent<SIZE>& event,
		   uintptr_t event_index,
		   const char* type,
		   const MachineVoucher<SIZE>* voucher,
		   bool is_create)
{
	print_base(buffer, globals, event.timestamp(), machine.thread(event.tid(), event.timestamp()), event, event_index, type, false);

	//
	// Calculate lifetime
	//

	char lifetime[32];
	AbsInterval timespan = voucher->timespan();

	//
	// Voucher created before the trace starts will have a starting time
	// of 0; Vouchers that are still alive will have a max of UINT64_MAX.
	//
	if (timespan.location() == AbsTime(0) || timespan.max() == AbsTime(UINT64_MAX)) {
		snprintf(lifetime, sizeof(lifetime), "???");
	} else {
		NanoTime t1 = timespan.length().nano_time(globals.timebase());
		snprintf(lifetime, sizeof(lifetime), "%0.2f", (double)t1.value() / NANOSECONDS_PER_MICROSECOND);
	}


	//
	// Voucher addr
	//
	if (is_create) {
		buffer.printf("Create voucher-%u @ %llX, lifetime will be %s µs, now %u vouchers\n", voucher->id(), (uint64_t)voucher->address(), lifetime, (uint32_t)event.arg3());
	} else {
		buffer.printf("Destroy voucher-%u @ %llX, lifetime was %s µs, now %u vouchers\n", voucher->id(), (uint64_t)voucher->address(), lifetime, (uint32_t)event.arg3());
	}
}

template <typename SIZE>
void print_voucher_contents(PrintBuffer& buffer,
			    const Globals& globals,
			    const Machine<SIZE>& machine,
			    const KDEvent<SIZE>& event,
			    uintptr_t event_index,
			    const MachineVoucher<SIZE>* voucher)
{
	const uint8_t* bytes = voucher->content_bytes();
	uint32_t bytes_required = voucher->content_size();

	ASSERT(bytes_required, "Printing empty voucher");

	unsigned int used_size = 0;
	mach_voucher_attr_recipe_t recipe = NULL;
	while (bytes_required > used_size) {
		recipe = (mach_voucher_attr_recipe_t)&bytes[used_size];

		switch (recipe->key) {
			case MACH_VOUCHER_ATTR_KEY_NONE:
				ASSERT(false, "No key in recipe");
				break;

			case MACH_VOUCHER_ATTR_KEY_ATM:
				print_base_empty(buffer, globals, event_index, "voucher_create", false);
				buffer.printf("       voucher-%u | ATM ID %llu\n", voucher->id(), *(uint64_t *)(uintptr_t)recipe->content);
				break;

			case MACH_VOUCHER_ATTR_KEY_IMPORTANCE:
				print_base_empty(buffer, globals, event_index, "voucher_create", false);
				buffer.printf("       voucher-%u | %s\n", voucher->id(), (char *)recipe->content);
				break;

			case MACH_VOUCHER_ATTR_KEY_BANK:
				// Spacing and newline is different because that is how BANK formats it :-(
				print_base_empty(buffer, globals, event_index, "voucher_create", false);
				buffer.printf("       voucher-%u |%s", voucher->id(), (char *)recipe->content);
				break;

			case MACH_VOUCHER_ATTR_KEY_USER_DATA:
				for (uint32_t offset=0; offset<recipe->content_size; offset += 16) {
					uint8_t* data = ((uint8_t*)recipe->content) + offset;
					size_t data_remaining = std::min(recipe->content_size - offset, (uint32_t)16);

					print_base_empty(buffer, globals, event_index, "voucher_create", false);
					buffer.printf("       voucher-%u | UserData: %04u  ", voucher->id(), offset);

					// 16 * 3 == 48, 16 chars to spare
					char hex_buffer[64];
					// Hex data.
					for (uint32_t cursor = 0; cursor<data_remaining; cursor++) {
						char* hex_buffer_tmp = &hex_buffer[cursor * 3];
						size_t hex_buffer_tmp_size = sizeof(hex_buffer) - cursor * 3;
						snprintf(hex_buffer_tmp, hex_buffer_tmp_size, "%02x ", data[cursor]);
					}

					char ascii_buffer[24];
					for (uint32_t cursor = 0; cursor<data_remaining; cursor++) {
						if (isprint(data[cursor]))
							ascii_buffer[cursor] = data[cursor];
						else
							ascii_buffer[cursor] = '.';
					}
					ascii_buffer[data_remaining] = 0;

					buffer.printf("%-48s  %-16s\n", hex_buffer, ascii_buffer);
				}
				break;

			default:
				print_base_empty(buffer, globals, event_index, "voucher_create", false);
				buffer.printf("       voucher-%u | UNKNOWN key-%u command-%u size-%u\n", voucher->id(), recipe->key, recipe->command, recipe->content_size);
				break;
		}

		used_size += sizeof(mach_voucher_attr_recipe_data_t) + recipe->content_size;
	}
}

#endif /* defined(__msa__MessagePrinting__) */
