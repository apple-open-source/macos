//
//  MachineProcess.hpp
//  KDBG
//
//  Created by James McIlree on 10/26/12.
//  Copyright (c) 2014 Apple. All rights reserved.
//

//
// Process life cycle
//
// There are four ways a process can be created:
//
// 1) CreatedByPreviousMachineState
//
//      It is a carryover from a previous Machine state. This happens when a
//    live trace creates a machine state that is a union of a previous state
//    and new event data.
//
// 2) CreatedByThreadMap
//
//      It is a process that was running at the time the trace events were
//    taken. The kernel provides this data.
//
// 3) CreatedByExecEvent
//
//      It is a process that "reused" an existing pid, and exec'd a new process
//    in place. The Machine State will completely close down the old process
//    and create a new one to track data for the newly exec'd process.
//
// 4) CreatedByForkExecEvent
//
//      An existing process "forked", creating a new pid, and then "exec'd".
//    This is seen in trace events as a TRACE_DATA_NEWTHREAD with a pid that
//    does not match the callers pid.
//
// There are also two catch-all processes, "Unknown", and "Kernel". The kernel
// process contains all kernel only threads, and unknown contains threads that
// are encountered without any previous identifying information.
//

enum class kMachineProcessFlag : std::uint32_t {
    CreatedByPreviousMachineState	= 0x00000001,
    CreatedByThreadMap			= 0x00000002,
    CreatedByForkExecEvent		= 0x00000004,
    CreatedByExecEvent			= 0x00000008,
    IsForkExecInProgress		= 0x00000010,
    IsExecInProgress			= 0x00000020,
    IsUnknownProcess			= 0x00000040,
    IsKernelProcess			= 0x00000080,
    IsExitBySyscall			= 0x00000100,
    IsExitByJetsam			= 0x00000400,
    IsExitByExec			= 0x00000800,
    IsTraceTerminated			= 0x00001000
};

template <typename SIZE> class Machine;

template <typename SIZE>
class MachineProcess {
  protected:
    pid_t				_pid;
    char				_name[20]; // Actual limit is 16, we round up for NULL terminator
    AbsInterval				_timespan; // This is set at trace termination, or in post_initialize if still live.
    AbsTime				_exit_initiated_timestamp;
    std::vector<MachineThread<SIZE>*>	_threads_by_time;
    uint32_t				_flags;
    int32_t				_exit_status;
    int32_t				_apptype; // Unset == -1

    //
    // Mutable API
    //

    friend class Machine<SIZE>;

    void set_flags(kMachineProcessFlag flags)			{ _flags |= (uint32_t)flags; }
    void clear_flags(kMachineProcessFlag flags)			{ _flags &= ~(uint32_t)flags; }
    bool is_flag_set(kMachineProcessFlag flag) const		{ return (_flags & (uint32_t)flag) > 0; }

    //
    // Process exit lifecycle
    //
    // Processes start to exit / terminate when one of the following happens:
    //
    // syscall to proc exit
    // jetsam causes a SIGKILL
    // syscall to exec
    //
    // It may be that more than one of these events happen. For example, jetsam
    // may cause a process to die via a SIGKILL.
    //
    // For the purposes of this API, only the first method of initiating exit
    // is recorded. This includes the timestamp; if you ask for the exit timestamp
    // you will get the timestamp for the first invocation of any of the exit
    // paths.
    //
    // Once a process starts terminating, it will eventually reach the point
    // where no futher events will ever be seen for that process. When the
    // last thread in the process is marked as trace terminated, the process
    // is marked as trace terminated.
    //
    // The definitive test for a process being entirely done is trace termination.
    //
    
    //
    // The exit code and conditions are a bit of a mess.
    // All processes exit. This is triggered by the BSD_PROC_EXIT
    // tracepoint. Some processes chose to exit, some are forced to
    // exit by signals (SIGKILL, for example). Some processes are
    // forced to exit by a mechanism that appears to be a signal but
    // we want to track them separately (jetsam).
    //
    // The upshot of this is the exit code is stored in waitpid
    // style. See waitpid(2) for the macros used to decode this.
    //
    void set_exit_by_syscall(AbsTime timestamp, int exit_status);
    void set_exit_by_jetsam(AbsTime timestamp);
    void set_exit_by_exec(AbsTime timestamp);
    void set_trace_terminated(AbsTime timestamp); // Also sets last timestamp

    void set_apptype(uint32_t apptype);
    void set_apptype_from_trequested(uint32_t apptype);
    void set_name(const char* name);

    void add_thread(MachineThread<SIZE>* thread);

    bool is_exec_in_progress() const					{ return (_flags & (uint32_t)kMachineProcessFlag::IsExecInProgress) > 0; }
    bool is_fork_exec_in_progress() const				{ return (_flags & (uint32_t)kMachineProcessFlag::IsForkExecInProgress) > 0; }

    void clear_fork_exec_in_progress();
    void clear_exec_in_progress();

    // This is called after all events have been processed, to allow the
    // threads to be sorted.
    void post_initialize(AbsTime last_machine_timestamp);

  public:
    MachineProcess(pid_t pid,
		   const char* name,
		   AbsTime create_timestamp,
		   kMachineProcessFlag flags);

    pid_t pid() const							{ return _pid; }
    const char* name() const						{ return _name; }
    AbsInterval timespan() const					{ return _timespan; }
    AbsTime exit_timestamp() const					{ return _exit_initiated_timestamp; }
    int32_t exit_status() const						{ return _exit_status; }
    int32_t apptype() const						{ return _apptype; }

    uint32_t flags() const						{ return _flags; }

    const std::vector<const MachineThread<SIZE>*>& threads() const	{ return *reinterpret_cast<const std::vector<const MachineThread<SIZE>*>*>(&_threads_by_time); }

    bool is_exit_by_syscall() const					{ return is_flag_set(kMachineProcessFlag::IsExitBySyscall); }
    bool is_exit_by_jetsam() const					{ return is_flag_set(kMachineProcessFlag::IsExitByJetsam); }
    bool is_exit_by_exec() const					{ return is_flag_set(kMachineProcessFlag::IsExitByExec); }
    
									// The invariant is that trace_terminated may not be set without is_exiting() set
    bool is_exiting() const						{ return is_exit_by_syscall() || is_exit_by_jetsam() || is_exit_by_exec(); }
    bool is_trace_terminated() const					{ return is_flag_set(kMachineProcessFlag::IsTraceTerminated); }

    bool is_unknown() const						{ return is_flag_set(kMachineProcessFlag::IsUnknownProcess); }
    bool is_kernel() const						{ return is_flag_set(kMachineProcessFlag::IsKernelProcess); }

    bool is_created_by_previous_machine_state() const			{ return is_flag_set(kMachineProcessFlag::CreatedByPreviousMachineState); }
    bool is_created_by_thread_map() const				{ return is_flag_set(kMachineProcessFlag::CreatedByThreadMap); }
    bool is_created_by_fork_exec() const				{ return is_flag_set(kMachineProcessFlag::CreatedByForkExecEvent); }
    bool is_created_by_exec() const					{ return is_flag_set(kMachineProcessFlag::CreatedByExecEvent); }

    DEBUG_ONLY(void validate() const;)
};

