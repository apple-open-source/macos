//
//  KDebug.h
//  KDBG
//
//  Created by James McIlree on 4/15/13.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#ifndef KDebug_KDebug_h
#define KDebug_KDebug_h

#include <sys/sysctl.h>
#include <sys/buf.h>
#include <mach/task_policy.h>

#ifndef KERNEL_PRIVATE
	#define KERNEL_PRIVATE
	#include <sys/kdebug.h>
	#undef KERNEL_PRIVATE
#else
	#error Something is really strage...
#endif /*KERNEL_PRIVATE*/

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

#include <libkern/OSAtomic.h>

#include <CPPUtil/CPPUtil.h>

using namespace util;

#include "MetaTypes.hpp"
#include "TaskRequestedPolicy.hpp"
#include "TaskEffectivePolicy.hpp"
#include "KDState.hpp"
#include "KDThreadMapEntry.hpp"
#include "KDCPUMapEntry.hpp"
#include "KDEvent.hpp"
#include "KDBG.hpp"
#include "Kernel.hpp"
#include "TraceCodes.hpp"
#include "MachineVoucher.hpp"
#include "VoucherInterval.hpp"
#include "MachineThread.hpp"
#include "IOActivity.hpp"
#include "CPUActivity.hpp"
#include "ThreadSummary.hpp"
#include "ProcessSummary.hpp"
#include "MachineMachMsg.hpp"
#include "NurseryMachMsg.hpp"
#include "CPUSummary.hpp"
#include "MachineCPU.hpp"
#include "MachineProcess.hpp"
#include "TraceDataHeader.hpp"
#include "TraceFile.hpp"
#include "Machine.hpp"
#include "Machine.impl.hpp"
#include "Machine.mutable-impl.hpp"
#include "MachineProcess.impl.hpp"
#include "MachineProcess.mutable-impl.hpp"
#include "MachineThread.impl.hpp"
#include "MachineThread.mutable-impl.hpp"
#include "MachineCPU.impl.hpp"
#include "MachineCPU.mutable-impl.hpp"

#endif
