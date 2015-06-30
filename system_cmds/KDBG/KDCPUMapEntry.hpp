//
//  KDCPUMapEntry.hpp
//  KDBG
//
//  Created by James McIlree on 4/18/13.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#ifndef kdprof_KDCPUMapEntry_hpp
#define kdprof_KDCPUMapEntry_hpp

class KDCPUMapEntry {
    protected:
	uint32_t	_cpu_id;
	uint32_t	_flags;
	char		_name[8];

    public:
	KDCPUMapEntry() {} // Default constructor must do nothing, so vector resizes do no work!
	KDCPUMapEntry(uint32_t cpu_id, uint32_t flags, const char* cpu_name) :
		_cpu_id(cpu_id),
		_flags(flags)
	{
		ASSERT(cpu_name, "Sanity");
		ASSERT(strlen(cpu_name) < sizeof(_name), "Name too long");
		strlcpy(_name, cpu_name, sizeof(_name));
	}
	
	uint32_t cpu_id() const		{ return _cpu_id; }
	uint32_t flags() const		{ return _flags; }
	const char* name() const	{ return _name; }

	bool is_iop() const		{ return _flags & KDBG_CPUMAP_IS_IOP; }
};

#endif
