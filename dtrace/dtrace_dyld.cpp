/*
 *  dtrace_dyld.cpp
 *  dtrace
 *
 *  Created by James McIlree on 3/30/09.
 *  Copyright 2009 Apple Inc.. All rights reserved.
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <mach/mach.h>

#include <mach-o/dyld_images.h>

#include "CSCppTimeoutLock.hpp"
#include "CSCppDyldSharedMemoryPage.hpp"

static
void prepareDTraceRPC() __attribute__((constructor));

static
void prepareDTraceRPC()
{
	unsetenv("DYLD_INSERT_LIBRARIES"); /* children must not have this present in their env */
	
	task_dyld_info_data_t task_dyld_info;
	mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
		
	if (kern_return_t err = task_info(mach_task_self(), TASK_DYLD_INFO, (task_info_t)&task_dyld_info, &count)) {
		mach_error("Call to task_info failed ", err);
		exit(1);
	}

	// This should NEVER happen. We're running in process, so we have to be initialized...
	if (task_dyld_info.all_image_info_addr == 0) {
		fprintf(stderr, "Impossible failure, task_dyld_info returned NULL all_image_info_addr. Please report this bug!\n");
		exit(1);
	}
	
	struct dyld_all_image_infos* aii = (struct dyld_all_image_infos*)task_dyld_info.all_image_info_addr;
	if (CSCppDyldSharedMemoryPage* connection = (CSCppDyldSharedMemoryPage*)aii->coreSymbolicationShmPage) {
		bool should_send_notice = false;
	
		//
		// First we encode data for the ping
		//
		{
			CSCppTimeoutLock lock(connection->lock_addr(), connection->timeout());
			if (lock.is_locked()) {
				connection->increment_data_generation();
				uint64_t* data = (uint64_t*)connection->data();
				data[0] = 666;					
				should_send_notice = true;
			}
		}
	
		//
		// Now we ping everyone
		//
		if (should_send_notice) {
			uint32_t sent, recv;
			connection->send_notice(CORESYMBOLICATION_DYLD_PING_MSGH_ID, sent, recv);
		}
	}
}
