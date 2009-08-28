//
//  GetEntry.m
//  IrDA Status
//
//  Created by jwilcox on Mon Apr 23 2001.
//  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
//

#import "GetEntry.h"
#import "LogItem.h"
#import "preferences.h"

@implementation GetEntry

static	char			gBigBuffer[1024*1024];	// fix: do two passes and allocate the appropriate size
static	IrDALogInfo		gInfo;					// pointers and sizes returned directly

void OutputBuffer(IrDALogHdr *obj, IrDAEventDesc *events, char *msgs, NSMutableArray *logItems)
{	
	IrDAEventDescPtr	eventPtr;									
	Boolean 			oldTracingFlag;
	
	if (obj->fWrapped == false &&					// if we've not wrapped around and
		obj->fEventIndex == obj->fPrintIndex) {		// print index == new event index, then nothing new
		return;
	}
	oldTracingFlag = obj->fTracingOn;			// Save old value of tracing enabled bit
	obj->fTracingOn = false;					// temporarily turn off tracing during dcmd (let's not race)
	if (obj->fWrapped) {						// if adding new entries wrapped over print index
		obj->fPrintIndex = obj->fEventIndex;	// then start printing at next "avail" entry
	}
	if( obj->fPrintIndex >= obj->fNumEntries )	// sanity check only, this shouldn't happen
		obj->fPrintIndex = 0;
	do{						
		char			*msg;
							
		eventPtr = &obj->fEventBuffer[obj->fPrintIndex];
		eventPtr = (eventPtr - gInfo.eventLog) + events;		// adjust event ptr for kernel/user address space change
		msg = (eventPtr->msg - gInfo.msgBuffer) + msgs;			// adjust msg ptr too
		[logItems addObject: [LogItem logItemWithValues: eventPtr->timeStamp: eventPtr->data1: eventPtr->data2: [NSString stringWithCString:msg]]];
		obj->fPrintIndex++;
		
		if( obj->fPrintIndex >= obj->fNumEntries )	// wrap print index at end of circular buffer
			obj->fPrintIndex = 0;
			
	} while((obj->fPrintIndex != obj->fEventIndex) );
	
	obj->fPrintIndex = obj->fEventIndex;	// FLUSH PENDING LOG ENTRIES if aborted
											// we shouldn't do this once we get a -C flag :-)
	obj->fWrapped = false;					// reset wrapped flag
	obj->fTracingOn = oldTracingFlag;		// restore tracing state (enable again)
}

// return true if there are messages to dump
Boolean CheckLog(IrDALogHdr *obj)
{
	if (obj->fWrapped == false &&					// if we've not wrapped around and
		obj->fEventIndex == obj->fPrintIndex) {		// print index == new event index, then nothing new
		return false;
	}
	return true;
}

void DumpLog(NSMutableArray *logItems)
{
	IrDALogHdr 		*hdr	= (IrDALogHdr *)&gBigBuffer[0];
	IrDAEventDesc	*events	= (IrDAEventDescPtr)&gBigBuffer[gInfo.hdrSize];
	char 			*msgs	= (char *)&gBigBuffer[gInfo.hdrSize + gInfo.eventLogSize];
	
	if (CheckLog(hdr)) {				// if any new entries
		OutputBuffer(hdr, events, msgs, logItems);
	}
}

kern_return_t doCommand(io_connect_t con, unsigned char commandID, void *inputData, unsigned long inputDataSize, void *outputData, size_t *outputDataSize)
{
	kern_return_t   err = KERN_SUCCESS;
	//mach_msg_type_number_t  outSize = outputDataSize;
	IrDACommandPtr command = NULL;
	
	// Creates a command block:
	command = (IrDACommandPtr)malloc (inputDataSize + sizeof (unsigned char));
	if (!command)
		return KERN_FAILURE;
	command->commandID = commandID;
	// Adds the data to the command block:
	if ((inputData != NULL) && (inputDataSize != 0))
		memcpy(command->data, inputData, inputDataSize);
	// Now we can (hopefully) transfer everything:
	err = IOConnectCallStructMethod(
			con,
			0,										/* method index */
			(char *) command,						/* input[] */
			inputDataSize+sizeof(unsigned char),	/* inputCount */
			(char *) outputData,					/* output */
			outputDataSize);						/* buffer size, then result */
	free (command);
	return err;
}

static kern_return_t closeDevice(io_connect_t con)
{
    return IOServiceClose(con);
}

static kern_return_t openDevice(io_object_t obj, io_connect_t * con)
{
    return IOServiceOpen(obj, mach_task_self(), 123, con);
}

/* ==========================================
 * Look through the registry and search for an
 * IONetworkInterface objects with the given
 * name.
 * If a match is found, the object is returned.
 * =========================================== */
io_object_t getInterfaceWithName(mach_port_t masterPort, const char *className)
{
    kern_return_t	kr;
    io_iterator_t	ite;
    io_object_t		obj = 0;

    kr = IORegistryCreateIterator(masterPort, kIOServicePlane, true, &ite);
    if (kr != kIOReturnSuccess) {
        printf("IORegistryCreateIterator() error %08lx\n", (unsigned long)kr);
        return 0;
    }
    while ((obj = IOIteratorNext(ite))) {
        if (IOObjectConformsTo(obj, (char *) className)) {
            break;
        }
		else {
			io_name_t name;
			kern_return_t rc;
			rc = IOObjectGetClass(obj, name);
		}
        IOObjectRelease(obj);
        obj = 0;
    }
    IOObjectRelease(ite);
    return obj;
}

+ (NSString *) GetCurrentDriverName{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	return ([defaults objectForKey: DefaultDriverKey]);
}

+ (void) getEntry: (NSMutableArray *)logItems{
    kern_return_t	kr;
    io_object_t		netif;
    io_connect_t	conObj;
    mach_port_t		masterPort;
	NSString		*driverName = [self GetCurrentDriverName];
	const char		*driverCStringName = [driverName cString];

    // Get master device port
    //
    kr = IOMasterPort(bootstrap_port, &masterPort);
    if (kr != KERN_SUCCESS) {
		printf("IOMasterPort() failed: %08lx\n", (unsigned long)kr);
		return;
    }
    netif = getInterfaceWithName(masterPort, driverCStringName);
    if (netif) {
		kr = openDevice(netif, &conObj);
		if (kr == kIOReturnSuccess) {
			UInt32 inbuf[2];			// big buffer address passed to userclient
			size_t infosize;
		
			inbuf[0] = (UInt32)&gBigBuffer[0];
			inbuf[1] = sizeof(gBigBuffer);
			infosize = sizeof(IrDALogInfo);

			kr = doCommand(conObj, 0x12, &inbuf, sizeof(inbuf), &gInfo, &infosize);
			if (kr == kIOReturnSuccess) {
				DumpLog(logItems);
			}
			else{
				printf("kr is %d\n", kr);
			}
			closeDevice(conObj);
      	}
		IOObjectRelease(netif);
    }
	else{
		NSLog(@"Unable to find the Requested Driver");
	}
}

@end
