/*
 * Copyright © 1998-2012 Apple Inc.  All rights reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


#include <sys/systm.h>

#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/usb/USB.h>

#ifdef	__cplusplus
	extern "C" {
#endif


//KernelDebugLevel		gKernelDebugLevel	= kKernelDebugAnyLevel;
KernelDebugLevel		gKernelDebugLevel	= 1;
KernelDebuggingOutputType	gKernelDebugOutputType 	= kKernelDebugOutputIOLogType;
com_apple_iokit_KLog *		gKernelLogger		= NULL;
#define DEBUG_NAME		"[KernelDebugging] "

// ascii armoring stuff
const char LOWEST = 0x20;
char *armor(void *buffer, int bytecount);

//===========================================================================================================================
//	EnableKernelDebugger
//===========================================================================================================================

void	 KernelDebugEnable( bool enable )
{
    IOLog( DEBUG_NAME"KernelDebugEnable (%d)\n",enable);
    
    if ( enable )
    {
        KernelDebugFindKernelLogger();
    }
}

//===========================================================================================================================
//	KernelDebugSetLevel
//===========================================================================================================================

void	KernelDebugSetLevel( KernelDebugLevel inLevel )
{	
	gKernelDebugLevel = inLevel;
	IOLog( DEBUG_NAME "Debugging level changed to: %d\n", (int) gKernelDebugLevel );
}

//===========================================================================================================================
//	KernelDebugGetLevel
//===========================================================================================================================

KernelDebugLevel	KernelDebugGetLevel()
{	
	IOLog( DEBUG_NAME "KernelDebugGetLevel called (%d)\n", (int) gKernelDebugLevel );
	return( gKernelDebugLevel );
}

//===========================================================================================================================
//	KernelDebugGetOutputType
//===========================================================================================================================

KernelDebugLevel	KernelDebugGetOutputType()
{	
	IOLog( DEBUG_NAME "KernelDebugGetOutputType called (%d)\n", (int) gKernelDebugOutputType );
	return( gKernelDebugOutputType );
}

//===========================================================================================================================
//	KernelDebugSetOutputType
//		Choose between regular IOLogs or the super-fancy pantsy logging kext.
//===========================================================================================================================

void	KernelDebugSetOutputType( KernelDebuggingOutputType inType )
{	
	gKernelDebugOutputType = inType;
	
	if ( inType & kKernelDebugOutputKextLoggerType )
	{
		// KernelDebugFindKernelLogger();
		
		if ( !gKernelLogger )
		{
			// Failed to find the logger!
			
			// gKernelDebugOutputType = kKernelDebugOutputIOLogType;
			IOLog( DEBUG_NAME "SetOutputType failed: could not find kernel logger. It probably needs to be loaded?\n" );
		}
	}
	
	IOLog( DEBUG_NAME "Debugging type changed to: %d\n", (int) gKernelDebugOutputType );
}

//===========================================================================================================================
//	KernelDebugLogInternal
//		This is called when a macro is invoked or KernelDebugLog is called.
//===========================================================================================================================

void	KernelDebugLogInternal( UInt32 inLevel,  UInt32 inTag, char const *inFormatString, ... )
{	
    UInt64		elapsedTime;
    uint32_t		secs, milliSecs;
    
    if ( inLevel > gKernelDebugLevel )
    {
        // The level is not high enough to be displayed, we're skipping this item.
        return;
    }
    else
    {
        // Print to the console.

        if ( gKernelDebugOutputType & kKernelDebugOutputIOLogType )
        {		
            va_list		ap;
            extern void 	conslog_putc(char);
           // extern void 	logwakeup();
                    
            // First, print our USB tag with the time
            // Find our current time in seconds (since bootup)
            //
			uint64_t	currentTime = mach_absolute_time();
			absolutetime_to_nanoseconds(*(AbsoluteTime *)&currentTime, &elapsedTime);
            
            // Convert it to milliseconds
            //
            elapsedTime = elapsedTime/(1000000);
            secs = elapsedTime / 1000;
            milliSecs = elapsedTime % 1000;

            IOLog("%c%c%c%c:\t%d.%3.3d\t",(char)(inTag>>24), (char)(inTag>>16), (char)(inTag>>8), (uint32_t)inTag, secs, milliSecs);

			va_start( ap, inFormatString );
			IOLogv(inFormatString, ap);
            va_end( ap );
			
            // And add a newline for USB logging
            if ( inTag == 'USBF')
                IOLog("\n");
        }

        // Write to the kernel logger if available.
        
        if ( (gKernelDebugOutputType & kKernelDebugOutputKextLoggerType) && gKernelLogger )
        {
            va_list		ap;
                    
            va_start( ap, inFormatString );
            gKernelLogger->vLog( inLevel, inTag, inFormatString, ap );
            va_end( ap );
        }
    }
}

//====================================================================================================
//	KernelDebugLogDataInternal
//		This is called when a macro is invoked or KernelDebugLogData is called.
//====================================================================================================

void 	KernelDebugLogDataInternal( UInt32 inLevel,  UInt32 inTag, void *buffer, UInt32 byteCount, bool preBuffer)
{
	if (preBuffer)
		KernelDebugLogInternal(inLevel, inTag, "%s", armor(buffer, byteCount));
	else
		KernelDebugLogInternal(inLevel, inTag, "%s\n", armor(buffer, byteCount));
}


#ifdef	__cplusplus
	}
#endif

//====================================================================================================
// KernelDebugFindKernelLogger
//====================================================================================================

IOReturn KernelDebugFindKernelLogger()
{
	OSIterator *		iterator 			= NULL;
	OSDictionary *		matchingDictionary	= NULL;
	IOReturn			error 				= 0;
	IOService *			matchingService		= NULL;
	
	// Get matching dictionary.
	
	matchingDictionary = IOService::serviceMatching( kLogKextName );
	if ( !matchingDictionary )
	{
		error = kIOReturnError;
		IOLog( DEBUG_NAME "[FindKernelLogger] Couldn't create a matching dictionary.\n" );
		goto exit;
	}
	
	// Get an iterator for the 'KLog'. Wait for up to 30 secs
	
	matchingService = IOService::waitForMatchingService( matchingDictionary, kSecondScale * 30ULL);
	if ( !matchingService )
	{
		error = kIOReturnError;
		IOLog( DEBUG_NAME "[FindKernelLogger] Couldn't find the matchingService.\n" );
		goto exit;
	}
	
	iterator = IOService::getMatchingServices( matchingDictionary );
	if ( !iterator )
	{
		error = kIOReturnError;
		IOLog( DEBUG_NAME "[FindKernelLogger] No %s found.\n", kLogKextName );
		goto exit;
	}
	
	// User iterator to find each com_apple_iokit_KLog instance. There should be only one, so we
	// won't iterate.
	
	gKernelLogger = (com_apple_iokit_KLog*) iterator->getNextObject();
	if ( gKernelLogger )
	{
		IOLog( DEBUG_NAME "[FindKernelLogger] Found a logger at %p.\n", gKernelLogger );
	}
	
exit:
	
	if ( error == kIOReturnSuccess )
	{
		IOLog( DEBUG_NAME "[FindKernelLogger] Found a logger instance.\n" );
	}
	else
	{
		gKernelLogger = NULL;
		IOLog( DEBUG_NAME "[FindKernelLogger] Could not find a logger instance. Error = %X.\n", error );
	}
	
    if ( matchingService) matchingService->release();                       // 10397671: need to release this matchingService
	if ( matchingDictionary ) matchingDictionary->release();
	if ( iterator ) iterator->release();
		
	return( error );
}




//====================================================================================================
// Ascii armoring utilitiy function.  Will pack 8bit data into asciiBuffer data for printf() compats
//====================================================================================================
char *armor(void *buffer, int bytecount)
{
	int asc_i, bin_i;
	UInt16 asc_length;
	//int buf_len;
	char *asciiBuffer;
	char *binaryBuffer = (char*)buffer;
	
	/*
	 * 4 ascii bytes per 3 binary bytes,
	 * plus a null byte.
	 */
	asc_length = bytecount * 4 / 3 + 1; 

	asciiBuffer = (char*)IOMalloc(asc_length);

	for(asc_i=0, bin_i=0; bin_i < bytecount; bin_i+=3) {
		/* ! are the bits we're taking */

		/* !!!!!!** ******** ******** */
		asciiBuffer[asc_i] = (binaryBuffer[bin_i] >> 2) & 0x3f;
		asciiBuffer[asc_i++] += LOWEST;

		/* ******!! !!!!**** ******** */
		asciiBuffer[asc_i] = (binaryBuffer[bin_i] << 4) & 0x30; 
		if (bin_i+1 < bytecount) {
			asciiBuffer[asc_i] |= (binaryBuffer[bin_i+1] >> 4) & 0xf;
			asciiBuffer[asc_i++] += LOWEST;
		} else {
			asciiBuffer[asc_i++] += LOWEST;
			break;
		}

		/* ******** ****!!!! !!****** */
		asciiBuffer[asc_i] = (binaryBuffer[bin_i+1] << 2) & 0x3c;
		if (bin_i+2 < bytecount) {
			asciiBuffer[asc_i] |= (binaryBuffer[bin_i+2] >> 6) & 0x3;		
			asciiBuffer[asc_i++] += LOWEST;
		} else {
			asciiBuffer[asc_i++] += LOWEST;
			break;
		}

		/* ******** ******** **!!!!!! */
		asciiBuffer[asc_i] = binaryBuffer[bin_i+2] & 0x3f;
		asciiBuffer[asc_i++] += LOWEST;
	}

	asciiBuffer[asc_i] = '\0';

	return asciiBuffer;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IOService
