//
//  TraceDataHeader.hpp
//  KDBG
//
//  Created by James McIlree on 10/25/12.
//  Copyright (c) 2014 Apple. All rights reserved.
//

//
// We have to specialize this, as the K64 min alignment is 4 bytes longer,
// to maintain 8 byte alignment for the uint64_t _TOD_secs.
//

template <typename KERNEL_SIZE> class TraceDataHeaderFields {};

template <>
class TraceDataHeaderFields<Kernel32> {
    public:
	uint32_t	version;
	uint32_t	thread_count;
	uint32_t	TOD_secs_top_half;
	uint32_t	TOD_secs_bottom_half;
	uint32_t	TOD_usecs;

	// NOTE! The compiler has shown a tendency to place this on non 8 byte
	// aligned addresses when stack allocating. We need to construct the
	// uint64_t values by logical-or and shifting, treating as a pointer
	// will fail!

	TraceDataHeaderFields(uint32_t v, uint32_t tc, uint64_t s, uint32_t us) :
		version(v),
		thread_count(tc),
		TOD_usecs(us)
	{
		TOD_secs_top_half = (uint32_t)(s >> 32);
		TOD_secs_bottom_half = (uint32_t)(s & 0xFFFFFFFF);
	}

	uint64_t TOD_secs() {
		return ((uint64_t)TOD_secs_top_half << 32) | (uint64_t)TOD_secs_bottom_half;
	}
};

template <>
class TraceDataHeaderFields<Kernel64> {
    public:
	uint32_t	version;
	uint32_t	thread_count;
	uint64_t	_TOD_secs;
	uint32_t	TOD_usecs;
	uint32_t	_force_alignment; // Need to force 8 byte alignment in 32 bit code

	TraceDataHeaderFields(uint32_t v, uint32_t tc, uint64_t s, uint32_t us) :
		version(v),
		thread_count(tc),
		_TOD_secs(s),
		TOD_usecs(us),
		_force_alignment(0)
	{
	}

	uint64_t TOD_secs() {
		return _TOD_secs;
	}
};

template <typename KERNEL_SIZE>
class TraceDataHeader {
    private:
	TraceDataHeaderFields<KERNEL_SIZE>	_fields;

    public:
	TraceDataHeader() : _fields(0, 0, 0, 0) {}
	TraceDataHeader(uint32_t v, uint32_t tc, uint64_t s, uint32_t us) : _fields(v, tc, s, us) {}

	uint32_t version() const			{ return _fields.version; }
	uint32_t thread_count() const			{ return _fields.thread_count; }
	uint64_t TOD_secs() const			{ return _fields.TOD_secs(); }
	uint32_t TOD_usecs() const			{ return _fields.TOD_usecs; }
};

