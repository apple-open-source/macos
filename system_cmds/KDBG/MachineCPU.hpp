//
//  MachineCPU.hpp
//  KDBG
//
//  Created by James McIlree on 10/26/12.
//  Copyright (c) 2014 Apple. All rights reserved.
//

//
// Much simplified cpu/thread state model.
//
// 1) A thread is *always* on cpu. Always. The only excpetion is during
//    initialization, when the thread on cpu is unknown.
//
// 2) There are three states possible: Running, IDLE, INTR.
//    A thread may move from any state to any other state.
//    A thread may not take an INTR while in INTR.
//    It is illegal to context_switch in IDLE or INTR.
//

enum class kMachineCPUFlag : std::uint32_t {
	IsStateIdleInitialized				= 0x00000001,	// Set when the idle state at event[0] has been identified.
	IsStateINTRInitialized				= 0x00000002,	// Set when the INTR state at event[0] has been identified.
	IsStateThreadInitialized			= 0x00000004,   // Set when the on-cpu thread at event[0] has been identified (may be NULL, for threads not known at the time of event[0])
	IsStateIdle					= 0x00000008,	// Set if the cpu is Idle
	IsStateINTR					= 0x00000010,	// Set if the cpu is servicing an interrupt
	IsStateDeactivatedForcedSwitchToIdleThread	= 0x00000020,	// OSX only; set when the cpu is deactivated and on wake forces a switch to its idle thread without a context switch tracepoint
	IsIOP						= 0x10000000	// Set if the cpu is an IOP
};

template <typename SIZE>
class MachineCPU {
    protected:

	class ThreadOnCPU {
	    protected:
		MachineThread<SIZE>*	_thread;
		AbsTime			_timestamp;
	    public:
		ThreadOnCPU(MachineThread<SIZE>* thread, bool is_event_zero_init_thread, AbsTime timestamp) :
			_thread(thread),
			_timestamp(timestamp)
		{
			if (is_event_zero_init_thread)
				_thread = (MachineThread<SIZE>*)((uintptr_t)_thread | 1);
		}

		MachineThread<SIZE>* thread()		{ return (MachineThread<SIZE>*)((uintptr_t)_thread & ~0x1); }
		AbsTime	timestamp()			{ return _timestamp; }
		bool is_event_zero_init_thread()	{ return (uintptr_t)_thread & 0x1; }
	};

	int32_t						_id;
	uint32_t					_flags;
	std::string					_name; // IOP's have names, AP's will be "AP"
	std::vector<CPUActivity<SIZE>>			_timeline;

	// State used only during initialization
	MachineThread<SIZE>*				_thread;
	AbsTime						_begin_idle;
	AbsTime						_begin_intr;
	std::vector<ThreadOnCPU>			_cpu_runq;
	std::vector<AbsInterval>			_cpu_intr;
	std::vector<AbsInterval>			_cpu_idle;

	friend class Machine<SIZE>;

	bool is_running() const					{ return (_flags & ((uint32_t)kMachineCPUFlag::IsStateIdle | (uint32_t)kMachineCPUFlag::IsStateINTR)) == 0; }
	
	bool is_idle() const					{ return (_flags & (uint32_t)kMachineCPUFlag::IsStateIdle) > 0; }
	void set_idle(AbsTime timestamp);
	void clear_idle(AbsTime timestamp);

	bool is_deactivate_switch_to_idle_thread() const	{ return (_flags & (uint32_t)kMachineCPUFlag::IsStateDeactivatedForcedSwitchToIdleThread) > 0; }
	void set_deactivate_switch_to_idle_thread();
	void clear_deactivate_switch_to_idle_thread();

	bool is_idle_state_initialized() const			{ return (_flags & (uint32_t)kMachineCPUFlag::IsStateIdleInitialized) > 0; }
	void initialize_idle_state(bool value, AbsTime timestamp);

	bool is_intr() const					{ return (_flags & (uint32_t)kMachineCPUFlag::IsStateINTR) > 0; }
	void set_intr(AbsTime timestamp);
	void clear_intr(AbsTime timestamp);

	bool is_intr_state_initialized() const			{ return (_flags & (uint32_t)kMachineCPUFlag::IsStateINTRInitialized) > 0; }
	void initialize_intr_state(bool state, AbsTime timestamp);

	void context_switch(MachineThread<SIZE>* to_thread, MachineThread<SIZE>* from_thread, AbsTime timestamp);

	bool is_thread_state_initialized() const		{ return (_flags & (uint32_t)kMachineCPUFlag::IsStateThreadInitialized) > 0; }
	void initialize_thread_state(MachineThread<SIZE>* thread, AbsTime timestamp);

	MachineThread<SIZE>* thread()				{ return _thread; }

	// This is called after all events have been processed, to allow the
	// cpu timelines to be collapsed and post processed.
	void post_initialize(AbsInterval events_interval);

    public:
	MachineCPU(int32_t id, bool is_iop, std::string name) :
		_id(id),
		_flags(is_iop ? (uint32_t)kMachineCPUFlag::IsIOP : 0),
		_name(name),
		_thread(nullptr),
		_begin_idle(AbsTime(0)),
		_begin_intr(AbsTime(0))
	{
	}
	
	int32_t id() const				{ return _id; }
	void set_id(int32_t id)				{ ASSERT(_id = -1, "Attempt to set id twice"); _id = id; }

	bool is_iop() const				{ return (_flags & (uint32_t)kMachineCPUFlag::IsIOP) > 0; }

	bool is_active() const				{ return !_timeline.empty(); }

	const char* name() const			{ return _name.c_str(); }

	const std::vector<CPUActivity<SIZE>>& timeline() const	{ return _timeline; }
	const CPUActivity<SIZE>* activity_for_timestamp(AbsTime timestamp) const;
};