OSDefineMetaClassAndStructors( IOUSBLog, IOService )

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool	IOUSBLog::init( OSDictionary * dictionary )
{
    if (!super::init(dictionary))
        return false;

    return true;
    
}


IOUSBLog *
IOUSBLog::usblog()
{
    IOUSBLog *me = new IOUSBLog;

    if (me && !me->init()) {
        me->release();
        return NULL;
    }

    return me;
}

//========================================================================================
//
// USBLogPrintf( level, format, ...)
//
//========================================================================================
//
void IOUSBLog::USBLogPrintf(UInt32 level, char *format,...)
{
#pragma unused (level)
    va_list		ap;
    char		msgBuf[255];
    
    va_start( ap, format );
    vsnprintf(msgBuf, sizeof(msgBuf), format, ap);
    va_end( ap );

    USBLog(level,"%s", msgBuf);
}

void 	IOUSBLog::AddStatusLevel (UInt32 level, UInt32 ref, char *status, UInt32 value)
{
#pragma unused (level, ref, status, value)
}

void	IOUSBLog::AddStatus(char *message)
{
#pragma unused (message)
}
void	IOUSBLog::AddStatus(UInt32 level, char *message)
{
#pragma unused (level, message)
}

char *
IOUSBLog::strstr(const char *in, const char *str)
{
    char c;
    size_t len;
	
    c = *str++;
    if (!c)
        return (char *) in;	// Trivial empty string case
	
    len = strlen(str);
    do {
        char sc;
		
        do {
            sc = *in++;
            if (!sc)
                return (char *) 0;
        } while (sc != c);
    } while (strncmp(in, str, len) != 0);
	
    return (char *) (in - 1);
}

