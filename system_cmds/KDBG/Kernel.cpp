//
//  Kernel.cpp
//  KDBG
//
//  Created by James McIlree on 4/17/13.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#include "KDebug.h"

using namespace util;

bool Kernel::is_64_bit()
{
	int mib[4];
	size_t len;
	struct kinfo_proc kp;

	/* Now determine if the kernel is running in 64-bit mode */
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PID;
	mib[3] = 0; /* kernproc, pid 0 */
	len = sizeof(kp);
	if (sysctl(mib, sizeof(mib)/sizeof(mib[0]), &kp, &len, NULL, 0) == -1) {
		THROW("sysctl to get kernel size failed");
	}

	if (kp.kp_proc.p_flag & P_LP64)
		return true;

	return false;
}

uint32_t Kernel::active_cpu_count()
{
	int mib[4];
	size_t len;
	int	num_cpus;

	/*
	 * grab the number of cpus and scale the buffer size
	 */
	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	mib[2] = 0;
	len = sizeof(num_cpus);

	sysctl(mib, 2, &num_cpus, &len, NULL, 0);

	return num_cpus;
}
