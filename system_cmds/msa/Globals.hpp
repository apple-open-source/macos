//
//  Globals.hpp
//  msa
//
//  Created by James McIlree on 4/17/13.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#ifndef msa_Globals_hpp
#define msa_Globals_hpp

//
// These are "global" values that control parsing and printing behavior.
//

enum class kLifecycleFilter : std::uint32_t {
	None = 0,
	User,
	All
};

enum class kMachMsgFilter : std::uint32_t {
	None = 0,
	User,
	Voucher,
	All
};

class Globals {
    protected:
	// Default/unknown parsing values
	uint32_t					_cpu_count;
	uint32_t					_iop_count;
	KernelSize					_kernel_size;
	std::string					_live_update_interval;

	bool						_is_cpu_count_set;
	bool						_is_iop_count_set;
	bool						_is_kernel_size_set;
	bool						_is_timebase_set;

	// Output, printing related.
	AbsTime						_beginning_of_time;
	mach_timebase_info_data_t			_timebase_info;
	FileDescriptor					_output_fd;
	bool						_should_print_mach_absolute_timestamps;
	bool						_should_print_event_index;
	bool						_is_verbose;
	bool						_should_presort_events;
	bool						_should_zero_base_timestamps;
	bool						_should_trace_voucher_contents;
	uint32_t					_trace_buffer_size;
	kLifecycleFilter				_lifecycle_filter;
	kMachMsgFilter					_mach_msg_filter;


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

	bool should_print_mach_absolute_timestamps() const			{ return _should_print_mach_absolute_timestamps; }
	void set_should_print_mach_absolute_timestamps(bool value)		{ _should_print_mach_absolute_timestamps = value; }

	bool should_print_event_index() const					{ return _should_print_event_index; }
	void set_should_print_event_index(bool value)				{ _should_print_event_index = value; }

	bool is_verbose() const							{ return _is_verbose; }
	void set_is_verbose(bool value)						{ _is_verbose = value; }

	bool should_presort_events() const					{ return _should_presort_events; }
	void set_should_presort_events(bool value)				{ _should_presort_events = value; }

	bool should_zero_base_timestamps() const				{ return _should_zero_base_timestamps; }
	void set_should_zero_base_timestamps(bool value)			{ _should_zero_base_timestamps = value; }

	bool should_trace_voucher_contents() const				{ return _should_trace_voucher_contents; }
	void set_should_trace_voucher_contents(bool value)			{ _should_trace_voucher_contents = value; }

	uint32_t trace_buffer_size() const					{ return _trace_buffer_size; }
	void set_trace_buffer_size(uint32_t value)				{ _trace_buffer_size = value; }

	AbsTime live_update_interval() const;
	void set_live_update_interval(const char* value)			{ _live_update_interval = value; }

	kLifecycleFilter lifecycle_filter() const				{ return _lifecycle_filter; }
	void set_lifecycle_filter(kLifecycleFilter value)			{ _lifecycle_filter = value; }

	kMachMsgFilter mach_msg_filter() const					{ return _mach_msg_filter; }
	void set_mach_msg_filter(kMachMsgFilter value)				{ _mach_msg_filter = value; }
};

#endif