const char * 
IOUSBLog::stringFromReturn( IOReturn rtn )
{
	static const IONamedValue USBReturn_values[] = { 
		{kIOUSBUnknownPipeErr,								"Pipe is invalid"														},
		{kIOUSBTooManyPipesErr,								"Device specified too many endpoints"									},
		{kIOUSBNoAsyncPortErr,								"Async Port has not been specified"        								},
		{kIOUSBNotEnoughPipesErr,							"Desired pipe was not found"        									},
		{kIOUSBNotEnoughPowerErr,							"There is not enough power for the device"        						},
		{kIOUSBEndpointNotFound,							"Endpoint does not exist"												},
		{kIOUSBConfigNotFound,								"Configuration does not exist"											},
		{kIOUSBTransactionTimeout,							"Request did not finish"												},
		{kIOUSBTransactionReturned,							"Request has been returned to the caller"								},
		{kIOUSBPipeStalled,									"Request returned a STALL"												},
		{kIOUSBInterfaceNotFound,							"Requested interface was not found"       								},
		{kIOUSBLowLatencyBufferNotPreviouslyAllocated,		"The buffer was not pre-allocated"										},
		{kIOUSBLowLatencyFrameListNotPreviouslyAllocated,	"The frame list was not pre-allocated"									},
		{kIOUSBHighSpeedSplitError,							"High Speed hub returned a split transaction error"						},
		{kIOUSBSyncRequestOnWLThread,						"Synchronous request was issued while holding from within the workloop"	},
		{kIOUSBHighSpeedSplitError,							"High Speed hub returned a split transaction error"						},
		{kIOUSBLinkErr,										"USB controller error"       											},
		{kIOUSBNotSent1Err,									"The isoch transfer did not occur, scheduled too late"					},
		{kIOUSBNotSent2Err,									"The isoch transfer did not occur, scheduled too late"					},
		{kIOUSBBufferUnderrunErr,							"Buffer Underrun (Host hardware failure on data out, PCI busy?"			},
		{kIOUSBBufferOverrunErr,							"Buffer Overrun (Host hardware failure on data out, PCI busy?"			},
		{kIOUSBReserved2Err,								"Reserved error #1"														},
		{kIOUSBReserved1Err,								"Reserved error #2"        												},
		{kIOUSBWrongPIDErr,									"Pipe stall, Bad or wrong PID"        									},
		{kIOUSBPIDCheckErr	,								"Pipe stall, PID CRC error"        										},
		{kIOUSBDataToggleErr,								"Pipe stall, Bad data toggle"       									},
		{kIOUSBBitstufErr,									"Pipe stall, bitstuffing"												},
		{kIOUSBCRCErr,										"Pipe stall, bad CRC"        											},
		{kIOUSBDeviceTransferredToCompanion,				"Device transferred to another controller"     							},
		{kIOUSBClearPipeStallNotRecursive,					"Attempting to clear stall while one is pending"     					},
		{kIOUSBDevicePortWasNotSuspended,					"Issued a Suspend but the port was not suspended"     					},
		{kIOUSBEndpointCountExceeded,						"Controller does not support more endpoints"     						},
		{kIOUSBDeviceCountExceeded,							"Controller does not support more devices"     							},
		{kIOUSBStreamsNotSupported,							"Controller does not support USB 3 streams"     						},
		{0,													NULL																	}
	};
	
	
	const char *	returnName = IOFindNameForValue(rtn, USBReturn_values);
	
	if ( strstr(returnName, "UNDEFINED") != NULL)
		returnName = super::stringFromReturn(rtn);
	
	return returnName;
}



