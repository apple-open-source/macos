#!/usr/local/bin/luatrace -s

trace_codename = function(codename, callback)
	local debugid = trace.debugid(codename)
	if debugid ~= 0 then 
		trace.single(debugid,callback) 
	end
end

initial_timestamp = 0
get_prefix = function(buf)
	if initial_timestamp == 0 then
		initial_timestamp = buf.timestamp
	end
	local prefix
	if trace.debugid_is_start(buf.debugid) then 
		prefix = "→" 
	elseif trace.debugid_is_end(buf.debugid) then 
		prefix = "←" 
	else 
		prefix = "↔" 
	end
	local secs = (buf.timestamp - initial_timestamp)   / 1000 / 1000000
	local usecs = (buf.timestamp - initial_timestamp) / 1000 % 1000000
	return string.format("%s %6d.%06d %-16s[%06x] %-24s",
		prefix, secs, usecs, buf.command, buf.threadid, buf.debugname)
end

parse_pthread_priority = function(pri)
	local qos = bit32.rshift(bit32.band(pri, 0x00ffff00), 8)
	if qos == 0x20 then
		return "UInter"
	elseif qos == 0x10 then
		return "UInit"
	elseif qos == 0x08 then
		return "Dflt"
	elseif qos == 0x04 then
		return "Util"
	elseif qos == 0x02 then
		return "BG"
	elseif qos == 0x01 then
		return "Maint"
	elseif qos == 0x00 then
		return "Unsp"
	else
		return "Unkn"
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
	printf("%s\trecording event manager request at %s, existing at %d, %d running\n",
		prefix, parse_pthread_priority(buf.arg2), buf.arg3, buf.arg4)
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
	if bit32.band(buf.arg2, 0x80) then
		printf("%s\tworkqueue overcimmitted @ %s, starting timer (thactive_count: %d, busycount; %d)",
			prefix, parse_pthread_priority(buf.arg2), buf.arg3, buf.arg4)
	else
		printf("%s\tworkqueue overcimmitted @ %s (thactive_count: %d, busycount; %d)",
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
			printf("%s\tthread %d looking for next request (idlecount: %d, reqcount: %d)\n",
				prefix, buf.threadid, buf.arg3, buf.arg4)
		else
			printf("%s\ttrying to run a request on an idle thread (idlecount: %d, reqcount: %d)\n",
				prefix, buf.arg3, buf.arg4)
		end
	else
		if buf.arg4 == 1 then
			printf("%s\tkicked off work on thread %d (overcommit: %d)\n", prefix, buf.arg2, buf.arg3)
		elseif buf.arg4 == 2 then
			printf("%s\tno work/threads (start_timer: %d)\n", prefix, buf.arg3)
		elseif buf.arg4 == 3 then
			printf("%s\tthread parked\n", prefix)
		elseif buf.arg4 == 4 then
			printf("%s\treturning with new request\n", prefix)
		else
			printf("%s\tWARNING: UNKNOWN END CODE:%d\n", prefix, buf.arg4)
		end
	end
end)

trace_codename("wq_runitem", function(buf)
	local prefix = get_prefix(buf)
	if trace.debugid_is_start(buf.debugid) then
		printf("%s\trunning an item at %s (flags: %x)\n", prefix, parse_pthread_priority(buf.arg3), buf.arg2)
	else
		printf("%s\tthread returned\n", prefix)
	end
end)

trace_codename("wq_thread_yielded", function(buf)
	local prefix = get_prefix(buf)
	if trace.debugid_is_start(buf.debugid) then
		printf("%s\tthread_yielded called (yielded_count: %d, reqcount: %d)\n",
			prefix, buf.arg2, buf.arg3)
	else
		if (buf.arg4 == 1) then
			printf("%s\tthread_yielded completed kicking thread (yielded_count: %d, reqcount: %d)\n",
				prefix, buf.arg2, buf.arg3)
		elseif (buf.arg4 == 2) then
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


-- The trace codes we need aren't enabled by default
darwin.sysctlbyname("kern.pthread_debug_tracing", 1)
completion_handler = function()
	darwin.sysctlbyname("kern.pthread_debug_tracing", 0)
end
trace.set_completion_handler(completion_handler)

