#!/usr/local/bin/luatrace -s

trace_codename = function(codename, callback)
	local debugid = trace.debugid(codename)
	if debugid ~= 0 then
		trace.single(debugid,callback)
	else
		printf("WARNING: Cannot locate debugid for '%s'\n", codename)
	end
end

initial_timestamp = 0
workqueue_ptr_map = {};
get_prefix = function(buf)
	if initial_timestamp == 0 then
		initial_timestamp = buf.timestamp
	end
	local secs = trace.convert_timestamp_to_nanoseconds(buf.timestamp - initial_timestamp) / 1000000000

	local prefix
	if trace.debugid_is_start(buf.debugid) then
		prefix = "→"
	elseif trace.debugid_is_end(buf.debugid) then
		prefix = "←"
	else
		prefix = "↔"
	end

	local proc
	if buf.command ~= "kernel_task" then
		proc = buf.command
		workqueue_ptr_map[buf[1]] = buf.command
	elseif workqueue_ptr_map[buf[1]] ~= nil then
		proc = workqueue_ptr_map[buf[1]]
	else
		proc = "UNKNOWN"
	end

	return string.format("%s %6.9f %-17s [%05d.%06x] %-24s",
		prefix, secs, proc, buf.pid, buf.threadid, buf.debugname)
end

parse_pthread_priority = function(pri)
	pri = pri & 0xffffffff
	if (pri & 0x02000000) == 0x02000000 then
		return "Manager"
	end
	local qos = (pri & 0x00ffff00) >> 8
	if qos == 0x20 then
		return string.format("UI[%x]", pri);
	elseif qos == 0x10 then
		return string.format("IN[%x]", pri);
	elseif qos == 0x08 then
		return string.format("DF[%x]", pri);
	elseif qos == 0x04 then
		return string.format("UT[%x]", pri);
	elseif qos == 0x02 then
		return string.format("BG[%x]", pri);
	elseif qos == 0x01 then
		return string.format("MT[%x]", pri);
	elseif qos == 0x00 then
		return string.format("--[%x]", pri);
	else
		return string.format("??[%x]", pri);
	end
end

parse_qos_bucket = function(pri)
	if pri == 0 then
		return string.format("UI[%x]", pri);
	elseif pri == 1 then
		return string.format("IN[%x]", pri);
	elseif pri == 2 then
		return string.format("DF[%x]", pri);
	elseif pri == 3 then
		return string.format("UT[%x]", pri);
	elseif pri == 4 then
		return string.format("BG[%x]", pri);
	elseif pri == 5 then
		return string.format("MT[%x]", pri);
	elseif pri == 6 then
		return string.format("MG[%x]", pri);
	else
		return string.format("??[%x]", pri);
	end
end

parse_thactive_req_bucket = function(pri)
    if pri ~= 6 then
        return parse_qos_bucket(pri)
    end
    return "None"
end

get_thactive = function(low, high)
    return string.format("req: %s, MG: %d, UI: %d, IN: %d, DE: %d, UT: %d, BG: %d, MT: %d",
           parse_thactive_req_bucket(high >> (16 * 3)), (high >> (2 * 16)) & 0xffff,
           (low  >> (0 * 16)) & 0xffff, (low  >> (1 * 16)) & 0xffff,
           (low  >> (2 * 16)) & 0xffff, (low  >> (3 * 16)) & 0xffff,
           (high >> (0 * 16)) & 0xffff, (high >> (1 * 16)) & 0xffff)
end

-- workqueue lifecycle

trace_codename("wq_pthread_exit", function(buf)
	local prefix = get_prefix(buf)
	if trace.debugid_is_start(buf.debugid) then
		printf("%s\tprocess is exiting\n",prefix)
	else
		printf("%s\tworkqueue marked as exiting and timer is complete\n",prefix)
	end
end)

trace_codename("wq_workqueue_exit", function(buf)
	local prefix = get_prefix(buf)
	if trace.debugid_is_start(buf.debugid) then
		printf("%s\tall threads have exited, cleaning up\n",prefix)
	else
		printf("%s\tclean up complete\n",prefix)
	end
end)

trace_codename("wq_start_add_timer", function(buf)
	local prefix = get_prefix(buf)
	printf("%s\tarming timer to fire in %d us (flags: %x, reqcount: %d)\n",
		prefix, buf.arg4, buf.arg3, buf.arg2)
end)

trace_codename("wq_add_timer", function(buf)
	local prefix = get_prefix(buf)
	if trace.debugid_is_start(buf.debugid) then
		printf("%s\tadd_timer fired (flags: %x, nthreads: %d, thidlecount: %d)\n",
			prefix, buf.arg2, buf.arg3, buf.arg4)
	elseif trace.debugid_is_end(buf.debugid) then
		printf("%s\tadd_timer completed (start_timer: %x, nthreads: %d, thidlecount: %d)\n",
			prefix, buf.arg2, buf.arg3, buf.arg4)
	else
		printf("%s\tadd_timer added threads (reqcount: %d, thidlecount: %d, busycount: %d)\n",
			prefix, buf.arg2, buf.arg3, buf.arg4)

	end
end)

