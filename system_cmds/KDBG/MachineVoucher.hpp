//
//  MachineVoucher.hpp
//  KDBG
//
//  Created by James McIlree on 2/18/14.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#ifndef kernel_perf_cmds_MachineVoucher_hpp
#define kernel_perf_cmds_MachineVoucher_hpp

template <typename SIZE> class Machine;

enum class kMachineVoucherFlag : std::uint32_t {
        CreatedByVoucherCreate		= 0x00000001,
	CreatedByFirstUse		= 0x00000002,
	CreatedByPreviousMachineState	= 0x00000004,
	IsNullVoucher			= 0x00000008,
	IsUnsetVoucher			= 0x00000010,
	IsDestroyed			= 0x00000020
};

template <typename SIZE>
class MachineVoucher {
    protected:
	AbsInterval			_timespan;
	uint8_t* 			_content_bytes;
	uint32_t 			_content_bytes_size;
	uint32_t 			_content_bytes_capacity;
	uint32_t			_id;
	uint32_t			_flags;
	typename SIZE::ptr_t		_address;

	static uint32_t voucher_id();

	// Voucher pointers are used as unique identifiers for the lifespan
	// of the voucher, which may exceed the lifespan of the Machine.
	// We may not copy or "move" a voucher.

	// Disable copy operators
	MachineVoucher(const MachineVoucher& ignored) = delete;
	MachineVoucher& operator=(const MachineVoucher& ignored) = delete;

	// Disable move operators
	MachineVoucher(MachineVoucher&& ignored) = delete;
	MachineVoucher& operator=(MachineVoucher&& ignored) = delete;

	friend class Machine<SIZE>;

	void workaround_16898190(kMachineVoucherFlag flags, uint32_t content_bytes_capacity);

	void add_content_bytes(uint8_t* bytes);

	void set_destroyed(AbsTime timestamp);

        // These are needed to make vouchers that are still alive at the
        // end of an event trace appear correctly in searches/queries.
        // However, when forwarding live vouchers to a future machine state,
        // that work must be undone.
	void set_timespan_to_end_of_time() {
		ASSERT(is_live(), "Modifying timespan of destroyed voucher");
		ASSERT(_timespan.length() == 0, "Modifying timespan after it has already been set");
		_timespan.set_length(AbsTime(UINT64_MAX) - _timespan.location());
	}

	void set_timespan_to_zero_length() {
		ASSERT(is_live(), "Modifying timespan of destroyed voucher");
		ASSERT(_timespan.max() == UINT64_MAX, "Modifying timespan after it has already been set");
		_timespan.set_length(AbsTime(0));
	}
	
    public:
	MachineVoucher(typename SIZE::ptr_t address, AbsInterval create_timespan, kMachineVoucherFlag flags, uint32_t content_bytes_capacity);

	~MachineVoucher() {
		if (_content_bytes) {
			free(_content_bytes);
			_content_bytes = nullptr;
		}
	}

	bool operator==(const MachineVoucher& rhs) const	{ return this->_id == rhs._id; }
	bool operator!=(const MachineVoucher& rhs) const	{ return !(*this == rhs); }

	bool is_live() const					{ return (_flags & (uint32_t)kMachineVoucherFlag::IsDestroyed) == 0; }
	bool is_destroyed() const				{ return (_flags & (uint32_t)kMachineVoucherFlag::IsDestroyed) > 0; }
	bool is_null() const					{ return (_flags & (uint32_t)kMachineVoucherFlag::IsNullVoucher) > 0; }
	bool is_unset() const					{ return (_flags & (uint32_t)kMachineVoucherFlag::IsUnsetVoucher) > 0; }
	bool is_created_by_voucher_create() const		{ return (_flags & (uint32_t)kMachineVoucherFlag::CreatedByVoucherCreate) > 0; }
	bool is_created_by_first_use() const			{ return (_flags & (uint32_t)kMachineVoucherFlag::CreatedByFirstUse) > 0; }
	bool is_created_by_previous_machine_state() const	{ return (_flags & (uint32_t)kMachineVoucherFlag::CreatedByPreviousMachineState) > 0; }
	bool has_valid_contents() const				{ return _content_bytes_size > 0 && _content_bytes_size == _content_bytes_capacity; }

