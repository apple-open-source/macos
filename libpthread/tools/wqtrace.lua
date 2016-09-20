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
	local secs = (buf.timestamp - initial_timestamp) / 1000000000

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
		return string.format("UInter[%x]", pri);
	elseif qos == 0x10 then
		return string.format("UInit[%x]", pri);
	elseif qos == 0x08 then
		return string.format("Dflt[%x]", pri);
	elseif qos == 0x04 then
		return string.format("Util[%x]", pri);
	elseif qos == 0x02 then
		return string.format("BG[%x]", pri);
	elseif qos == 0x01 then
		return string.format("Maint[%x]", pri);
	elseif qos == 0x00 then
		return string.format("Unsp[%x]", pri);
	else
		return string.format("Unkn[%x]", pri);
	end
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

-- thread requests

trace_codename("wq_kevent_req_threads", function(buf)
	local prefix = get_prefix(buf)
	if trace.debugid_is_start(buf.debugid) then
		printf("%s\tkevent requesting threads (requests[] length = %d)\n", prefix, buf.arg2)
	else
		printf("%s\tkevent request complete (start_timer: %d)\n", prefix, buf.arg2)
	end
end)

trace_codename("wq_req_threads", function(buf)
	local prefix = get_prefix(buf)
	printf("%s\trecording %d constrained request(s) at %s, total %d requests\n",
		prefix, buf.arg4, parse_pthread_priority(buf.arg2), buf.arg3)
end)

trace_codename("wq_req_octhreads", function(buf)
	local prefix = get_prefix(buf)
	printf("%s\tattempting %d overcommit request(s) at %s, total %d requests\n",
		prefix, buf.arg4, parse_pthread_priority(buf.arg2), buf.arg3)
end)
trace_codename("wq_delay_octhreads", function(buf)
	local prefix = get_prefix(buf)
	printf("%s\trecording %d delayed overcommit request(s) at %s, total %d requests\n",
		prefix, buf.arg4, parse_pthread_priority(buf.arg2), buf.arg3)
end)

