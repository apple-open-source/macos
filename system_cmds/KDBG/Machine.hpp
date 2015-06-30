//
//  Machine.hpp
//  KDBG
//
//  Created by James McIlree on 10/25/12.
//  Copyright (c) 2014 Apple. All rights reserved.
//

enum class kMachineFlag : std::uint32_t {
	LostEvents			= 0x00000001
};

template <typename SIZE>
class Machine {
    protected:
	std::vector<MachineCPU<SIZE>>									_cpus;

	std::unordered_multimap<pid_t, MachineProcess<SIZE>>						_processes_by_pid;
	std::unordered_multimap<const char*, MachineProcess<SIZE>*, ConstCharHash, ConstCharEqualTo>	_processes_by_name;
	std::vector<MachineProcess<SIZE>*>								_processes_by_time;

	std::unordered_multimap<typename SIZE::ptr_t, MachineThread<SIZE> >				_threads_by_tid;
	std::vector<MachineThread<SIZE>*>								_threads_by_time;

	std::vector<MachineMachMsg<SIZE>>								_mach_msgs;
	std::unordered_map<uintptr_t, uintptr_t>							_mach_msgs_by_event_index;
	std::unordered_map<typename SIZE::ptr_t, NurseryMachMsg<SIZE>>					_mach_msg_nursery;

	//
	// Vouchers are a bit special. We install pointers to vouchers in
	// MachineThreads and MachineMachMsg. This means that vouchers cannot
	// be moved once allocated. We could do two passes to exactly size
	// the data structures, this should be investigated in the future.
	//
	// On create or first observed use, a voucher goes into the nursery.
	// It stays there until a destroy event, or the end of Machine events.
	// Once flushed from the nursery, we have a map of addresses, which
	// points to a vector sorted by time. This allows addr @ time lookups
	// later.
	//
	std::unordered_map<typename SIZE::ptr_t, std::unique_ptr<MachineVoucher<SIZE>>>			_voucher_nursery;
        std::unordered_map<typename SIZE::ptr_t, std::vector<std::unique_ptr<MachineVoucher<SIZE>>>>	_vouchers_by_addr;

	std::unordered_map<typename SIZE::ptr_t, IOActivity<SIZE>>					_io_by_uid; // uid == unique id, not user id
	std::vector<IOActivity<SIZE>>									_all_io;
	std::vector<AbsInterval>									_all_io_active_intervals;

	MachineProcess<SIZE>*										_kernel_task;
	const KDEvent<SIZE>*										_events;
	uintptr_t											_event_count;
	uint32_t											_flags;
	int32_t												_unknown_process_pid; // We need unique negative pid's for previously unknown TID's

	//
	// Protected initialization code
	//
	void raw_initialize(const KDCPUMapEntry* cpumaps,
			    uint32_t cpumap_count,
			    const KDThreadMapEntry<SIZE>* threadmaps,
			    uint32_t threadmap_count,
			    const KDEvent<SIZE>* events,
			    uintptr_t event_count);

	void post_initialize();

	//
	// Mutable API, for use during construction
	//
	
	pid_t next_unknown_pid()									{ return --_unknown_process_pid; }
	
	MachineProcess<SIZE>* create_process(pid_t pid, const char* name, AbsTime create_timestamp, kMachineProcessFlag flags);
	MachineThread<SIZE>* create_thread(MachineProcess<SIZE>* process, typename SIZE::ptr_t tid, MachineVoucher<SIZE>* voucher, AbsTime create_timestamp, kMachineThreadFlag flags);
	MachineVoucher<SIZE>* create_voucher(typename SIZE::ptr_t address, AbsTime create_timestamp, kMachineVoucherFlag flags, uint32_t content_bytes_capacity);

	void destroy_voucher(typename SIZE::ptr_t address, AbsTime timestamp);

	void set_flags(kMachineFlag flag)								{ _flags |= (uint32_t)flag; }
	
	void set_process_name(MachineProcess<SIZE>* process, const char* name);
	
