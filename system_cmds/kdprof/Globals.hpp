//
//  Globals.hpp
//  kdprof
//
//  Created by James McIlree on 4/17/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef kdprof_Globals_hpp
#define kdprof_Globals_hpp

//
// These are "global" values that control parsing and printing behavior.
//

enum class kSortKey : std::uint32_t {
	CPU=0,
	VMFault,
	IO_Ops,
	IO_Size,
	IO_Wait,
	ID
};
	
class Globals {
    protected:
	// Default/unknown parsing values
	uint32_t					_cpu_count;
	uint32_t					_iop_count;
	KernelSize					_kernel_size;
	std::string					_summary_start;
	std::string					_summary_stop;
	std::string					_summary_step;

	bool						_is_cpu_count_set;
	bool						_is_iop_count_set;
	bool						_is_kernel_size_set;
	bool						_is_summary_start_set;
	bool						_is_summary_stop_set;
	bool						_is_summary_step_set;
	bool						_is_should_print_summary_set;
	bool						_is_timebase_set;

	// Output, printing related.
	AbsTime						_beginning_of_time;
	mach_timebase_info_data_t			_timebase_info;
	FileDescriptor					_output_fd;
	bool						_should_read_default_trace_codes;
	std::vector<std::string>			_additional_trace_code_paths;
	std::unordered_map<uint32_t, std::string>	_trace_codes;
	bool						_should_print_mach_absolute_timestamps;
	bool						_should_print_event_index;
	bool						_should_print_symbolic_event_codes;
	bool						_is_verbose;
	bool						_should_presort_events;
	bool						_should_print_cpu_summaries;
	bool						_should_print_process_summaries;
	bool						_should_print_thread_summaries;
	bool						_should_print_events;
	bool						_should_print_summary;
	bool						_should_zero_base_timestamps;
	bool						_should_print_process_start_stop_timestamps;
	bool						_should_print_csv_summary;
	kSortKey					_sort_key;

	AbsTime parse_time(const char* arg) const;
	
    public:
	Globals();

	uint32_t cpu_count() const						{ return _cpu_count; }
	void set_cpu_count(uint32_t num)					{ _cpu_count = num; _is_cpu_count_set = true; }
	bool is_cpu_count_set() const						{ return _is_cpu_count_set; }

	uint32_t iop_count() const						{ return _iop_count; }
	void set_iop_count(uint32_t num)					{ _iop_count = num; _is_iop_count_set = true; }
	bool is_iop_count_set() const						{ return _is_iop_count_set; }

	KernelSize kernel_size() const						{ return _kernel_size; }
	void set_kernel_size(KernelSize size)					{ _kernel_size = size; _is_kernel_size_set = true; }
	bool is_kernel_size_set() const						{ return _is_kernel_size_set; }

	AbsTime beginning_of_time() const					{ return _beginning_of_time; }
	void set_beginning_of_time(AbsTime t)					{ _beginning_of_time = t; }

	mach_timebase_info_data_t timebase() const				{ return _timebase_info; }
	void set_timebase(mach_timebase_info_data_t timebase, bool is_user_set)	{ _timebase_info = timebase; if (is_user_set) _is_timebase_set = true; }
	bool is_timebase_set() const						{ return _is_timebase_set; }
	
	int output_fd() const							{ return _output_fd.is_open() ? (int)_output_fd : STDOUT_FILENO; }

	// Okay, this method caused enough pain to make the final resolution worth a comment.
	//
	// http://thbecker.net/articles/rvalue_references/section_05.html
	//
	// Things that are declared as rvalue reference can be lvalues or rvalues.
	// The distinguishing criterion is: if it has a name, then it is an lvalue. Otherwise, it is an rvalue.
	//
	// In this case, you cannot call set_output_fd with an lvalue, but fd is STILL an lvalue.
	// We must still explicitly use std::move on fd!
	void set_output_fd(FileDescriptor&& fd)					{ _output_fd = std::move(fd); }

