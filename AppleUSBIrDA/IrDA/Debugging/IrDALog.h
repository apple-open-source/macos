/*
    File:       IrDALog.h

    Contains:   Class definition for IrDA logging object

    Written by: Clark Donahue, Jim Guyton
    
    Add copyright


*/

#ifndef __IRDALOG__
#define __IRDALOG__

#include <IOKit/IOTypes.h>

typedef struct EventTraceCauseDesc          // An array of these is supplied by each logging client
{
    UInt32  cause;                          // really a single byte cause enumeration
    const char    *description;		    // descriptive text
    char    *msgcopy;                       // client sets to nil.  Set by IrDALog to point to cached copy of msg
} EventTraceCauseDesc;

typedef struct IrDAEventDesc                // Each entry in the saved log looks like this
{
    UInt16              data1;              // two 16 bit numbers for the log
    UInt16              data2;
    UInt32              timeStamp;          // timestamp of the log entry - in microseconds
    char                *msg;               // pointer to copy of event msg
} IrDAEventDesc, *IrDAEventDescPtr;

typedef struct IrDALogHdr                   // The one global log header to keep track of the log buffer
{
    IrDAEventDescPtr    fEventBuffer;       // pointer to base of the event log
    UInt32              fEventIndex;        // index of next available log entry
    UInt32              fPrintIndex;        // set by dcmd to keep track of what's pretty printed already
    UInt32              fNumEntries;        // kEntryCount -- let dcmd know when to wrap
    Boolean             fTracingOn;         // true if allowing adds
    Boolean             fWrapped;           // true if adding log entries wrapped past the printing ptr
    //Boolean               fWrappingEnabled;   // true if wrapping allowed
} IrDALogHdr, *IrDAEventLogPtr;

typedef struct IrDALogInfo                  // The pointers and buffers passed by to the dumplog application
{
    IrDALogHdr  *hdr;                       // the global log header (points to event array)
    UInt32      hdrSize;                    // size of the log hdr
    IrDAEventDesc   *eventLog;              // the event buffer
    UInt32      eventLogSize;               // size of the event log array
    char        *msgBuffer;                 // pointer buffer of messages
    UInt32      msgBufferSize;              // size of above
} IrDALogInfo;


#ifdef __cplusplus
extern "C"
{
#endif
void IrDALogAdd (   UInt16  eventIndex,         // index of log entry in table (1-based)
			UInt16  data1,              // arbitrary data for the log, first 16 bits
			UInt16  data2,              // arbitrary data for the log, 2nd 16 bits
			EventTraceCauseDesc *desc,  // base of event trace table
			Boolean timeStamp);         // true if want log entry timestamped

void    IrDALogTracingOn    ( void );           // default is on
void    IrDALogTracingOff   ( void );
//void  IrDALogWrappingOff();
//void  IrDALogWrappingOn();

void    IrDALogReset        ( void );
IrDALogInfo *IrDALogGetInfo(void);

#ifdef __cplusplus
}   // extern C
#endif



#endif // __IRDALOG__