trace_codename("wq_run_threadreq", function(buf)
	local prefix = get_prefix(buf)
	if trace.debugid_is_start(buf.debugid) then
		if buf[2] > 0 then
			printf("%s\trun_threadreq: %x (priority: %s, flags: %d) on %x\n",
					prefix, buf[2], parse_qos_bucket(buf[4] >> 16), buf[4] & 0xff, buf[3])
		else
			printf("%s\trun_threadreq: <none> on %x\n",
					prefix, buf[3])
		end
	else
		if buf[2] == 1 then
			printf("%s\tpended event manager, already running\n", prefix)
		elseif buf[2] == 2 then
			printf("%s\tnothing to do\n", prefix)
		elseif buf[2] == 3 then
			printf("%s\tno eligible request found\n", prefix)
		elseif buf[2] == 4 then
			printf("%s\tadmission control failed\n", prefix)
		elseif buf[2] == 5 then
			printf("%s\tunable to add new thread (may_add_new_thread: %d, nthreads: %d)\n", prefix, buf[3], buf[4])
		elseif buf[2] == 6 then
			printf("%s\tthread creation failed\n", prefix)
		elseif buf[2] == 0 then
			printf("%s\tsuccess\n", prefix)
		else
			printf("%s\tWARNING: UNKNOWN END CODE:%d\n", prefix, buf.arg4)
		end
	end
end)

trace_codename("wq_run_threadreq_mgr_merge", function(buf)
	local prefix = get_prefix(buf)
	printf("%s\t\tmerging incoming manager request into existing\n", prefix)
end)

trace_codename("wq_run_threadreq_req_select", function(buf)
	local prefix = get_prefix(buf)
	if buf[3] == 1 then
		printf("%s\t\tselected event manager request %x\n", prefix, buf[2])
	elseif buf[3] == 2 then
		printf("%s\t\tselected overcommit request %x\n", prefix, buf[2])
	elseif buf[3] == 3 then
		printf("%s\t\tselected constrained request %x\n", prefix, buf[2])
	else
		printf("%s\t\tWARNING: UNKNOWN DECISION CODE:%d\n", prefix, buf.arg[3])
	end
end)

trace_codename("wq_run_threadreq_thread_select", function(buf)
	local prefix = get_prefix(buf)
	if buf[2] == 1 then
		printf("%s\t\trunning on current thread %x\n", prefix, buf[3])
	elseif buf[2] == 2 then
		printf("%s\t\trunning on idle thread %x\n", prefix, buf[3])
	elseif buf[2] == 3 then
		printf("%s\t\tcreated new thread\n", prefix)
	else
		printf("%s\t\tWARNING: UNKNOWN DECISION CODE:%d\n", prefix, buf.arg[2])
	end
end)

trace_codename("wq_thread_reset_priority", function(buf)
	local prefix = get_prefix(buf)
	local old_qos = buf[3] >> 16;
	local new_qos = buf[3] & 0xff;
	if buf[4] == 1 then
		printf("%s\t\treset priority of %x from %s to %s\n", prefix, buf[2], parse_qos_bucket(old_qos), parse_qos_bucket(new_qos))
	elseif buf[4] == 2 then
		printf("%s\t\treset priority of %x from %s to %s for reserve manager\n", prefix, buf[2], parse_qos_bucket(old_qos), parse_qos_bucket(new_qos))
	elseif buf[4] == 3 then
		printf("%s\t\treset priority of %x from %s to %s for cleanup\n", prefix, buf[2], parse_qos_bucket(old_qos), parse_qos_bucket(new_qos))
	end
end)

trace_codename("wq_thread_park", function(buf)
	local prefix = get_prefix(buf)
	if trace.debugid_is_start(buf.debugid) then
		printf("%s\tthread parking\n", prefix)
	else
		printf("%s\tthread woken\n", prefix)
	end
end)

trace_codename("wq_thread_squash", function(buf)
	local prefix = get_prefix(buf)
	printf("%s\tthread squashed from %s to %s\n", prefix,
			parse_qos_bucket(buf[2]), parse_qos_bucket(buf[3]))
end)

