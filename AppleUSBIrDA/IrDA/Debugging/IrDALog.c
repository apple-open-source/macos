/*
    File:       IrDALog.c

    Contains:   Logging support for IrDA.

    Written by: Clark Donahue, Jim Guyton
    
    Todo: add copyright


*/
//#include <Kernel/kern/clock.h>
#include <IOKit/IOLib.h>
#include "IrDALog.h"
#include "IrDADebugging.h"
#include "IrDALogPriv.h"

#ifndef nil
#define nil 0
#endif

#define DEBUGGER(x) panic(x)    // revisit this!

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Following moved to IrDALogPriv.h (or IrDADebugging.h)
//
//#define hasTracing  1             // set to one to have tracing, to zero to compile out
//#define USE_IOLOG 1               // true if want to go to IOLog
//#define IOSLEEPTIME   700         // ms delay after each IOLog
//#define kEntryCount   (10*1024)   // Number of log entries.   *** Change to runtime alloc?
//#define kMaxModuleNames   50              // max number of clients (unique module names)
//#define kMaxModuleNameLen 32              // max length of module name
//#define kMaxIndex         200             // max event index (# of msgs) per module
//#define   kMsgBufSize     (20*1024)       // way overkill -- 20k for copies of msgs
///////////////////////////////////////////////////////////////////////////////////////////////////////


#if (hasTracing > 0)

char *GetCachedMsg(EventTraceCauseDesc *desc, UInt16 eventIndex);

// Globals
IrDAEventDesc fBuffer[kEntryCount+10];  // Buffer (+10 is hack so wrap race condition doesn't smash memory)

#pragma export on               // Start of public code
#pragma mark Start Exported -------------

IrDALogHdr gIrDALog = {                 // the log header
	&fBuffer[0],                    // fEventBuffer
	0,                              // fEventIndex
	0,                              // fPrintIndex
	kEntryCount,                    // fNumEntries
	true,                           // fTracingOn
	false                           // fWrapped
	//true                          // fWrappingEnabled
};

#endif // hasTracing > 0


#if (hasTracing > 0)
#ifdef __cplusplus
extern "C"
#endif // __cplusplus
void IrDALogAdd( UInt16 eventIndex, UInt16 data1, UInt16 data2, EventTraceCauseDesc * desc, Boolean timeStamp)
{

    IrDAEventDesc *logEntry;
    UInt32 cTime;
    
    // sanity checks
    require(eventIndex > 0, Fail);
    require(desc, Fail);
    
    if(!gIrDALog.fTracingOn)        // nop if tracing not enabled
	return;
	
    eventIndex--;                   // FOO.  EventIndex is 1 based instead of zero based.
    
#if (USE_IOLOG > 0)
    {
	IOLog("%04x %04x %s\n", data1, data2, desc[eventIndex].description);
	IOSleep(IOSLEEPTIME);   // in ms
    }
#endif

    if( gIrDALog.fEventIndex >= gIrDALog.fNumEntries ) {    // Wrap if hit end of buffer (this one shouldn't be hit, sanity only)
	gIrDALog.fEventIndex = 0;
	if (gIrDALog.fEventIndex == gIrDALog.fPrintIndex)   // if newly wrapped index now matches print index
	    gIrDALog.fWrapped = true;                       // then we've wrapped past it
    }
    
    logEntry = &gIrDALog.fEventBuffer[gIrDALog.fEventIndex++];  // Get the log entry pointer & incr ptr

    if( gIrDALog.fEventIndex >= gIrDALog.fNumEntries) {     // Wrap if hit end of buffer (this one should be hit)
	gIrDALog.fEventIndex = 0;
    }
    
    if (gIrDALog.fEventIndex == gIrDALog.fPrintIndex)           // if the (incr'd) index matches printing index
	gIrDALog.fWrapped = true;                               // then we've wrapped past it

    //if (gIrDALog.fWrappingEnabled == false && gIrDALog.fWrapped == true)  // if not allowed to wrap
    //  return;
	
    //
    // Now that we have a record, get the current time (if requested)
    //
    if (timeStamp) {
	    AbsoluteTime now;
	    UInt64  nanoseconds;
	    clock_get_uptime(&now);
	    absolutetime_to_nanoseconds(now, &nanoseconds);
	    cTime = nanoseconds / 1000;     // microseconds is plenty for me
    }
    else cTime = 0;
    
    // Ok, stuff a log entry
    logEntry->data1         = data1;                                // Stuff in the data
    logEntry->data2         = data2;
    logEntry->timeStamp     = cTime;                                // log the time
    logEntry->msg           = GetCachedMsg(desc, eventIndex);       // get pointer to copy of msg (or nil)
    
Fail:
    return;
    
} // IrDALogAdd


void
IrDALogTracingOn(void)
{
    gIrDALog.fTracingOn = true;
}

void
IrDALogTracingOff(void)
{
    gIrDALog.fTracingOn = false;
}

/*
void
IrDALogWrappingOn()
{
    gIrDALog.fWrappingEnabled = true;
}

void
IrDALogWrappingOff()
{
    gIrDALog.fWrappingEnabled = false;
}
*/

#endif // hasTracing > 0

#pragma mark Message Cache -------------

#if (hasTracing > 0)


char    gMsgBuf[kMsgBufSize];                               // the big buffer for copies of msgs
char    gModuleNames[kMaxModuleNames][kMaxModuleNameLen];   // table client module names
char    *gMsgPtrs[kMaxModuleNames][kMaxIndex];              // pointers to copies of msgs
char    *gNextMsg = &gMsgBuf[0];                            // pointer to next avail byte in gMsgBuf
int     gNextModuleIndex = 0;                               // index of next avail entry in gModuleNames 

