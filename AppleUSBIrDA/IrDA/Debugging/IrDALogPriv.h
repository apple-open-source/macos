/*
 * Control how IrDAlog.c is compiled
 *
 */

#include "IrDADebugging.h"          // uses hasTracing from here.  If hasTracing = 0, no logging at all

#define USE_IOLOG   0               // true if want to go to IOLog
#define IOSLEEPTIME 25              // ms delay after each IOLog

#define kEntryCount (30*1024)               // Number of log entries.   *** Change to runtime alloc?
#define kMaxModuleNames     50              // max number of clients (unique module names)
#define kMaxModuleNameLen   32              // max length of module name
#define kMaxIndex           500             // max event index (# of msgs) per module
#define kMsgBufSize     (20*1024)           // way overkill -- 20k for copies of msgs
