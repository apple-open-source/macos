//
//  EventPrinting.hpp
//  kdprof
//
//  Created by James McIlree on 4/20/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef kdprof_ParallelPrinting_hpp
#define kdprof_ParallelPrinting_hpp

void print_event_header(const Globals& globals, bool is_64_bit);

template <typename SIZE>
char* print_event(char* buf, char* buf_end, const Globals& globals, const Machine<SIZE>& machine, const KDEvent<SIZE>& event, uintptr_t event_index)
{
	// Header is...
	//
	// [Index] Time Type  Code arg1 arg2 arg3 arg4 thread cpu# command/IOP-name pid
	//      8    16    4  34   8/16 8/16 8/16 8/16     10    4 16               6
	//
	// For now, each column is folding up the "after" spacing in a single printf, IOW
	//
	// buf += snprintf(buf, buf_end - buf, "%8s  ", "COL"); /* + 2 spaces */
	//
	// Not:
	//
	// buf += snprintf(buf, buf_end - buf, "%8s", "COL");
	// buf += snprintf(buf, buf_end - buf, "  "); /* 2 spaces */

	ASSERT(event.cpu() > -1 && event.cpu() < machine.cpus().size(), "cpu_id out of range");
	const MachineCPU<SIZE>& cpu = machine.cpus()[event.cpu()];

	//
	// Okay, here is how snprintf works.
	//
	// char buf[2];
	//
	// snprintf(buf, 0, "a"); // Returns 1, buf is unchanged.
	// snprintf(buf, 1, "a"); // Returns 1, buf = \0
	// snprintf(buf, 2, "a"); // Returns 1, buf = 'a', \0

	//
	// If we cannot print successfully, we return the orignal pointer.
	//
	char* orig_buf = buf;

	//
	// [Index]
	//
	if (globals.should_print_event_index()) {
		buf += snprintf(buf, buf_end - buf, "%8llu ", (uint64_t)event_index);
	}

	if (buf >= buf_end)
		return orig_buf;

	//
	// Time
	//
	if (globals.should_print_mach_absolute_timestamps()) {
		if (globals.beginning_of_time().value() == 0)
			buf += snprintf(buf, buf_end - buf, "%16llX  ", (event.timestamp() - globals.beginning_of_time()).value());
		else
			buf += snprintf(buf, buf_end - buf, "%16llu  ", (event.timestamp() - globals.beginning_of_time()).value());
	} else {
		NanoTime ntime = (event.timestamp() - globals.beginning_of_time()).nano_time(globals.timebase());
		buf += snprintf(buf, buf_end - buf, "%16.2f  ", (double)ntime.value() / 1000.0);
	}

	if (buf >= buf_end)
		return orig_buf;

	//
	// Type Code
	//
	const char* type = event.is_func_start() ?  "beg" : (event.is_func_end() ? "end" : "---");
	auto trace_code_it = globals.trace_codes().find(event.dbg_cooked());
	if (cpu.is_iop() || !globals.should_print_symbolic_event_codes() || trace_code_it == globals.trace_codes().end()) {
		buf += snprintf(buf, buf_end - buf, "%4s   %-34x ", type, event.dbg_cooked());
	} else {
		buf += snprintf(buf, buf_end - buf, "%4s   %-34s ", type, trace_code_it->second.c_str());
	}

	if (buf >= buf_end)
		return orig_buf;

	//
	// arg1
	//
	if (event.dbg_class() == DBG_IOKIT && event.dbg_subclass() == DBG_IOPOWER) {
		std::string kext_name = event.arg1_as_string();
		std::reverse(kext_name.begin(), kext_name.end());

		if (SIZE::is_64_bit)
			buf += snprintf(buf, buf_end - buf, "%-16s ", kext_name.c_str());
		else
			buf += snprintf(buf, buf_end - buf, "%-8s ", kext_name.c_str());
	} else {
		if (SIZE::is_64_bit)
			buf += snprintf(buf, buf_end - buf, "%-16llX ", (uint64_t)event.arg1());
		else
			buf += snprintf(buf, buf_end - buf, "%-8x ", (uint32_t)event.arg1());
	}

	if (buf >= buf_end)
		return orig_buf;

	//
	// Profiling showed that the repeated snprintf calls were hot, rolling them up is ~2.5% per on a HUGE file.
	//
	// arg2 arg3 arg4 thread cpu
	//
	if (SIZE::is_64_bit)
		buf += snprintf(buf, buf_end - buf, "%-16llX %-16llX %-16llX %10llX %4u ", (uint64_t)event.arg2(), (uint64_t)event.arg3(), (uint64_t)event.arg4(), (uint64_t)event.tid(), event.cpu());
	else
		buf += snprintf(buf, buf_end - buf, "%-8x %-8x %-8x %10llX %4u ", (uint32_t)event.arg2(), (uint32_t)event.arg3(), (uint32_t)event.arg4(), (uint64_t)event.tid(), event.cpu());

	if (buf >= buf_end)
		return orig_buf;

	//
	// command & pid (handled together due to IOP not printing a pid
	//
	if (cpu.is_iop()) {
		// We print the IOP name instead of a command
		buf += snprintf(buf, buf_end - buf, "%-16s\n", cpu.name());
	} else {
		if (const MachineThread<SIZE>* thread = machine.thread(event.tid(), event.timestamp())) {
			buf += snprintf(buf, buf_end - buf, "%-16s %-6d\n", thread->process().name(), thread->process().pid());
		} else {
			buf += snprintf(buf, buf_end - buf, "%-16s %-6s\n", "?????", "???");
		}
	}

	// Still need to check this, its an error if we overflow on the last print!
	if (buf >= buf_end)
		return orig_buf;

	return buf;
}