	MachineProcess<SIZE>* mutable_process(pid_t pid, AbsTime time)					{ return const_cast<MachineProcess<SIZE>*>(process(pid, time)); }
	MachineThread<SIZE>* mutable_thread(typename SIZE::ptr_t tid, AbsTime time)			{ return const_cast<MachineThread<SIZE>*>(thread(tid, time)); }
	
	MachineProcess<SIZE>* youngest_mutable_process(pid_t pid);
	MachineThread<SIZE>* youngest_mutable_thread(typename SIZE::ptr_t tid);

	MachineVoucher<SIZE>* process_event_voucher_lookup(typename SIZE::ptr_t address, uint32_t msgh_bits);
	MachineThread<SIZE>* process_event_tid_lookup(typename SIZE::ptr_t tid, AbsTime now);

	MachineVoucher<SIZE>* thread_forwarding_voucher_lookup(const MachineVoucher<SIZE>* original_thread_voucher);

	void begin_io(MachineThread<SIZE>* thread, AbsTime time, typename SIZE::ptr_t uid, typename SIZE::ptr_t size);
	void end_io(AbsTime time, typename SIZE::ptr_t uid);

	bool process_event(const KDEvent<SIZE>& event);
	void process_trequested_task(pid_t pid, typename SIZE::ptr_t trequested_0, typename SIZE::ptr_t trequested_1);
	void process_trequested_thread(typename SIZE::ptr_t tid, typename SIZE::ptr_t trequested_0, typename SIZE::ptr_t trequested_1);

	void initialize_cpu_idle_intr_states();

    public:
	static MachineVoucher<SIZE> UnsetVoucher;
	static MachineVoucher<SIZE> NullVoucher;

	Machine(KDCPUMapEntry* cpumaps, uint32_t cpumap_count, KDThreadMapEntry<SIZE>* threadmaps, uint32_t threadmap_count, KDEvent<SIZE>* events, uintptr_t event_count);
	// Destructive, mutates parent!
	Machine(Machine<SIZE>& parent, KDEvent<SIZE>* events, uintptr_t event_count);
	Machine(const TraceFile& file);

	bool lost_events() const									{ return (_flags & (uint32_t)kMachineFlag::LostEvents) > 0; }
	
	const MachineProcess<SIZE>* process(pid_t pid, AbsTime time) const;
	const MachineThread<SIZE>* thread(typename SIZE::ptr_t tid, AbsTime time) const;
	const MachineVoucher<SIZE>* voucher(typename SIZE::ptr_t address, AbsTime time) const;
	const MachineMachMsg<SIZE>* mach_msg(uintptr_t event_index) const;

	const std::vector<const MachineProcess<SIZE>*>& processes() const;
	const std::vector<const MachineThread<SIZE>*>& threads() const;
	const std::vector<const MachineCPU<SIZE>>& cpus() const;

	const KDEvent<SIZE>* events() const								{ return _events; }
	uintptr_t event_count() const									{ return _event_count; }

	AbsInterval timespan() const;

	// Returns the number of cpus that have timeline data.
	// (IOW, typically the number of AP(s) on a machine, but might be less if you've disabled some so they generate no trace data)
	uint32_t active_cpus() const;

	// If summary_cpu == NULL , all cpus are matched.
	CPUSummary<SIZE> summary_for_timespan(AbsInterval timespan, const MachineCPU<SIZE>* summary_cpu) const;

	// This attempts to analyze various pieces of data and guess
	// if the Machine represents an ios device or not.
	bool is_ios() const;

	DEBUG_ONLY(void validate() const;)
};

template <typename SIZE> MachineVoucher<SIZE> Machine<SIZE>::UnsetVoucher(SIZE::PTRMAX, AbsInterval(AbsTime(0),AbsTime(UINT64_MAX)), kMachineVoucherFlag::IsUnsetVoucher, 0);
template <typename SIZE> MachineVoucher<SIZE> Machine<SIZE>::NullVoucher(0, AbsInterval(AbsTime(0),AbsTime(UINT64_MAX)), kMachineVoucherFlag::IsNullVoucher, 0);

