//
//  SaveTraceAction.cpp
//  kdprof
//
//  Created by James McIlree on 5/2/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#include "global.h"

template <typename SIZE>
static void execute_arch_specific(Globals& globals, KDState& state, FileDescriptor& save_fd) {
	// Collect all data first, printing takes time...
	auto threadmap = KDBG::threadmap<SIZE>(state);
	auto cpumap = KDBG::cpumap();

	// These are future proofing, trace doesn't actually need page alignment
	// here, just file block size alignment. When page sizes go to 16k, we
	// don't want 16k of padding.
	
#define FILE_BLOCK_SIZE 4096
#define FILE_BLOCK_SIZE_MASK 4095

	/*
	 * To write a RAW_VERSION1+ file, we must embed a cpumap in the "padding"
	 * used to file block align the events folloing the threadmap. If the
	 * threadmap happens to not require enough padding, we artificially
	 * increase its footprint until it needs enough padding.
	 */

	uint32_t pad_size = FILE_BLOCK_SIZE - ((sizeof(TraceDataHeader<SIZE>) + (threadmap.size() * sizeof(KDThreadMapEntry<SIZE>))) & FILE_BLOCK_SIZE_MASK);
	uint32_t cpumap_size = sizeof(kd_cpumap_header) + (uint32_t)cpumap.size() * sizeof(KDCPUMapEntry);
	uint32_t extra_thread_count = 0;

	if (cpumap_size > pad_size) {
		/* Force an overflow onto the next page, we get a full page of padding */
		extra_thread_count = (pad_size / sizeof(KDCPUMapEntry)) + 1;
	}

	// Write the header
	TraceDataHeader<SIZE> header(RAW_VERSION1, (uint32_t)threadmap.size(), time(NULL), 0);
	write(save_fd, &header, sizeof(TraceDataHeader<SIZE>));

	// Write the threadmaps
	write(save_fd, threadmap.data(), threadmap.size() * sizeof(KDThreadMapEntry<SIZE>));

	if (extra_thread_count) {
		pad_size = extra_thread_count * sizeof(KDThreadMapEntry<SIZE>);
		auto pad_buf = (uint8_t *)calloc(pad_size, 1);
		write(save_fd, pad_buf, pad_size);
		free(pad_buf);
	}
	
	// Write the cpumaps & any remaining padding
	size_t bytes_written = sizeof(TraceDataHeader<SIZE>) + (threadmap.size() + extra_thread_count) * sizeof(KDThreadMapEntry<SIZE>);
	pad_size = FILE_BLOCK_SIZE - (bytes_written & FILE_BLOCK_SIZE_MASK);

	ASSERT(pad_size >= cpumap.size() * sizeof(KDCPUMapEntry), "Not enough padding bytes!");
	if (pad_size) {
		auto cpumap_header = (kd_cpumap_header*)calloc(pad_size, 1);
		cpumap_header->version_no = RAW_VERSION1;
		cpumap_header->cpu_count = (uint32_t)cpumap.size();
		auto cpus = (kd_cpumap*)&cpumap_header[1];
		memcpy(cpus, cpumap.data(), cpumap.size() * sizeof(KDCPUMapEntry));
		write(save_fd, cpumap_header, pad_size);
	}

	// Write the events
	//
	// Because this may be used to capture boot traces which consume very
	// large amounts of memory, we will likely not be able to collect
	// the entire buffer space in a single shot. Read it in small chunks.
	//
	auto twenty_mb = 20 * 1024 * 1024;
	auto num_events_in_twenty_mb = twenty_mb / sizeof(KDEvent<SIZE>);
	MemoryBuffer<KDEvent<SIZE>> events(num_events_in_twenty_mb);

	// We read until we don't get back a full buffer, hoping thats enough.
	while (1) {
		int count = KDBG::read(events.data(), events.capacity_in_bytes());

		if (count != -1)
			write(save_fd, events.data(), count * sizeof(KDEvent<SIZE>));
		
		if (count < num_events_in_twenty_mb) {
			break;
		}
	}

	// close up
	save_fd.close();
}

void SaveTraceAction::execute(Globals& globals) {
	KDState state = KDBG::state();
	if (state.is_lp64()) {
		execute_arch_specific<Kernel64>(globals, state, _save_fd);
	} else {
		execute_arch_specific<Kernel32>(globals, state, _save_fd);
	}
}