template <typename SIZE>
char* print_event_range_to_buffer(const Globals& globals, const Machine<SIZE>& machine, TRange<uintptr_t> range, MemoryBuffer<char>& buffer ) {
	char* cursor = buffer.data();
	char* cursor_end = cursor + buffer.capacity();

	if (const KDEvent<SIZE>* events = machine.events()) {
		ASSERT(TRange<uintptr_t>(0, machine.event_count()).contains(range), "Sanity");
		for (uintptr_t index = range.location(); index < range.max(); ++index) {
			char* temp = print_event(cursor, cursor_end, globals, machine, events[index], index);
			if (temp != cursor)
				cursor = temp;
			else {
				// Changing the capacity will invalidate the cursor
				ptrdiff_t offset = cursor - buffer.data();
				buffer.set_capacity(buffer.capacity()*2);
				cursor = buffer.data() + offset;
				cursor_end = buffer.data() + buffer.capacity();
			}
		}
	}

	return cursor;
}

class PrintWorkUnit {
    protected:
	MemoryBuffer<char>	_buffer;
	TRange<uintptr_t>	_event_range;
	char*			_buffer_end;

	// We do not want work units copied.
	PrintWorkUnit(const PrintWorkUnit& that) = delete;
	PrintWorkUnit& operator=(const PrintWorkUnit& other) = delete;

    public:
	PrintWorkUnit(MemoryBuffer<char>&& buffer, TRange<uintptr_t> event_range, char* buffer_end) :
		_buffer(std::move(buffer)),
		_event_range(event_range),
		_buffer_end(buffer_end)
	{
		ASSERT(_buffer.capacity(), "Sanity");
		ASSERT(_buffer.data(), "Sanity");
		ASSERT(!_buffer_end || _buffer_end > _buffer.data(), "Sanity");
		ASSERT(!_buffer_end || (_buffer_end < _buffer.data() + _buffer.capacity()), "Sanity");
	}

	MemoryBuffer<char>& buffer()			{ return _buffer; }

	TRange<uintptr_t> event_range()			{ return _event_range; }
	void set_event_range(TRange<uintptr_t> range)	{ _event_range = range; }

	char* buffer_end() const			{ return _buffer_end; }
	void set_buffer_end(char* buffer_end)		{ _buffer_end = buffer_end; }
};

template <typename SIZE>
class PrintProducer {
    protected:
	const Globals&		_globals;
	const Machine<SIZE>&	_machine;
	uintptr_t		_start_index;
	uintptr_t		_end_index;
	uintptr_t		_chunk_size;