UInt32 GetModuleIndex(EventTraceCauseDesc *desc);
char *CopyMsg(const char *msg);

//EventTraceCauseDesc* gDebugTable;     // temp
//UInt32                gDebugIndex;
//char              *gDebugMsg;
//char              **gDebugMsgAddr;
//int                   gDebugSize;

char *
GetCachedMsg(EventTraceCauseDesc *desc, UInt16 eventIndex)
{
    UInt32  moduleIndex;        // index to module name in gModuleNames
    char *msg;
    
    // TEMP
    //gDebugTable = desc;
    //gDebugIndex = eventIndex;
    
    // Sanity checks
    if (desc[eventIndex].cause != (eventIndex + 1)) return nil;
    if (eventIndex >= kMaxIndex) {
	DEBUGGER("IrDALog: need to incr kMaxIndex");
	return nil;
    }
    
    // if msgcopy in the client array is already set, use that
    // note: hopefully this will be true a lot more often than not
    if (desc[eventIndex].msgcopy != nil)
	return desc[eventIndex].msgcopy;

    // Ok, haven't seen this event msg before. Find this module
    // in our cache.
    
    moduleIndex = GetModuleIndex(desc);         // find existing module name or make new entry and return that
    if (moduleIndex == -1) return nil;          // error return if too many modules

    // If msg is in the module table (but not in the client table) then
    // the client has died and come back.  Set all msgcopy ptrs for it.
    if (gMsgPtrs[moduleIndex][eventIndex]) {
	int i;
	//DebugStr("\pAbout to set ptrs for an entire module");
	for (i = 0 ; i < kMaxIndex; i++) {
	    if (gMsgPtrs[moduleIndex][i])                   // if a copy exists
		desc[i].msgcopy = gMsgPtrs[moduleIndex][i]; // set the msgcopy ptr
	}
	return desc[eventIndex].msgcopy;
    }
    
    // Ok, time to make a copy of the msg
    //gDebugMsg = desc[eventIndex].description;     // TEMP
    //gDebugMsgAddr = &desc[eventIndex].description;
    //gDebugSize = sizeof(desc[0]);
    
    msg = CopyMsg(desc[eventIndex].description);            // get a copy of the msg
    desc[eventIndex].msgcopy = msg;                         // save msg pointer in client!
    gMsgPtrs[moduleIndex][eventIndex] = msg;                // and in per-module table
    return msg; 
}

// This should be rewritten to use a hash table
int gModuleIndex;

UInt32 GetModuleIndex(EventTraceCauseDesc *desc)
{
    int i;
    const char *modstart;
    int namelen;
    char modulename[kMaxModuleNameLen];         // copy of module name (need the trailing null)
    
    modstart = desc[0].description;             // extract module name from 1st msg in client table
    {   const char *t;
	t = strchr(modstart, ':');
	if (t == nil) return -1;
	namelen = t - modstart;
	if (namelen < 1 ||  namelen-1 > kMaxModuleNameLen) {        // check length
	    DEBUGGER("IrDALog: rejecting module name(len)");
	    return -1;
	}
    }
    strncpy(modulename, modstart, namelen);     // copy the name
    modulename[namelen] = 0;                    // make it a C string
    
    for (i = 0 ; i < gNextModuleIndex; i++) {   // Sigh.  Search the module table.
	if (strcmp(modulename, gModuleNames[i]) == 0) {
	    gModuleIndex = i;
	    return i;                           // found it!
	}
    }
    
    // Not in the current list of module names
    // make a new entry
    
    if (gNextModuleIndex < kMaxModuleNames) {                   // if room in the table
	strlcpy(gModuleNames[gNextModuleIndex++], modulename, sizeof(gModuleNames[0]));   // copy it in
	gModuleIndex = gNextModuleIndex-1;
	return gNextModuleIndex-1;
    }
    DEBUGGER("IrDALog: need to increase kMaxModuleNames");
    return -1;
}


// return ptr to a copy of the msg or nil if out of memory
char *CopyMsg(const char *msg)
{
    char *result;
    int len;
    
    result = gNextMsg;          // start of next avail msg
    len = strlen(msg) + 1;      // how much to copy

    if (gNextMsg + len >= &gMsgBuf[kMsgBufSize]) {  // to many msgs?
	DEBUGGER("IrDALog: need to incr kMsgBufSize");
	return nil;
    }
    
    memcpy(result, msg, len);   // copy msg including null
    gNextMsg += len;            // incr avail ptr for next msg
    
    return result;
}

IrDALogInfo gIrDALogInfo = {
	&gIrDALog, sizeof(gIrDALog),
	fBuffer, sizeof(fBuffer),
	gMsgBuf, sizeof(gMsgBuf) };
	

IrDALogInfo *
IrDALogGetInfo(void)
{
    return &gIrDALogInfo;
/*
    if (info == nil) return;
    info->hdr = &gIrDALog;
    info->hdrSize = sizeof(gIrDALog);
    info->eventLog = fBuffer;
    info->eventLogSize = sizeof(fBuffer);
    info->msgBuffer = gMsgBuf;
    info->msgBufferSize = sizeof(gMsgBuf);
*/
}

void
IrDALogReset(void)
{
    gIrDALog.fEventIndex = 0;
    gIrDALog.fPrintIndex = 0;
    gIrDALog.fNumEntries = kEntryCount;
    // don't reset fTracingOn
    //gIrDALog.fTracingOn = true;
    gIrDALog.fWrapped = false;
    // don't reset fWrappingEnabled
}

#endif // hasTracing > 0



