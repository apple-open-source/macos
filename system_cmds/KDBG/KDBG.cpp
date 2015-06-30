//
//  Kernel.cpp
//  KDBG
//
//  Created by James McIlree on 10/24/12.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#include <CPPUtil/CPPUtil.h>

using namespace util;

#include "KDebug.h"

KDState KDBG::state()
{
    static_assert(sizeof(KDState) == sizeof(kbufinfo_t), "Types must be the same size");

    KDState state;
    int mib[3];
    size_t len = sizeof(state);

    mib[0] = CTL_KERN;
    mib[1] = KERN_KDEBUG;
    mib[2] = KERN_KDGETBUF;

    if (sysctl(mib, 3, &state, &len, 0, 0) < 0) {
	DEBUG_ONLY(log_msg(ASL_LEVEL_ERR, "trace facility failure, KERN_KDGETBUF: %s\n", strerror(errno)));
	THROW("trace facility failure, KERN_KDGETBUF: %s\n" << strerror(errno));
    }

    return state;
}

bool KDBG::reset()
{
    int mib[3];

    mib[0] = CTL_KERN;
    mib[1] = KERN_KDEBUG;
    mib[2] = KERN_KDREMOVE;
    if (sysctl(mib, 3, NULL, NULL, NULL, 0) < 0) {
	DEBUG_ONLY(log_msg(ASL_LEVEL_WARNING, "trace facility failure, KERN_KDREMOVE: %s\n", strerror(errno)));
	return false;
    }

    return true;
}

bool KDBG::set_buffer_capacity(uint32_t capacity)
{
    int mib[4];

    mib[0] = CTL_KERN;
    mib[1] = KERN_KDEBUG;
    mib[2] = KERN_KDSETBUF;
    mib[3] = (int)capacity;

    if (sysctl(mib, 4, NULL, NULL, NULL, 0) < 0) {
	DEBUG_ONLY(log_msg(ASL_LEVEL_WARNING, "trace facility failure, KERN_KDSETBUF: %s\n", strerror(errno)));
	return false;
    }

    return true;
}

bool KDBG::set_nowrap(bool is_nowrap)
{
    int mib[4];

    mib[0] = CTL_KERN;
    mib[1] = KERN_KDEBUG;
    mib[2] = is_nowrap ? KERN_KDEFLAGS : KERN_KDDFLAGS;
    mib[3] = KDBG_NOWRAP;

    if (sysctl(mib, 4, NULL, NULL, NULL, 0) < 0) {
	DEBUG_ONLY(log_msg(ASL_LEVEL_WARNING, "trace facility failure, KDBG_NOWRAP: %s\n", strerror(errno)));
	return false;
    }

    return true;
}

bool KDBG::initialize_buffers()
{
    int mib[3];

    mib[0] = CTL_KERN;
    mib[1] = KERN_KDEBUG;
    mib[2] = KERN_KDSETUP;

    if (sysctl(mib, 3, NULL, NULL, NULL, 0) < 0) {
	DEBUG_ONLY(log_msg(ASL_LEVEL_WARNING, "trace facility failure, KERN_KDSETUP: %s\n", strerror(errno)));
	return false;
    }
    return true;
}


//
// Legal values are:
//
// KDEBUG_TRACE (full set of tracepoints)
// KDEBUG_PPT (subset of tracepoints to minimize performance impact)
// 0 (Disable)
//
bool KDBG::set_enabled(uint32_t value)
{
    int mib[4];

    mib[0] = CTL_KERN;
    mib[1] = KERN_KDEBUG;
    mib[2] = KERN_KDENABLE;
    mib[3] = value;

    if (sysctl(mib, 4, NULL, NULL, NULL, 0) < 0) {
	DEBUG_ONLY(log_msg(ASL_LEVEL_WARNING, "trace facility failure, KERN_KDENABLE: %s\n", strerror(errno)));
	return false;
    }
    return true;
}

std::vector<KDCPUMapEntry> KDBG::cpumap()
{
	std::vector<KDCPUMapEntry> cpumap;

	/*
	 * To fit in the padding space of a VERSION1 file, the max possible
	 * cpumap size is one page.
	 */
	if (kd_cpumap_header* cpumap_header = (kd_cpumap_header*)malloc(PAGE_SIZE)) {
		int mib[3];
		mib[0] = CTL_KERN;
		mib[1] = KERN_KDEBUG;
		mib[2] = KERN_KDCPUMAP;

		size_t temp = PAGE_SIZE;
		if (sysctl(mib, 3, cpumap_header, &temp, NULL, 0) == 0) {
			if (PAGE_SIZE >= temp) {
				if (cpumap_header->version_no == RAW_VERSION1) {
					cpumap.resize(cpumap_header->cpu_count);
					memcpy(cpumap.data(), &cpumap_header[1], cpumap_header->cpu_count * sizeof(KDCPUMapEntry));
				}
			}
		}
		free(cpumap_header);
	}

	return cpumap;
}

bool KDBG::write_maps(int fd)
{
	int mib[4];

        mib[0] = CTL_KERN;
        mib[1] = KERN_KDEBUG;
        mib[2] = KERN_KDWRITEMAP;
        mib[3] = fd;

        if (sysctl(mib, 4, NULL, NULL, NULL, 0) < 0)
                return false;
        
        return true;
}

int KDBG::write_events(int fd)
{
	int mib[4];
	size_t events_written = 0;

	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDWRITETR;
	mib[3] = fd;

	if (sysctl(mib, 4, NULL, &events_written, NULL, 0) < 0)
		return -1;

	return (int)events_written;
}
