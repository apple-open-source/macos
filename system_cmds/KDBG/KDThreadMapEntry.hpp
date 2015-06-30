//
//  KDThreadMapEntry.hpp
//  KDBG
//
//  Created by James McIlree on 10/25/12.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#ifndef kdprof_KDThreadMapEntry_hpp
#define kdprof_KDThreadMapEntry_hpp

//
// This is the kd_threadmap from the kernel
//
// There is one interesting conflict I have noticed so far.
//
// The _pid field is set to 1 for kernel threads that have no user space
// representation. However, 1 is a valid pid, and in fact, used by launchd.
//
// A full disambiguation of entries *must* include the tid, pid, AND name:
//
//    000000000000011f 00000001 launchd
//    000000000000014f 00000001 launchd
//    0000000000000150 00000001 launchd
//
//    0000000000000110 00000001 kernel_task
//    0000000000000120 00000001 kernel_task
//    0000000000000133 00000001 kernel_task
//
template <typename SIZE>
class KDThreadMapEntry {
  protected:
    typename SIZE::ptr_t	_tid;
    int32_t			_pid;
    char			_name[20]; // This is process name, not thread name!

  public:
    typename SIZE::ptr_t tid() const		{ return _tid; }
    int32_t pid() const				{ return _pid; }
    const char* name() const			{ return _name; }
};

#endif
