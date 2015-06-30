//
//  MachineThread.hpp
//  KDBG
//
//  Created by James McIlree on 10/26/12.
//  Copyright (c) 2014 Apple. All rights reserved.
//

template <typename SIZE> class MachineProcess;

enum class kMachineThreadFlag : std::uint32_t {
    CreatedByPreviousMachineState	= 0x00000001,
    CreatedByThreadMap			= 0x00000002,
    CreatedByTraceDataNewThread		= 0x00000004,
    CreatedByUnknownTidInTrace		= 0x00000008,
    CreatedByForkExecEvent		= 0x00000010,
    CreatedByExecEvent			= 0x00000020,
    IsMain				= 0x00000040,
    IsIdle				= 0x00000080,
    TraceTerminated			= 0x00000200,	// Set when a MACH_THREAD_TERMINATED is seen. This is definitive, no further trace events should reference this thread.
};

template <typename SIZE> class Machine;
template <typename SIZE> class JetsamActivity;
template <typename SIZE> class MachineVoucher;

template <typename SIZE>
class MachineThread {
  protected:
    uint32_t				_flags;
    typename SIZE::ptr_t		_tid;			// We're unlikely to ever look at K64 on a 32 bit machine, put this here as best chance to not increase struct size with useless padding bytes.
    MachineProcess<SIZE>*		_process;
    AbsInterval				_timespan;
    AbsTime				_begin_blocked;
    AbsTime				_begin_vm_fault;
    AbsTime				_begin_jetsam_activity;
    uint32_t				_begin_jetsam_activity_type;
    std::vector<AbsInterval>		_blocked;
    std::vector<AbsInterval>		_vm_faults;
    std::vector<AbsInterval>		_jetsam_activity;
    std::vector<VoucherInterval<SIZE>>	_vouchers_by_time;

    //
    // Mutable API
    //
    friend class Machine<SIZE>;
    friend class MachineProcess<SIZE>;

    MachineProcess<SIZE>& mutable_process()			{ return *_process; }

    void set_flags(kMachineThreadFlag flags)			{ _flags |= (uint32_t)flags; }
    void clear_flags(kMachineThreadFlag flags)			{ _flags &= ~(uint32_t)flags; }
    bool is_flag_set(kMachineThreadFlag flag) const		{ return (_flags & (uint32_t)flag) > 0; }

    // This can be discovered after the thread is created.
    void set_is_idle_thread();
    void set_trace_terminated(AbsTime timestamp);

    void set_voucher(MachineVoucher<SIZE>* voucher, AbsTime timestamp);

    //
    // NOTE! Unrunnable/blocked isn't quite exact; it doesn't match
    // the scheduler view of unrunnable/blocked.
    //
    // 1) If you're not blocked, you're runnable
    // 2) A thread is considered "blocked" if the cpu it is on goes idle.
    //
    void make_runnable(AbsTime timestamp);
    void make_unrunnable(AbsTime timestamp);

    void begin_vm_fault(AbsTime timestamp);
    void end_vm_fault(AbsTime timestamp);

    void begin_jetsam_activity(uint32_t type, AbsTime timestamp);
    void end_jetsam_activity(uint32_t type, AbsTime timestamp);

    void add_io_activity(AbsInterval interval, uint32_t code, uint32_t page_count);

    AbsTime blocked_in_timespan(AbsInterval timespan) const;
    AbsTime next_blocked_after(AbsTime timestamp) const;

    // This is called after all events have been processed, to flush any pending state
    void post_initialize(AbsTime last_machine_timestamp);

  public:
    MachineThread(MachineProcess<SIZE>* process, typename SIZE::ptr_t tid, MachineVoucher<SIZE>* initial_voucher, AbsTime create_timestamp, kMachineThreadFlag flags) :
	_flags((uint32_t)flags),
	_tid(tid),
	_process(process),
	_timespan(create_timestamp, AbsTime(0)),
	_begin_jetsam_activity_type(0)
    {
	ASSERT(_tid != 0, "Sanity");
	ASSERT(_process, "Sanity");
	ASSERT(initial_voucher, "Sanity");

	_vouchers_by_time.emplace_back(initial_voucher, AbsInterval(create_timestamp, AbsTime::END_OF_TIME - create_timestamp));
    }

    typename SIZE::ptr_t tid() const				{ return _tid; }
    AbsInterval timespan() const				{ return _timespan; }
    const MachineProcess<SIZE>& process() const			{ return *_process; }
    uint32_t flags() const					{ return _flags; }

    const MachineVoucher<SIZE>* voucher(AbsTime timestamp) const;
    const MachineVoucher<SIZE>* last_voucher() const;

    const std::vector<AbsInterval>&	vm_faults() const	{ return _vm_faults; }
    const std::vector<AbsInterval>&	jetsam_activity() const	{ return _jetsam_activity; }

    bool is_created_by_previous_machine_state() const		{ return is_flag_set(kMachineThreadFlag::CreatedByPreviousMachineState); }
    bool is_created_by_thread_map() const			{ return is_flag_set(kMachineThreadFlag::CreatedByThreadMap); }
    bool is_created_by_trace_data_new_thread() const		{ return is_flag_set(kMachineThreadFlag::CreatedByTraceDataNewThread); }
    bool is_created_by_unknown_tid_in_trace() const		{ return is_flag_set(kMachineThreadFlag::CreatedByUnknownTidInTrace); }
    bool is_created_by_fork_exec() const			{ return is_flag_set(kMachineThreadFlag::CreatedByForkExecEvent); }
    bool is_created_by_exec() const				{ return is_flag_set(kMachineThreadFlag::CreatedByExecEvent); }

    bool is_idle_thread() const					{ return is_flag_set(kMachineThreadFlag::IsIdle); }
    bool is_main_thread() const					{ return is_flag_set(kMachineThreadFlag::IsMain); }

    bool is_trace_terminated() const				{ return is_flag_set(kMachineThreadFlag::TraceTerminated); }

    DEBUG_ONLY(void validate() const;)
};
