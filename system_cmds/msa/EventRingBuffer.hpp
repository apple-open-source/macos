//
//  EventRingBuffer.hpp
//  msa
//
//  Created by James McIlree on 10/8/13.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#ifndef __msa__EventRingBuffer__
#define __msa__EventRingBuffer__

template <typename SIZE>
class EventRingBuffer {
    protected:
	const Globals&			_globals; // Used for printing the ringbuffer
	std::vector<KDEvent<SIZE>>	_events;
	std::size_t			_head;
	std::size_t			_tail;

    public:
	EventRingBuffer(const Globals& globals, std::size_t size);

	// Returns:
	//
	// events, capacity, number_read
	std::tuple<KDEvent<SIZE>*, std::size_t, std::size_t> read();

	void print() const;
	void print_event_index(std::size_t index) const;
	void print_all_events() const;
	void print_last_events(std::size_t lastN) const;
	void print_from_timestamp(uint64_t timestamp) const;
};

template <typename SIZE>
EventRingBuffer<SIZE>::EventRingBuffer(const Globals& globals, std::size_t size) :
	_globals(globals),
	_events(size),
	_head(0),
	_tail(0)
{
	ASSERT(size, "Sanity");

	// Force all pages into memory so the first bazillion
	// trace entries aren't VM_FAULT...
	bzero(_events.data(), _events.size() * sizeof(KDEvent<SIZE>));
}

template <typename SIZE>
std::tuple<KDEvent<SIZE>*, std::size_t, std::size_t> EventRingBuffer<SIZE>::read() {
	std::size_t modulo_index = _tail % _events.size();
	std::size_t count, capacity = _events.size() - modulo_index;
	KDEvent<SIZE>* events = &_events.data()[modulo_index];

	if ((count = KDBG::read(events, capacity * sizeof(KDEvent<SIZE>)))) {
		// Update head/tail as soon as we have added data.
		_tail += count;
		if (_tail - _head > _events.size()) {
			_head += count;
		}
	}

	return std::make_tuple(events, count, capacity);
}

#if 0

template <typename SIZE>
void EventRingBuffer<SIZE>::print() const {
	printf("%zu events in buffer [%zu -> %zu)\n", _tail - _head, _head, _tail);
}

template <typename SIZE>
void EventRingBuffer<SIZE>::print_event_index(std::size_t index) const {
	const KDEvent<SIZE>& event = _events[index % _events.size()];

	const char* type = event.is_func_start() ?  "beg" : (event.is_func_end() ? "end" : "---");
	auto trace_code_it = _globals.trace_codes().find(event.dbg_cooked());

	if (trace_code_it == _globals.trace_codes().end()) {
		printf("event[%ld] { timestamp=%llx, arg1=%llx, arg2=%llx, arg3=%llx, arg4=%llx, tid=%llx, %4s %x, cpu=%u }\n", index, event.timestamp().value(),
		       (uint64_t)event.arg1(), (uint64_t)event.arg2(), (uint64_t)event.arg3(), (uint64_t)event.arg4(), (uint64_t)event.tid(), type, event.dbg_cooked(), event.cpu());
	} else {
		printf("event[%ld] { timestamp=%llx, arg1=%llx, arg2=%llx, arg3=%llx, arg4=%llx, tid=%llx, %4s %s, cpu=%u }\n", index, event.timestamp().value(),
		       (uint64_t)event.arg1(), (uint64_t)event.arg2(), (uint64_t)event.arg3(), (uint64_t)event.arg4(), (uint64_t)event.tid(), type, trace_code_it->second.c_str(), event.cpu());
	}
}

template <typename SIZE>
void EventRingBuffer<SIZE>::print_all_events() const {
	std::size_t begin = _head;
	while (begin < _tail) {
		print_event_index(begin++);
	}
}

template <typename SIZE>
void EventRingBuffer<SIZE>::print_last_events(std::size_t lastN) const {
	std::size_t length = std::min(lastN, _tail - _head);
	std::size_t begin = _tail - length;
	ASSERT(begin <= _tail, "Sanity");
	while (begin < _tail) {
		print_event_index(begin++);
	}
}

template <typename SIZE>
void EventRingBuffer<SIZE>::print_from_timestamp(uint64_t t) const {
	std::size_t begin = _head;
	while (begin < _tail) {
		const KDEvent<SIZE>& event = _events[begin % _events.size()];
		if (event.timestamp() >= t)
			break;
		begin++;
	}

	while (begin < _tail) {
		print_event_index(begin++);
	}
}

void PrintEventRingBuffer() {
	//	uint64_t	_timestamp;
	//	uint64_t	_arg1;
	//	uint64_t	_arg2;
	//	uint64_t	_arg3;
	//	uint64_t	_arg4;
	//	uint64_t	_thread;
	//	uint32_t	_debugid;
	//	uint32_t	_cpuid;

	const KDEvent<Kernel64>* events = (const KDEvent<Kernel64>*)g_rb;
	for (std::size_t i=ring_buffer_head_index; i<ring_buffer_tail_index; i++) {
		const KDEvent<Kernel64>& event = events[i % g_rb_size];
		printf("event[%ld] { timestamp=%llx, ", i, event.timestamp().value());
		printf("arg1=%llx, ", event.arg1());
		printf("arg2=%llx, ", event.arg2());
		printf("arg3=%llx, ", event.arg3());
		printf("arg4=%llx, ", event.arg4());
		printf("tid=%llx, ", event.tid());
		const char* type = event.is_func_start() ?  "beg" : (event.is_func_end() ? "end" : "---");
		auto trace_code_it = gglobals->trace_codes().find(event.dbg_cooked());
		if (trace_code_it == gglobals->trace_codes().end()) {
			printf("%4s %x, ", type, event.dbg_cooked());
		} else {
			printf("%4s %s, ", type, trace_code_it->second.c_str());
		}
		printf("cpu=%u }\n", event.cpu());
	}
	printf("%lu\n", ring_buffer_tail_index - ring_buffer_head_index);
}
#endif

#endif /* defined(__staintracker__EventRingBuffer__) */
