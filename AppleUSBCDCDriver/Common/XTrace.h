#ifndef _XTRACE_H_
#define	_XTRACE_H_

#ifdef KERNEL
#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#endif // KERNEL


#define XTRACE_HELPER(global, id, x, y, msg)                    					\
do {													\
    if (global) {											\
	static char *__xtrace = 0;              							\
	    if (__xtrace)   global->LogAdd((UInt32)id, (UInt32)(x), (UInt32)(y), __xtrace);     	\
		else __xtrace = global->LogAdd((UInt32)id, (UInt32)(x), (UInt32)(y), msg, false);	\
    }													\
} while(0)

#define kXTraceKextName 	"com_apple_iokit_XTrace"

typedef struct {			// Each log entry in the circular buffer looks like this
    UInt32	id;			// unique id or 'this' or whatever the client wants to put here
    UInt32	data1;			// two 32 bit numbers for the log
    UInt32	data2;
    UInt32	timeStamp;		// timestamp of the log entry - in microseconds
    char	*msg;			// pointer to cached copy of the event msg
} EventDesc;

enum {					// command codes for the user-client to send in
    cmdXTrace_GetLog = 201,		// get the big buffers
    cmdXTrace_GetLogSizes,		// get the buffer sizes
    XTRACE_Magic_Key = 'JDG!'		// code for connect
};


typedef struct  {				// structure of info returned to userclient
    EventDesc	*eventLog;			// address of the eventlog (in the kernel)
    UInt32	eventLogSize;			// number of bytes in the eventLog
    char	*msgBuffer;			// address of the msg buffer (in the kernel)
    UInt32	msgBufferSize;			// number of bytes in above
    char	*fNextMsg;			// pointer to next avail byte in msgBuffer

    UInt32	fNumEntries;			// number of entries in eventLog (kEntryCount)
    UInt32	fEventIndex;			// index of next available event entry in fBuffer
    UInt32	fPrintIndex;			// set by dcmd to keep track of what's pretty printed already
    Boolean	fTracingOn;			// true if allowing adds
    Boolean	fWrapped;			// true if adding log entries wrapped past the printing ptr
    Boolean	fWrappingEnabled;		// true if wrapping allowed (else stops before dropping data)
} XTraceLogInfo;

typedef struct {
    UInt32	fBufferSize;			// sizeof(fBuffer)
    UInt32	fMsgBufSize;			// sizeof(fMsgBuf)
} XTraceLogSizes;

#ifdef KERNEL

enum {					// todo - make these plist or userclient changeable
    kEntryCount = (200*1024),		// number of log entries before wrap (16 bytes per entry)
    kMsgBufSize = (20*1024)		// buffer for message texts
};

class com_apple_iokit_XTrace : public IOService
{
    OSDeclareDefaultStructors(com_apple_iokit_XTrace)

private:
    XTraceLogInfo	fInfo;			// indexes and pointers.  passed to remote clients and userclient
    IOLock		*fMyLock;		// protect self
    UInt32		fId;			// simple id counter for callers use
    
    char	*fErrorMsg;			// first msg is reserved for an overflow error msg
    char	*GetCachedMsg(char *msg, bool compress);
    char	*CopyMsg(char *msg);
    void	dumpbuffers();			// temp debugging to iolog
	
public:
    virtual bool init(OSDictionary *dictionary);
    virtual void free(void);
    virtual bool start(IOService *provider);
    virtual void stop(IOService *provider);
    virtual IOService *probe(IOService *provider, SInt32 *score);
    virtual IOReturn newUserClient( task_t owningTask, void * securityID, UInt32 type, IOUserClient ** handler );

    // this is optional - call to get a simple "unique" id for LogAdd (or just use 'this' from C++)
    virtual UInt32	GetNewId(void);			// simple counter of xtrace clients

    // call this one first, it adds the msg to the cache and returns a pointer to the cached copy
    // pass 'true' to compress if you want to search the cache for an old(er) copy of the same msg
    virtual char *	LogAdd(UInt32 id, UInt32 data1, UInt32 data2, char *msg, Boolean compress);

    // call this one after the first time with the cache value.
    virtual void	LogAdd(UInt32 id, UInt32 data1, UInt32 data2, char *cachedtext);
    
    virtual void	LogTracingOn(void);		// default is on
    virtual void	LogTracingOff(void);
    virtual void	LogWrappingOn(void);		// default is on
    virtual void	LogWrappingOff(void);

    // for userclient
    virtual XTraceLogInfo *LogGetInfo(void);
    virtual void	   LogReset(UInt32 printIndex);
    virtual void	   LogLock();
    virtual void	   LogUnlock();

};

void inline com_apple_iokit_XTrace::LogTracingOn(void) { fInfo.fTracingOn = true; }
void inline com_apple_iokit_XTrace::LogTracingOff(void) { fInfo.fTracingOn = false; }
void inline com_apple_iokit_XTrace::LogWrappingOn(void) { fInfo.fWrappingEnabled = true; }
void inline com_apple_iokit_XTrace::LogWrappingOff(void) { fInfo.fWrappingEnabled = false; }
UInt32 inline com_apple_iokit_XTrace::GetNewId(void) { return ++fId; }		// lock this

void inline com_apple_iokit_XTrace::LogLock(void)	{ IOLockLock(fMyLock); }
void inline com_apple_iokit_XTrace::LogUnlock(void)	{ IOLockUnlock(fMyLock); }

/****************************************************************************************************/
/***************************************** UserClient ***********************************************/
/****************************************************************************************************/

class com_apple_iokit_XTrace;

class com_apple_iokit_XTraceUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(com_apple_iokit_XTraceUserClient)

private:
    com_apple_iokit_XTrace	*fProvider;
    IOExternalMethod		fMethods[1];		// just one method
    task_t			fTask;

public:
    static com_apple_iokit_XTraceUserClient *withTask(task_t owningTask);	// Factory Constructor
    IOReturn		clientClose();
    IOReturn		clientDied();

    bool		start(IOService *provider);
    IOReturn		connectClient(IOUserClient *client);
    IOExternalMethod*	getExternalMethodForIndex(UInt32 index);

    IOReturn doRequest(void *pIn, void *pOut, IOByteCount inputSize, IOByteCount *outPutSize);

private:
    IOReturn getLog(void *pIn, void *pOut, IOByteCount inputSize, IOByteCount *outPutSize);
    IOReturn getLogSizes(void *pIn, void *pOut, IOByteCount inputSize, IOByteCount *outPutSize);
    IOReturn copyLogEntries (IOMemoryDescriptor *md,	// path to client memory
			     EventDesc	*eventLog,	// source buffer
			     UInt32	first,		// index of first entry to copy over
			     UInt32	next);		// index of last+1 entry to copy over
};


#endif		// KERNEL



#endif // _XTRACE_H_
