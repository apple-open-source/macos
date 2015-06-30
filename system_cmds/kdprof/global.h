//
//  global.h
//  kdprof
//
//  Created by James McIlree on 4/15/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef kdprof_global_h
#define kdprof_global_h

#include <CPPUtil/CPPUtil.h>

using namespace util;

#include "KDebug.h"

#include <dispatch/dispatch.h>
#include <libkern/OSAtomic.h>

#include <vector>
#include <unordered_map>
#include <thread>

#include "Globals.hpp"
#include "EventPrinting.hpp"
#include "SummaryPrinting.hpp"
#include "Action.hpp"
#include "InitializeAction.hpp"
#include "TraceFileAction.hpp"
#include "RemoveAction.hpp"
#include "NoWrapAction.hpp"
#include "PrintStateAction.hpp"
#include "EnableAction.hpp"
#include "DisableAction.hpp"
#include "CollectAction.hpp"
#include "SleepAction.hpp"
#include "SaveTraceAction.hpp"

__attribute__((noreturn)) void usage(const char *);

#endif
