//
//  TaskRequestedPolicy.hpp
//  system_cmds
//
//  Created by James McIlree on 6/23/14.
//
//


// Using the raw struct was causing alignment issues on arm32; it seems that
// structs have very relaxed alignment requirements on the v7 architectures.
// The class wrapper forces a higher alignment and allows for some convenience
// operators (compare, xor, etc)

class TaskRequestedPolicy {
    protected:
	union {
		Kernel32::ptr_t _kernel_32[2];
		Kernel64::ptr_t _kernel_64;
		struct task_requested_policy _policy;
	} _content;

    public:
	TaskRequestedPolicy() {}

	TaskRequestedPolicy(struct task_requested_policy policy) {
		static_assert(sizeof(_content) == sizeof(struct task_requested_policy), "Sanity");
		_content._policy = policy;
	}

	TaskRequestedPolicy(Kernel64::ptr_t trequested_0) {
		static_assert(sizeof(_content) == sizeof(trequested_0), "Sanity");
		_content._kernel_64 = trequested_0;
	}

	TaskRequestedPolicy(Kernel32::ptr_t trequested_0, Kernel32::ptr_t trequested_1) {
		static_assert(sizeof(_content) == (sizeof(trequested_0) + sizeof(trequested_1)), "Sanity");
		_content._kernel_32[0] = trequested_0;
		_content._kernel_32[1] = trequested_1;
	}

	bool operator==(const TaskRequestedPolicy& other) const			{ return this->_content._kernel_64 == other._content._kernel_64; }
	bool operator!=(const TaskRequestedPolicy& other) const	 		{ return !(*this == other); }

	TaskRequestedPolicy operator~() const					{ return TaskRequestedPolicy(~this->_content._kernel_64); }

	struct task_requested_policy as_struct() { return _content._policy; }
};