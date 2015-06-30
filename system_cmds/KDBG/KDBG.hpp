//
//  KDBG
//  KDBG
//
//  Created by James McIlree on 10/24/12.
//  Copyright (c) 2014 Apple. All rights reserved.
//

class KDBG {
    private:

	//    KERN_KDGETBUF == fill out kd_bufinfo struct. Tolerates undersize input structs to fetch less.
	//    KERN_KDSETBUF == set nkdbufs (kernel variable)
	//    KERN_KDSETUP == dealloc old buffers, alloc new ones
	//    KERN_KDEFLAGS == "Enable Flags" (masked against KDBG_USERFLAGS)
	//    KERN_KDDFLAGS == "Disable Flags" (masked against KDBG_USERFLAGS)
	//    KERN_KDENABLE == Actually turn on/off tracing
	//    KERN_KDSETREG == Set some kind of filtering.


	//    KERN_KDREMOVE == Turn off tracing, delete all buffers, set bufsize to zero.
	//                     Clears KDBG_CKTYPES, KDBG_NOWRAP, KDBG_RANGECHECK, KDBG_VALCHECK,
	//                            KDBG_PIDCHECK, and KDBG_PIDEXCLUDE.
	//                     Sets controlling_pid to -1.
	//                     Disables and deallocates thread map.

    public:

	static KDState state();
	
	//
	// Does not appear that this call can fail.
	//
	// Clears/disables everything, resets to base state. (Calls KDREMOVE)
	//
	static bool reset();

	//
	// Does not actually allocate any buffer space, you must
	// call initialize_buffers to do that.
	//
	static bool set_buffer_capacity(uint32_t capacity);

	//
	// It appears this flag can be set or cleared at any time, even if a
	// trace is currently active.
	//
	// If nowrap is true, the buffer state will not set is_wrapped, even
	// if the buffer overflows.
	//
	static bool set_nowrap(bool is_nowrap);

	//
	// If tracing is active, disable it.
	// If buffers are allocated, free them.
	// If a thread map is allocated, delete it.
	//
	// clears KDBG_WRAPPED
	//
	// Allocates new buffers of the size set in set_buffer_capacity()
	// Sets KDBG_BUFINIT
	//
	static bool initialize_buffers();

	//
	// Legal values are:
	//
	// KDEBUG_ENABLE_TRACE (full set of tracepoints)
	// KDEBUG_ENABLE_PPT (subset of tracepoints to minimize performance impact)
	// 0 (Disable)
	//
	static bool set_enabled(uint32_t value);

	//
	// Reads all available threadmap data
	//
	// Fails if KDBG_MAPINIT is not set.
	//
	template <typename KERNEL_SIZE>
	static std::vector<KDThreadMapEntry<KERNEL_SIZE>> threadmap(KDState& state);

	//
	// Reads the *current* threadmap data
	//
	// NOTE that this differs from "threadmap", which reads the threadmap
	// data that was snapshotted when the trace buffers were initialized.
	//
	template <typename SIZE>
	static std::vector<KDThreadMapEntry<SIZE>> current_threadmap();

	//
	// Reads the current cpumap.
	//
	// Fails if the buffers have not been initialized.
	//
	// The caller is responsible for the memory returned, which should be free()'d
	//
	static std::vector<KDCPUMapEntry> cpumap();

        //
        // Writes the current cpumap to the given fd
        //
        // Fails if the buffers have not been initialized, or if the provided fd cannot be written to.
	//
	// Writes a VERSION 1+ threadmap (containing an embedded cpumap) to the fd, and then
	// enough zero bytes to pad to a file block alignment
	//
        static bool write_maps(int fd);

	//
	// Blocks in the kernel until the trace buffers are 50% full.
	// Then writes all events to the provided fd.
	//
	// Fails if the buffers are not initialized, tracing is not enabled, or the provided fd cannot be written to.
	//
	// Returns -1 on failure, otherwise the numbers of trace events written.
	static int write_events(int fd);
        
	//
	// Reads all available trace data.
	//
	// Returns -1 on failure, otherwise the number of elements read.
	//
	template <typename SIZE>
	static int read(KDEvent<SIZE>* buf, size_t buf_size_in_bytes);
};

template <typename SIZE>
int KDBG::read(KDEvent<SIZE>* buf, size_t buf_size_in_bytes)
{
	ASSERT(buf, "Sanity");
	
	int mib[3];
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDREADTR;
	
	if (sysctl(mib, 3, buf, &buf_size_in_bytes, NULL, 0) < 0) {
		DEBUG_ONLY(log_msg(ASL_LEVEL_WARNING, "trace facility failure, KERN_KDREADTR: %s\n", strerror(errno)));
		return -1;
	}
	
	return (int)buf_size_in_bytes;
 }

template <typename SIZE>
std::vector<KDThreadMapEntry<SIZE>> KDBG::threadmap(KDState& state)
{
	std::vector<KDThreadMapEntry<SIZE>> maps(state.thread_map_capacity());
	size_t size = state.thread_map_capacity() * sizeof(KDThreadMapEntry<SIZE>);
	
	int mib[3];
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDTHRMAP;
	
	if (sysctl(mib, 3, maps.data(), &size, NULL, 0) < 0) {
		maps.clear();
	}
	
	return maps;
 }

template <typename SIZE>
std::vector<KDThreadMapEntry<SIZE>> KDBG::current_threadmap()
{
	std::vector<KDThreadMapEntry<SIZE>> maps(2048);

resize:
	size_t size_in = maps.size() * sizeof(KDThreadMapEntry<SIZE>);
	size_t size_out = size_in;

	int mib[3];
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDREADCURTHRMAP;

	if (sysctl(mib, 3, maps.data(), &size_out, NULL, 0) < 0) {
		// Grr, seems like this doesn't report a target size, we have to guess!
		if (errno == EINVAL && size_out == size_in) {
			maps.resize(maps.size() * 2);
			goto resize;
		}
		maps.clear();
	} else {
		maps.resize(size_out / sizeof(KDThreadMapEntry<SIZE>));
	}

	return maps;
}
