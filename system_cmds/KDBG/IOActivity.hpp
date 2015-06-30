//
//  IOActivity.hpp
//  KDBG
//
//  Created by James McIlree on 9/2/13.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#ifndef kdprof_IOActivity_hpp
#define kdprof_IOActivity_hpp

template <typename SIZE>
class IOActivity : public TRange<AbsTime> {
    private:
	MachineThread<SIZE>*	_thread;
	typename SIZE::ptr_t	_size;

    public:
	IOActivity(AbsTime start, AbsTime length, MachineThread<SIZE>* thread, typename SIZE::ptr_t size) :
		TRange(start, length),
		_thread(thread),
		_size(size)
	{
		ASSERT(_thread, "Sanity");
		ASSERT(_size, "Zero length IO");
	}

	MachineThread<SIZE>* thread() const		{ return _thread; }
	void set_thread(MachineThread<SIZE>* thread)	{ _thread = thread; }

	typename SIZE::ptr_t size() const		{ return _size; }
};

#endif
