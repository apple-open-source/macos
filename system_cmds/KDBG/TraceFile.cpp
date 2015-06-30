//
//  TraceFile.cpp
//  system_cmds
//
//  Created by James McIlree on 4/1/14.
//
//

#include "KDebug.h"

TraceFile::TraceFile(const char* path, bool sort_events, uint32_t default_ap_count, uint32_t default_iop_count) :
	_file(path),
	_version(kTraceFileVersion::Unknown),
	_is_64_bit(false),
	_is_valid(false),
	_threadmap(nullptr),
	_threadmap_count(0),
	_cpumap(nullptr),
	_cpumap_count(0),
	_events(nullptr),
	_event_count(0)
{
	try {
		parse<Kernel64>(sort_events, default_ap_count, default_iop_count);
	} catch (...) {
		parse<Kernel32>(sort_events, default_ap_count, default_iop_count);
	}
}