trace_codename("wq_req_kevent_threads", function(buf)
	local prefix = get_prefix(buf)
	printf("%s\trecording kevent constrained request at %s, total %d requests\n",
		prefix, parse_pthread_priority(buf.arg2), buf.arg3)
end)
trace_codename("wq_req_kevent_octhreads", function(buf)
	local prefix = get_prefix(buf)
	printf("%s\trecording kevent overcommit request at %s, total %d requests\n",
		prefix, parse_pthread_priority(buf.arg2), buf.arg3)
end)
trace_codename("wq_req_event_manager", function(buf)
	local prefix = get_prefix(buf)
	if buf.arg2 == 1 then
		printf("%s\tstarting event manager thread, existing at %d, %d added\n",
			prefix, buf.arg3, buf.arg4)
	else
		printf("%s\trecording event manager request, existing at %d, %d added\n",
			prefix, buf.arg3, buf.arg4)
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

trace_codename("wq_overcommitted", function(buf)
	local prefix = get_prefix(buf)
	if buf.arg2 & 0x1000000 ~= 0 then
		printf("%s\tworkqueue overcommitted @ %s, starting timer (thactive_count: %d, busycount; %d)\n",
			prefix, parse_pthread_priority(buf.arg2), buf.arg3, buf.arg4)
	else
		printf("%s\tworkqueue overcommitted @ %s (thactive_count: %d, busycount; %d)\n",
			prefix, parse_pthread_priority(buf.arg2), buf.arg3, buf.arg4)
	end
end)

trace_codename("wq_stalled", function(buf)
	local prefix = get_prefix(buf)
	printf("%s\tworkqueue stalled (nthreads: %d)\n", prefix, buf.arg3)
end)

-- thread lifecycle

trace_codename("wq_run_nextitem", function(buf)
	local prefix = get_prefix(buf)
	if trace.debugid_is_start(buf.debugid) then
		if buf.arg2 == 0 then
			printf("%s\ttrying to run a request on an idle thread (idlecount: %d, reqcount: %d)\n",
				prefix, buf.arg3, buf.arg4)
		else
			printf("%s\tthread %x looking for next request (idlecount: %d, reqcount: %d)\n",
				prefix, buf.threadid, buf.arg3, buf.arg4)
		end
	else
		if buf.arg4 == 1 then
			printf("%s\tkicked off work on thread %x (overcommit: %d)\n", prefix, buf.arg2, buf.arg3)
		elseif buf.arg4 == 3 then
			printf("%s\tno work %x can currently do (start_timer: %d)\n", prefix, buf.arg2, buf.arg3)
		elseif buf.arg4 == 4 then
			printf("%s\treturning to run next item\n", prefix)
		else
			printf("%s\tWARNING: UNKNOWN END CODE:%d\n", prefix, buf.arg4)
		end
	end
end)

trace_codename("wq_runitem", function(buf)
	local prefix = get_prefix(buf)
	if trace.debugid_is_start(buf.debugid) then
		printf("%s\tSTART running item\n", prefix)
	else
		printf("%s\tDONE running item; thread returned to kernel\n", prefix)
	end
end)

trace_codename("wq_thread_yielded", function(buf)
	local prefix = get_prefix(buf)
	if trace.debugid_is_start(buf.debugid) then
		printf("%s\tthread_yielded called (yielded_count: %d, reqcount: %d)\n",
			prefix, buf.arg2, buf.arg3)
	else
		if buf.arg4 == 1 then
			printf("%s\tthread_yielded completed kicking thread (yielded_count: %d, reqcount: %d)\n",
				prefix, buf.arg2, buf.arg3)
		elseif buf.arg4 == 2 then
			printf("%s\tthread_yielded completed (yielded_count: %d, reqcount: %d)\n",
				prefix, buf.arg2, buf.arg3)
		else
			printf("%s\tthread_yielded completed unusually (yielded_count: %d, reqcount: %d)\n",
				prefix, buf.arg2, buf.arg3)
		end
	end
end)

trace_codename("wq_thread_block", function(buf)
	local prefix = get_prefix(buf)
	if trace.debugid_is_start(buf.debugid) then
		printf("%s\tthread blocked (activecount: %d, prioritiy: %d, start_time: %d)\n",
			prefix, buf.arg2, buf.arg3, buf.arg3)
	else
		printf("%s\tthread unblocked (threads_scheduled: %d, priority: %d)\n",
			prefix, buf.arg2, buf.arg3)
	end
end)

trace_codename("wq_thread_suspend", function(buf)
	local prefix = get_prefix(buf)
	if trace.debugid_is_start(buf.debugid) then
		printf("%s\tcreated new suspended thread (nthreads:%d)\n",
			prefix, buf.arg2)
	else
		if buf.arg4 == 0xdead then
			printf("%s\tthread exited suspension to die (nthreads: %d)\n",
				prefix, buf.arg3)
		end
	end
end)

trace_codename("wq_thread_park", function(buf)
	local prefix = get_prefix(buf)
	if trace.debugid_is_start(buf.debugid) then
		printf("%s\tthread parked (threads_scheduled: %d, thidlecount: %d, us_to_wait: %d)\n",
			prefix, buf.arg2, buf.arg3, buf.arg4)
	else
		if buf.arg4 == 0xdead then
			printf("%s\tthread exited park to die (nthreads: %d)\n", prefix, buf.arg3)
		end
	end

end)

trace_codename("wq_thread_limit_exceeded", function(buf)
	local prefix = get_prefix(buf)
	printf("%s\ttotal thread limit exceeded, %d threads, total %d max threads, (kern limit: %d)\n",
		prefix, buf.arg2, buf.arg3, buf.arg4)
end)

trace_codename("wq_thread_constrained_maxed", function(buf)
	local prefix = get_prefix(buf)
	printf("%s\tattempted to add thread at max constrained limit, total %d threads (limit: %d)\n",
		prefix, buf.arg2, buf.arg3)
end)

trace_codename("wq_thread_add_during_exit", function(buf)
	local prefix = get_prefix(buf)
	printf("%s\tattempted to add thread during WQ_EXITING\n", prefix)
end)

trace_codename("wq_thread_create_failed", function(buf)
	local prefix = get_prefix(buf)
	if buf.arg3 == 0 then
		printf("%s\tfailed to create new workqueue thread, kern_return: 0x%x\n",
			prefix, buf.arg2)
	elseif buf.arg3 == 1 then
		printf("%s\tfailed to vm_map workq thread stack: 0x%x", prefix, buf.arg2)
	elseif buf.arg3 == 2 then
		printf("%s\tfailed to vm_protect workq thread guardsize: 0x%x", prefix, buf.arg2)
	end
end)

trace_codename("wq_thread_create", function(buf)
	printf("%s\tcreateed new workqueue thread\n", get_prefix(buf))
end)

trace_codename("wq_manager_request", function(buf)
	local prefix = get_prefix(buf)
	printf("%s\tthread in bucket %d\n", prefix, buf.arg3)
end)


-- The trace codes we need aren't enabled by default
darwin.sysctlbyname("kern.pthread_debug_tracing", 1)
completion_handler = function()
	darwin.sysctlbyname("kern.pthread_debug_tracing", 0)
end
trace.set_completion_handler(completion_handler)
