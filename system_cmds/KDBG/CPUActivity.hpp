//
//  CPUActivity.hpp
//  KDBG
//
//  Created by James McIlree on 4/22/13.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#ifndef kdprof_CPUActivity_hpp
#define kdprof_CPUActivity_hpp

// NOTE! Counted, not bits!
enum class kCPUActivity : uint32_t {
	Unknown	= 0x00000001,
	Idle	= 0x00000002,
	INTR	= 0x00000003,
	Run	= 0x00000004 // *MUST* be the last definition. See "is_run()"
};

template <typename SIZE>
class CPUActivity : public AbsInterval {

	// Declaring this a union to make the behavior clearer.
	//
	// If _type > kCPUActivity::Run, the _thread portion of
	// the union is valid, the _type is coonsider to be Run.
	//
	// However, if the _thread is valid, the low order bit of
	// the thread indicates if this was a context switch.
	//
	// So:
	//
	// 0000000X     == _type;
	// XXXXXXX[0/1] == _thread;

	union {
		MachineThread<SIZE>*	thread;
		uintptr_t		type;
	} _u;

	enum {
		kCPUActivityRunIsContextSwitch = 0x1
	};

    public:
	CPUActivity(kCPUActivity type, AbsInterval interval) :
		AbsInterval(interval)
	{
		ASSERT(type != kCPUActivity::Run, "Cannot be Run without a thread");
		_u.type = (uintptr_t)type;
	}

	CPUActivity(MachineThread<SIZE>* thread, AbsInterval interval, bool is_cntx_swtch) :
		AbsInterval(interval)
	{
		_u.thread = thread;
		if (is_cntx_swtch)
			_u.type |= kCPUActivityRunIsContextSwitch;
		
		ASSERT(is_run(), "Sanity");
		ASSERT(is_context_switch() == is_cntx_swtch, "Sanity");
	}

	// We can safely assume that the memory system will never allocate
	// a thread in the first page of memory.
	bool is_run() const			{ return _u.type >  (uintptr_t)kCPUActivity::Run; }
	bool is_idle() const			{ return _u.type == (uintptr_t)kCPUActivity::Idle; }
	bool is_intr() const			{ return _u.type == (uintptr_t)kCPUActivity::INTR; }
	bool is_unknown() const			{ return _u.type == (uintptr_t)kCPUActivity::Unknown; }

	bool is_context_switch() const {
		if (is_run() && (_u.type & kCPUActivityRunIsContextSwitch))
			return true;
		return false;
	}

	kCPUActivity type() const {
		if (_u.type > (uintptr_t)kCPUActivity::Run)
			return kCPUActivity::Run;

		return (kCPUActivity)_u.type;
	}

	const MachineThread<SIZE>* thread() const {
		ASSERT(is_run(), "Sanity");
		return (MachineThread<SIZE>* )((uintptr_t)_u.thread & ~kCPUActivityRunIsContextSwitch);
	}
};

#endif