    public:
	PrintProducer(const Globals& globals, const Machine<SIZE>& machine, uintptr_t chunk_size) :
		_globals(globals),
		_machine(machine),
		_chunk_size(chunk_size)
	{
		_start_index = 0;
		_end_index = machine.event_count();

		if (globals.is_summary_start_set() || globals.is_summary_stop_set()) {
			AbsInterval machine_timespan = machine.timespan();

			KDEvent<SIZE> start_event(globals.summary_start(machine_timespan));
			auto it = std::lower_bound(machine.events(), machine.events() + _end_index, start_event);
			ASSERT(&*it >= machine.events(), "Returned start index lower than start");
			_start_index = std::distance(machine.events(), it);

			KDEvent<SIZE> end_event(globals.summary_stop(machine_timespan));
			it = std::lower_bound(machine.events(), machine.events() + _end_index, end_event);
			ASSERT(&*it <= machine.events() + _end_index, "Returned end index greater than end");
			_end_index = std::distance(machine.events(), it);

			ASSERT(_start_index <= _end_index, "start index is > end index");
		}
	}

	bool produce(PrintWorkUnit& work_unit) {
		// Claim a chunk of work to do
		uintptr_t orig_start_index, new_start_index;
		do {
			orig_start_index = _start_index;
			new_start_index = orig_start_index + std::min(_chunk_size, _end_index - orig_start_index);
		} while (orig_start_index < _end_index && !OSAtomicCompareAndSwapPtrBarrier((void*)orig_start_index, (void *)new_start_index, (void * volatile *)&_start_index));

		// Did we claim work?
		if (orig_start_index < _end_index) {
			TRange<uintptr_t> event_range(orig_start_index, new_start_index - orig_start_index);
			char* end = print_event_range_to_buffer(_globals, _machine, event_range, work_unit.buffer());

			work_unit.set_event_range(event_range);
			work_unit.set_buffer_end(end);
			return true;
		}

		return false;
	}

	uintptr_t start_index() const		{ return _start_index; }
};

template <typename SIZE>
class PrintConsumer {
    protected:
	const Globals&		_globals;
	uintptr_t		_write_index;
	std::mutex		_write_mutex;
	std::condition_variable	_write_condition;

    public:
	PrintConsumer(const Globals& globals, const Machine<SIZE>& machine, uintptr_t start_index) :
		_globals(globals),
		_write_index(start_index)
	{
	}

	void consume(PrintWorkUnit& work_unit) {
		std::unique_lock<std::mutex> guard(_write_mutex);
		_write_condition.wait(guard, [&](){ return work_unit.event_range().location() == this->_write_index; });

		ASSERT(work_unit.event_range().location() == _write_index, "Sanity");

		char* data = work_unit.buffer().data();
		size_t bytes = work_unit.buffer_end() - data;
		write(_globals.output_fd(), work_unit.buffer().data(), bytes);
		_write_index = work_unit.event_range().max();

		_write_condition.notify_all();
	}
};

template <typename SIZE>
uintptr_t print_machine_events(const Globals& globals, const Machine<SIZE>& machine) {
	print_event_header(globals, SIZE::is_64_bit);

	if (const KDEvent<SIZE>* events = machine.events()) {
		if (uintptr_t event_count = machine.event_count()) {

			//
			// We want to chunk this up into reasonably sized pieces of work.
			// Because each piece of work can potentially accumulate a large
			// amount of memory, we need to limit the amount of work "in-flight".
			//
			uint32_t active_cpus = Kernel::active_cpu_count();

			uintptr_t chunk_size = 2000;

			PrintProducer<SIZE> producer(globals, machine, chunk_size);
			PrintConsumer<SIZE> consumer(globals, machine, producer.start_index());

			std::vector<std::thread> threads;
			for (uint32_t i=0; i<active_cpus; ++i) {
				threads.push_back(std::thread([&]() {
					PrintWorkUnit work_unit(MemoryBuffer<char>(160 * chunk_size), TRange<uintptr_t>(0, 0), (char*)NULL);
					while (producer.produce(work_unit)) {
						consumer.consume(work_unit);
					}
				}));
			}

			for(auto& thread : threads){
				thread.join();
			}

			uint32_t totalProcesses = 0;
			uint32_t totalThreads = 0;

			for (auto process : machine.processes()) {
				if (!process->is_created_by_previous_machine_state()) {
					totalProcesses++;
				}
			}

			for (auto thread : machine.threads()) {
				if (!thread->is_created_by_previous_machine_state()) {
					totalThreads++;
				}
			}

			dprintf(globals.output_fd(), "Total Events:       %llu\n", (uint64_t)event_count);
			dprintf(globals.output_fd(), "Total Processes:    %u\n", totalProcesses);
			dprintf(globals.output_fd(), "Total Threads:      %u\n", totalThreads);

			return event_count;
		}
	}
	
	return 0;
}

#endif
