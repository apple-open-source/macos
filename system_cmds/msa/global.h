//
//  global.h
//  msa
//
//  Created by James McIlree on 2/1/14.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#ifndef msa_global_h
#define msa_global_h

#include <CPPUtil/CPPUtil.h>

using namespace util;

#include <KDBG/KDebug.h>

#include <signal.h>

#include <libkern/OSAtomic.h>

#include <vector>
#include <unordered_map>
#include <thread>
#include <tuple>

__attribute__((noreturn)) void usage(const char *);

#include "Globals.hpp"
#include "EventRingBuffer.hpp"
#include "PrintBuffer.hpp"
#include "Action.hpp"
#include "ReadTraceFileAction.hpp"
#include "WriteTraceFileAction.hpp"
#include "LiveTraceAction.hpp"
#include "Printing.hpp"
#include "EventProcessing.hpp"
#include "VoucherContentSysctl.hpp"

#endif
