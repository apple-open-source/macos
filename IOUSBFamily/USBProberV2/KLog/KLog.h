/*
	KLog.h
	standard dot.h for KLog
	
	$Source: /cvs/root/IOUSBFamily/USBProberV2/KLog/KLog.h,v $
	$Log: KLog.h,v $
	Revision 1.1  2003/08/22 05:15:54  nano
	Added KLog.kext sources
	
	Revision 1.4  2001/10/13 02:27:29  bubba
	Got rid of need for config.h file.
	
	Revision 1.3  2001/10/09 23:12:28  bubba
	Updated version to 1.0.2, removed non-building targets from the BUILD_ALL
	target, upped the buffers to allow faster logging.
	
	Revision 1.2  2001/08/20 21:33:51  bubba
	Cleaned up code a bit.
	
	Revision 1.1  2001/08/13 22:37:19  davidson
	initial commit of iLogger into io/Tools directory.  Also added the
	BT-iLoggerPlugin in the ToolSources of Odin.

	Revision 1.5  2001/07/27 20:02:10  bubba
	According to convention, 'v' denotes functions that take a va_list parameter. I had these backwards
	when I made the logger changes. And since I'm a conventional guy (or not), I changed the function
	names to reflect this naming convention.
	
	Revision 1.4  2001/07/26 20:38:38  davidson
	Updates! Just some basic code cleanup, renamed some vars, etc.
	
	Revision 1.3  2001/07/26 18:23:49  bubba
	Fix misprinted values in the logging mechanism.
	
*/

#ifndef KLOG_H	
#define KLOG_H

#include <IOKit/IOService.h>
#include <IOKit/IOLocks.h>

#include "KLogClient.h"

extern "C" {
    #include <sys/time.h>
}

//================================================================================================
//   Defines
//================================================================================================

#define kLogKextName 	"com_apple_iokit_KLog"

//================================================================================================
//   Configuration constants
//================================================================================================

#define BUFSIZE 	512 	//bytes
#define MAXENTRIES	500
#define MAXUSERS 	5

//================================================================================================
//   Custom Types
//================================================================================================

typedef UInt32 KLogLevel;
typedef UInt32 KLogTag;

//================================================================================================
//   com_apple_iokit_KLog
//================================================================================================

class com_apple_iokit_KLog : public IOService
{

    OSDeclareDefaultStructors(com_apple_iokit_KLog)

    com_apple_iokit_KLogClient *mClientPtr[MAXUSERS+1];

    unsigned char *					mMsgBuffer;
    UInt8 							mClientCount;
    UInt16							mMsgSize;
    bool 							mErrFlag;
    struct timeval *				mTimeVal;
    IOLock *						mLogLock;
    
public:

    static com_apple_iokit_KLog	*	sLogger;
    
	// IOService overrides.
	
    virtual bool 		init(OSDictionary *dictionary = 0);
    virtual void 		free(void);
    
    virtual IOService *	probe(IOService *provider, SInt32 *score);
    virtual bool 		start(IOService *provider);
    virtual void 		stop(IOService *provider);
	
    void 				closeChild(com_apple_iokit_KLogClient *ptr);
    
    virtual IOReturn	newUserClient( task_t owningTask, void * securityID,
										UInt32 type, IOUserClient ** handler );

	// Class specific stuff.
	
	void 				setErr( bool set );

	// Write items into our buffer using these.

	virtual	SInt8		Log( KLogLevel level, KLogTag tag, const char *format, ... );
	virtual	SInt8		vLog( KLogLevel level, KLogTag tag, const char *format, va_list in_va_list );
};

#endif