	void set_should_read_default_trace_codes(bool value)			{ _should_read_default_trace_codes = value; }
	void append_trace_codes_at_path(std::string path)			{ _additional_trace_code_paths.push_back(path); }
	void resolve_trace_codes(void)						{ _trace_codes = ::resolve_trace_codes(_should_read_default_trace_codes, _is_verbose ? 1 : -1, _additional_trace_code_paths); }
	
	const std::unordered_map<uint32_t, std::string>& trace_codes() const	{ return _trace_codes; }
	void set_trace_codes(std::unordered_map<uint32_t, std::string>&& codes)	{ _trace_codes = codes; }

	bool should_print_mach_absolute_timestamps() const			{ return _should_print_mach_absolute_timestamps; }
	void set_should_print_mach_absolute_timestamps(bool value)		{ _should_print_mach_absolute_timestamps = value; }

	bool should_print_event_index() const					{ return _should_print_event_index; }
	void set_should_print_event_index(bool value)				{ _should_print_event_index = value; }

	bool should_print_symbolic_event_codes() const				{ return _should_print_symbolic_event_codes; }
	void set_should_print_symbolic_event_codes(bool value)			{ _should_print_symbolic_event_codes = value; }

	bool is_verbose() const							{ return _is_verbose; }
	void set_is_verbose(bool value)						{ _is_verbose = value; }

	bool should_presort_events() const					{ return _should_presort_events; }
	void set_should_presort_events(bool value)				{ _should_presort_events = value; }

	bool should_print_cpu_summaries() const					{ return _should_print_cpu_summaries; }
	void set_should_print_cpu_summaries(bool value)				{ _should_print_cpu_summaries = value; }

	bool should_print_process_summaries() const				{ return _should_print_process_summaries; }
	void set_should_print_process_summaries(bool value)			{ _should_print_process_summaries = value; }

	bool should_print_thread_summaries() const				{ return _should_print_thread_summaries; }
	void set_should_print_thread_summaries(bool value)			{ _should_print_thread_summaries = value; }

	bool should_print_events() const					{ return _should_print_events; }
	void set_should_print_events(bool value)				{ _should_print_events = value; }

	bool should_print_summary() const					{ return _should_print_summary; }
	void set_should_print_summary(bool value)				{ _should_print_summary = value; _is_should_print_summary_set = true; }
	bool is_should_print_summary_set() const				{ return _is_should_print_summary_set; }
	
	bool should_zero_base_timestamps() const				{ return _should_zero_base_timestamps; }
	void set_should_zero_base_timestamps(bool value)			{ _should_zero_base_timestamps = value; }

	bool should_print_process_start_stop_timestamps() const			{ return _should_print_process_start_stop_timestamps; }
	void set_should_print_process_start_stop_timestamps(bool value)		{ _should_print_process_start_stop_timestamps = value; }

	bool should_print_csv_summary() const					{ return _should_print_csv_summary; }
	void set_should_print_csv_summary(bool value)				{ _should_print_csv_summary = value; }

	kSortKey sort_key() const						{ return _sort_key; }
	void set_sort_key(kSortKey key)						{ _sort_key = key; }

	//
	// The summary {start/stop/step} functions translate the string on the fly,
	// using the currently set timebase. They need to be fed a timespan that
	// corresponds to the Machine<SIZE>'s timespan, because the default values
	// and offsets depend on that.
	//
	// This solve the issue of the user saying --start 1234mabs at the command line
	// and getting an offset of 1234 nanoseconds on a desktop when they are looking
	// at a device file.
	//
	AbsTime summary_start(AbsInterval timespan) const;
	void set_summary_start(const char* value)				{ _summary_start = value; _is_summary_start_set = true; }
	bool is_summary_start_set() const					{ return _is_summary_start_set; }

	AbsTime summary_stop(AbsInterval timespan) const;
	void set_summary_stop(const char* value)				{ _summary_stop = value; _is_summary_stop_set = true; }
	bool is_summary_stop_set() const					{ return _is_summary_stop_set; }

	AbsTime summary_step(AbsInterval timespan) const;
	void set_summary_step(const char* value)				{ _summary_step = value; _is_summary_step_set = true; }
	bool is_summary_step_set() const					{ return _is_summary_step_set; }
};

#endif