trace.enable_thread_cputime()
runitem_time_map = {}
runitem_cputime_map = {}
trace_codename("wq_runitem", function(buf)
	local prefix = get_prefix(buf)
	if trace.debugid_is_start(buf.debugid) then
		runitem_time_map[buf.threadid] = buf.timestamp;
		runitem_cputime_map[buf.threadid] = trace.cputime_for_thread(buf.threadid);

		printf("%s\tSTART running item @ %s\n", prefix, parse_qos_bucket(buf[3]))
	elseif runitem_time_map[buf.threadid] then
		local time = buf.timestamp - runitem_time_map[buf.threadid]
		local cputime = trace.cputime_for_thread(buf.threadid) - runitem_cputime_map[buf.threadid]

		local time_ms = trace.convert_timestamp_to_nanoseconds(time) / 1000000
		local cputime_ms = trace.convert_timestamp_to_nanoseconds(cputime) / 1000000

		printf("%s\tDONE running item @ %s: time = %6.6f ms, cputime = %6.6f ms\n",
				prefix, parse_qos_bucket(buf[2]), time_ms, cputime_ms)

		runitem_time_map[buf.threadid] = 0
		runitem_cputime_map[buf.threadid] = 0
	else
		printf("%s\tDONE running item @ %s\n", prefix, parse_qos_bucket(buf[2]))
	end
end)

trace_codename("wq_runthread", function(buf)
	local prefix = get_prefix(buf)
	if trace.debugid_is_start(buf.debugid) then
		printf("%s\tSTART running thread\n", prefix)
	elseif trace.debugid_is_end(buf.debugid) then
		printf("%s\tDONE running thread\n", prefix)
	end
end)

trace_codename("wq_thactive_update", function(buf)
    local prefix = get_prefix(buf)
    local thactive = get_thactive(buf[2], buf[3])
    if buf[1] == 1 then
        printf("%s\tthactive constrained pre-post (%s)\n", prefix, thactive)
    elseif buf[1] == 2 then
        printf("%s\tthactive constrained run (%s)\n", prefix, thactive)
    else
        return
    end
end)

trace_codename("wq_thread_block", function(buf)
	local prefix = get_prefix(buf)
        local req_pri = parse_thactive_req_bucket(buf[3] >> 8)
	if trace.debugid_is_start(buf.debugid) then
		printf("%s\tthread blocked (activecount: %d, priority: %s, req_pri: %s, reqcount: %d, start_timer: %d)\n",
			prefix, buf[2], parse_qos_bucket(buf[3] & 0xff), req_pri, buf[4] >> 1, buf[4] & 0x1)
	else
		printf("%s\tthread unblocked (activecount: %d, priority: %s, req_pri: %s, threads_scheduled: %d)\n",
			prefix, buf[2], parse_qos_bucket(buf[3] & 0xff), req_pri, buf[4])
	end
end)

trace_codename("wq_thread_create_failed", function(buf)
	local prefix = get_prefix(buf)
	if buf[3] == 0 then
		printf("%s\tfailed to create new workqueue thread, kern_return: 0x%x\n",
			prefix, buf[2])
	elseif buf[3] == 1 then
		printf("%s\tfailed to vm_map workq thread stack: 0x%x\n", prefix, buf[2])
	elseif buf[3] == 2 then
		printf("%s\tfailed to vm_protect workq thread guardsize: 0x%x\n", prefix, buf[2])
	end
end)

trace_codename("wq_thread_create", function(buf)
	printf("%s\tcreated new workqueue thread\n", get_prefix(buf))
end)

trace_codename("wq_wqops_reqthreads", function(buf)
	local prefix = get_prefix(buf)
	printf("%s\tuserspace requested %d threads at %s\n", prefix, buf[2], parse_pthread_priority(buf[3]));
end)

trace_codename("wq_kevent_reqthreads", function(buf)
	local prefix = get_prefix(buf)
	if buf[4] == 0 then
		printf("%s\tkevent requested a thread at %s\n", prefix, parse_pthread_priority(buf[3]));
	elseif buf[4] == 1 then
		printf("%s\tworkloop requested a thread for req %x at %s\n", prefix, buf[2], parse_pthread_priority(buf[3]));
	elseif buf[4] == 2 then
		printf("%s\tworkloop updated priority of req %x to %s\n", prefix, buf[2], parse_pthread_priority(buf[3]));
	elseif buf[4] == 3 then
		printf("%s\tworkloop canceled req %x\n", prefix, buf[2], parse_pthread_priority(buf[3]));
	elseif buf[4] == 4 then
		printf("%s\tworkloop redrove a thread request\n", prefix);
	end
end)

trace_codename("wq_constrained_admission", function(buf)
	local prefix = get_prefix(buf)
	if buf[2] == 1 then
		printf("fail: %s\twq_constrained_threads_scheduled=%d >= wq_max_constrained_threads=%d\n",
                prefix, buf[3], buf[4])
	elseif (buf[2] == 2) or (buf[2] == 3) then
		local success = nil;
		if buf[2] == 2 then success = "success"
		else success = "fail" end
		printf("%s: %s\tthactive_count=%d + busycount=%d >= wq->wq_max_concurrency\n",
				prefix, success, buf[3], buf[4])
	end
end)

-- The trace codes we need aren't enabled by default
darwin.sysctlbyname("kern.pthread_debug_tracing", 1)
completion_handler = function()
	darwin.sysctlbyname("kern.pthread_debug_tracing", 0)
end
trace.set_completion_handler(completion_handler)
