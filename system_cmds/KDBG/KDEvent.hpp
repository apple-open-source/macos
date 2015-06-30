//
//  KDEvent.hpp
//  KDBG
//
//  Created by James McIlree on 10/25/12.
//  Copyright (c) 2014 Apple. All rights reserved.
//

template <typename SIZE> class KDEventFields {};

template <>
class KDEventFields<Kernel32> {
    private:
	static const uint64_t K32_TIMESTAMP_MASK = 0x00ffffffffffffffULL;
	static const uint64_t K32_CPU_MASK = 0xff00000000000000ULL;
	static const uint32_t K32_CPU_SHIFT = 56;

    public:
	uint64_t	_timestamp;
	uint32_t	_arg1;
	uint32_t	_arg2;
	uint32_t	_arg3;
	uint32_t	_arg4;
	uint32_t	_thread;
	uint32_t	_debugid;

	int cpu() const			{ return (int) ((_timestamp & K32_CPU_MASK) >> K32_CPU_SHIFT); }
	uint64_t timestamp() const	{ return _timestamp & K32_TIMESTAMP_MASK; }

	uint64_t unused() const		{ THROW("Calling method for field that does not exist"); }
};

template <>
class KDEventFields<Kernel64> {
    public:
	uint64_t	_timestamp;
	uint64_t	_arg1;
	uint64_t	_arg2;
	uint64_t	_arg3;
	uint64_t	_arg4;
	uint64_t	_thread;
	uint32_t	_debugid;
	uint32_t	_cpuid;
	uint64_t	_unused; // Defined as uintptr in orignal header

	int cpu() const			{ return (int)_cpuid; }
	uint64_t timestamp() const	{ return _timestamp; }

	uint64_t unused() const		{ return _unused; }
};

/* The debug code consists of the following
 *
 * ----------------------------------------------------------------------
 *|              |               |                               |Func   |
 *| Class (8)    | SubClass (8)  |          Code (14)            |Qual(2)|
 * ----------------------------------------------------------------------
 * The class specifies the higher level
 */

template <typename SIZE>
class KDEvent {
    private:
	KDEventFields<SIZE> _fields;

	static const uint32_t DBG_CLASS_MASK = 0xFF000000;
	static const uint32_t DBG_CLASS_MASK_SHIFT = 24;
	static const uint32_t DBG_SUBCLASS_MASK = 0x00FF0000;
	static const uint32_t DBG_SUBCLASS_MASK_SHIFT = 16;
	static const uint32_t DBG_CODE_MASK = 0x0000FFFC;
	static const uint32_t DBG_CODE_MASK_SHIFT = 2;
	static const uint32_t DBG_FUNC_MASK = DBG_FUNC_START | DBG_FUNC_END;

    public:
	//
	// Provided only for lower_bounds/upper_bounds binary searches of event buffers
	//
	KDEvent()					{}
	KDEvent(AbsTime timestamp)			{ _fields._timestamp = timestamp.value(); }

	// Sort by time operator for lower_bounds/upper_bounds
	bool operator<(const KDEvent& rhs) const	{ return this->timestamp() < rhs.timestamp(); }

	AbsTime timestamp() const			{ return AbsTime(_fields.timestamp()); }
	typename SIZE::ptr_t tid() const		{ return _fields._thread; }
	int cpu() const					{ return _fields.cpu(); }

	uint32_t dbg_class() const			{ return (_fields._debugid & DBG_CLASS_MASK) >> DBG_CLASS_MASK_SHIFT; }
	uint32_t dbg_subclass() const			{ return (_fields._debugid & DBG_SUBCLASS_MASK) >> DBG_SUBCLASS_MASK_SHIFT; }
	uint32_t dbg_code() const			{ return (_fields._debugid & DBG_CODE_MASK) >> DBG_CODE_MASK_SHIFT; }
	uint32_t dbg_cooked() const			{ return _fields._debugid & ~DBG_FUNC_MASK; }
	uint32_t dbg_raw() const			{ return _fields._debugid; }

	typename SIZE::ptr_t arg1() const		{ return _fields._arg1; }
	typename SIZE::ptr_t arg2() const		{ return _fields._arg2; }
	typename SIZE::ptr_t arg3() const		{ return _fields._arg3; }
	typename SIZE::ptr_t arg4() const		{ return _fields._arg4; }

	uint8_t* arg1_as_pointer() const		{ return (uint8_t*)&_fields._arg1; }
	std::string arg1_as_string() const;
	std::string all_args_as_string() const;

	bool is_func_start() const			{ return (_fields._debugid & DBG_FUNC_MASK) == DBG_FUNC_START; }
	bool is_func_end() const			{ return (_fields._debugid & DBG_FUNC_MASK) == DBG_FUNC_END; }
	bool is_func_none() const			{ return (_fields._debugid & DBG_FUNC_MASK) == DBG_FUNC_NONE; }

	uint64_t unused() const				{ return _fields.unused(); }

	bool is_valid() {
		// Must have a code set to be valid, no codes are 0x00
		if (dbg_code() == 0)
			return false;

		// Legal values are NONE, START, and END.
		if ((_fields._debugid & DBG_FUNC_MASK) == DBG_FUNC_MASK)
			return false;

		return true;
	}

	std::string to_string() const;
};

template <typename SIZE>
std::string KDEvent<SIZE>::arg1_as_string() const {
	// We can't count on the arg being NULL terminated, we have to copy.
	// Using a uint32_t/uint64_t instead of a char[] guarantees alignment.
	decltype(_fields._arg1) buf[2];

	buf[0] = _fields._arg1;
	buf[1] = 0;

	return std::string(reinterpret_cast<char*>(buf));
}

template <typename SIZE>
std::string KDEvent<SIZE>::all_args_as_string() const {
	// We can't count on the arg being NULL terminated, we have to copy.
	// Using a uint32_t/uint64_t instead of a char[] guarantees alignment.
	decltype(_fields._arg1) buf[5];

	buf[0] = _fields._arg1;
	buf[1] = _fields._arg2;
	buf[2] = _fields._arg3;
	buf[3] = _fields._arg4;
	buf[4] = 0;

	return std::string(reinterpret_cast<char*>(buf));
}

