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
	proc = buf.command

	return string.format("%s %6.9f %-17s [%05d.%06x] %-24s",
		prefix, secs, proc, buf.pid, buf.threadid, buf.debugname)
end

decode_lval = function(lval)
	local kbit = " "
	if lval & 0x1 ~= 0 then
		kbit = "K"
	end
	local ebit = " "
	if lval & 0x2 ~= 0 then
		ebit = "E"
	end
	local wbit = " "
	if lval & 0x4 ~= 0 then
		wbit = "W"
	end

	local count = lval >> 8
	return string.format("[0x%06x, %s%s%s]", count, wbit, ebit, kbit)
end

decode_sval = function(sval)
	local sbit = " "
	if sval & 0x1 ~= 0 then
		sbit = "S"
	end
	local ibit = " "
	if sval & 0x2 ~= 0 then
		ibit = "I"
	end

	local count = sval >> 8
	return string.format("[0x%06x, %s%s]", count, ibit, sbit)
end

trace_codename("psynch_mutex_lock_updatebits", function(buf)
	local prefix = get_prefix(buf)
	if buf[4] == 0 then
		printf("%s\tupdated lock bits, pre-kernel (addr: 0x%016x, oldlval: %s, newlval: %s)\n", prefix, buf[1], decode_lval(buf[2]), decode_lval(buf[3]))
	else
		printf("%s\tupdated lock bits, post-kernel (addr: 0x%016x, oldlval: %s, newlval: %s)\n", prefix, buf[1], decode_lval(buf[2]), decode_lval(buf[3]))
	end
end)

trace_codename("psynch_mutex_unlock_updatebits", function(buf)
	local prefix = get_prefix(buf)
	printf("%s\tupdated unlock bits (addr: 0x%016x, oldlval: %s, newlval: %s)\n", prefix, buf[1], decode_lval(buf[2]), decode_lval(buf[3]))
end)

trace_codename("psynch_mutex_ulock", function(buf)
	local prefix = get_prefix(buf)

	if trace.debugid_is_start(buf.debugid) then
		printf("%s\tlock busy, waiting in kernel (addr: 0x%016x, lval: %s, sval: %s, owner_tid: 0x%x)\n",
			prefix, buf[1], decode_lval(buf[2]), decode_sval(buf[3]), buf[4])
	elseif trace.debugid_is_end(buf.debugid) then
		printf("%s\tlock acquired from kernel (addr: 0x%016x, updated bits: %s)\n",
			prefix, buf[1], decode_lval(buf[2]))
	else
		printf("%s\tlock taken, uncontended (addr: 0x%016x, lval: %s, sval: %s)\n",
			prefix, buf[1], decode_lval(buf[2]), decode_sval(buf[3]))
	end
end)

trace_codename("psynch_mutex_utrylock_failed", function(buf)
	local prefix = get_prefix(buf)
	printf("%s\tmutex trybusy addr: 0x%016x lval: %s sval: %s owner: 0x%x\n", prefix, buf[1], decode_lval(buf[2]), decode_sval(buf[3]), buf[4])
end)

trace_codename("psynch_mutex_uunlock", function(buf)
	local prefix = get_prefix(buf)

	if trace.debugid_is_start(buf.debugid) then
		printf("%s\tunlock, signalling kernel waiters (addr: 0x%016x, lval: %s, sval: %s, owner_tid: 0x%x)\n",
			prefix, buf[1], decode_lval(buf[2]), decode_sval(buf[3]), buf[4])
	elseif trace.debugid_is_end(buf.debugid) then
		printf("%s\tunlock, waiters signalled (addr: 0x%016x, updated bits: %s)\n",
			prefix, buf[1], decode_lval(buf[2]))
	else
		printf("%s\tunlock, no kernel waiters (addr: 0x%016x, lval: %s, sval: %s)\n",
			prefix, buf[1], decode_lval(buf[2]), decode_sval(buf[3]))
	end
end)

-- The trace codes we need aren't enabled by default
darwin.sysctlbyname("kern.pthread_debug_tracing", 1)
completion_handler = function()
	darwin.sysctlbyname("kern.pthread_debug_tracing", 0)
end
trace.set_completion_handler(completion_handler)