	typename SIZE::ptr_t address() const			{ return _address; }
        AbsInterval timespan() const				{ return _timespan; }
	const uint8_t* content_bytes() const			{ return _content_bytes; }
	uint32_t content_size() const				{ return _content_bytes_capacity; }
        uint32_t id() const					{ return _id; }
};

template <typename SIZE>
uint32_t MachineVoucher<SIZE>::voucher_id() {
	static uint32_t voucher_id = 1;
	return OSAtomicIncrement32Barrier((volatile int32_t*)&voucher_id);
}

template <typename SIZE>
MachineVoucher<SIZE>::MachineVoucher(typename SIZE::ptr_t address, AbsInterval timespan, kMachineVoucherFlag flags, uint32_t content_bytes_capacity) :
	_timespan(timespan),
	_content_bytes((content_bytes_capacity > 0) ? (uint8_t*)malloc((size_t)content_bytes_capacity) : nullptr),
	_content_bytes_size(0),
	_content_bytes_capacity(content_bytes_capacity),
	_id(voucher_id()),
	_flags((uint32_t)flags),
	_address(address)
{
	DEBUG_ONLY({
		if (!is_null() && !is_unset()) {
			ASSERT(timespan.location() != 0 || is_created_by_first_use(), "Only implicitly created vouchers should have an unknown (0) create time");
			ASSERT(is_created_by_voucher_create() || is_created_by_first_use() , "Should have a create flag");
			ASSERT(content_bytes_capacity == 0 || is_created_by_voucher_create(), "Implicitly created vouchers should not have content");
		}
	})
}

template <typename SIZE>
void MachineVoucher<SIZE>::workaround_16898190(kMachineVoucherFlag flags, uint32_t content_bytes_capacity) {
	ASSERT(_content_bytes_capacity == 0, "Attempting to reset non-zero content_bytes_capacity");
	ASSERT(!is_null(), "Sanity");
	ASSERT(!is_unset(), "Sanity");
	ASSERT(is_live(), "Should be live"); // This may be too strong, some races could have destroy before create

	_flags |= (uint32_t)flags;
	_content_bytes_capacity = content_bytes_capacity;
}

template <typename SIZE>
void MachineVoucher<SIZE>::add_content_bytes(uint8_t* src) {
	ASSERT(src, "Sanity");

	// If the first reference we see to a voucher is an MACH_IPC_VOUCHER_CREATE_ATTR_DATA,
	// we will not have contents.
	if (!is_created_by_first_use()) {
		size_t bytes_remaining = _content_bytes_capacity - _content_bytes_size;
		ASSERT(bytes_remaining > 0, "Sanity");

		// We either write an entire tracepoint worth of data,
		// or the # of bytes remaining.
		size_t bytes_to_write = std::min(bytes_remaining, sizeof(typename SIZE::ptr_t) * 4);
		auto dest = &_content_bytes[_content_bytes_size];
		memcpy(dest, src, bytes_to_write);
		_content_bytes_size += bytes_to_write;
	}
}

template <typename SIZE>
void MachineVoucher<SIZE>::set_destroyed(AbsTime timestamp) {
	ASSERT(!is_destroyed(), "Sanity");
	ASSERT(timestamp > _timespan.location(), "Sanity");
	ASSERT(_timespan.length() == 0, "Sanity");
	
        // It turns out this is too strong. The kernel has a limited amount of buffer space available
        // to hold the voucher contents. If the voucher exceeds that, no contents are emitted, and we
        // fail this assert.
        // ASSERT(_content_bytes_capacity == _content_bytes_size, "Destroying voucher with incomplete contents");
	
	// +1 to make sure searches for this voucher at the destroy timestamp
	// can find it.
	_timespan.set_length((timestamp - _timespan.location()) + AbsTime(1));
	_flags |= (uint32_t)kMachineVoucherFlag::IsDestroyed;
}


#endif
