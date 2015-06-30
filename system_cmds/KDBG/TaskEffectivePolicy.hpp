//
//  TaskEffectivePolicy.hpp
//  system_cmds
//
//  Created by James McIlree on 6/19/14.
//
//

// Using the raw struct was causing alignment issues on arm32; it seems that
// structs have very relaxed alignment requirements on the v7 architectures.
// The class wrapper forces a higher alignment and allows for some convenience
// operators (compare, xor, etc)

class TaskEffectivePolicy {
    protected:
	union {
		Kernel32::ptr_t _kernel_32[2];
		Kernel64::ptr_t _kernel_64;
		struct task_effective_policy _policy;
	} _content;

    public:
	TaskEffectivePolicy() {}

	TaskEffectivePolicy(struct task_effective_policy policy) {
		static_assert(sizeof(_content) == sizeof(struct task_effective_policy), "Sanity");
		_content._policy = policy;
	}

	TaskEffectivePolicy(Kernel64::ptr_t teffective_0) {
		static_assert(sizeof(_content) == sizeof(teffective_0), "Sanity");
		_content._kernel_64 = teffective_0;
	}

	TaskEffectivePolicy(Kernel32::ptr_t teffective_0, Kernel32::ptr_t teffective_1) {
		static_assert(sizeof(_content) == (sizeof(teffective_0) + sizeof(teffective_1)), "Sanity");
		_content._kernel_32[0] = teffective_0;
		_content._kernel_32[1] = teffective_1;
	}

	bool operator==(const TaskEffectivePolicy& other) const			{ return this->_content._kernel_64 == other._content._kernel_64; }
	bool operator!=(const TaskEffectivePolicy& other) const	 		{ return !(*this == other); }

	TaskEffectivePolicy operator~() const					{ return TaskEffectivePolicy(~this->_content._kernel_64); }

	struct task_effective_policy as_struct() { return _content._policy; }
};