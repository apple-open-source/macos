#import "IrDAStatus.h"
#import "Preferences.h"

@implementation IrDAStatusObj

 - (void) DumpResults:(IrDAStatus *)stats
{
	io_name_t	temp;
	BOOL		makeSound = NO;
	
	if (stats->connectionSpeed != oldStatus.connectionSpeed){
		oldStatus.connectionSpeed = stats->connectionSpeed;
		sprintf(temp, "%lu", stats->connectionSpeed); 
		[connectionSpeed setStringValue: [NSString stringWithCString: temp]];
	}

	if (stats->connectionState != oldStatus.connectionState){
		oldStatus.connectionState = stats->connectionState;
		switch (stats->connectionState) {
			case kIrDAStatusIdle:					[connectionState setStringValue: @"Idle"];			break;
			case kIrDAStatusDiscoverActive:			[connectionState setStringValue: @"Discovering"];	break;
			case kIrDAStatusConnected:				[connectionState setStringValue: @"Connected"];		break;
			case kIrDAStatusBrokenConnection:		[connectionState setStringValue: @"Beam Broken"];	makeSound = YES;	break;
			case kIrDAStatusOff:					[connectionState setStringValue: @"Off"];			break;
			case kIrDAStatusInvalid:				[connectionState setStringValue: @"Invalid"];		break;
		}
	}

	if (stats->crcErrors != oldStatus.crcErrors){
		oldStatus.crcErrors = stats->crcErrors;
		sprintf(temp, "%lu", stats->crcErrors); 
		[crcErrors setStringValue: [NSString stringWithCString: temp]];
	}
	
	if (stats->dataPacketsIn != oldStatus.dataPacketsIn){
		oldStatus.dataPacketsIn = stats->dataPacketsIn;
		sprintf(temp, "%lu", stats->dataPacketsIn); 
		[dataPacketsIn setStringValue: [NSString stringWithCString: temp]];
	}
	
	if (stats->dataPacketsOut != oldStatus.dataPacketsOut){
		oldStatus.dataPacketsOut = stats->dataPacketsOut;
		sprintf(temp, "%lu", stats->dataPacketsOut); 
		[dataPacketsOut setStringValue: [NSString stringWithCString: temp]];
	}
	
	if (stats->dropped != oldStatus.dropped){
		oldStatus.dropped = stats->dropped;
		sprintf(temp, "%lu", stats->dropped); 
		[dropped setStringValue: [NSString stringWithCString: temp]];
	}
	
	if (stats->iFrameRec != oldStatus.iFrameRec){
		oldStatus.iFrameRec = stats->iFrameRec;
		sprintf(temp, "%lu", stats->iFrameRec); 
		[iFrameRec setStringValue: [NSString stringWithCString: temp]];
	}
	
	if (stats->iFrameSent != oldStatus.iFrameSent){
		oldStatus.iFrameSent = stats->iFrameSent;
		sprintf(temp, "%lu", stats->iFrameSent); 
		[iFrameSent setStringValue: [NSString stringWithCString: temp]];
	}
	
	if (stats->ioErrors != oldStatus.ioErrors){
		oldStatus.ioErrors = stats->ioErrors;
		sprintf(temp, "%lu", stats->ioErrors); 
		[ioErrors setStringValue: [NSString stringWithCString: temp]];
	}
	
	[nickName setStringValue: [NSString stringWithCString: stats->nickName]];
	
	if (stats->protcolErrs != oldStatus.protcolErrs){
		makeSound = YES;
		oldStatus.protcolErrs = stats->protcolErrs;
		sprintf(temp, "%lu", stats->protcolErrs); 
		[protocolErrs setStringValue: [NSString stringWithCString: temp]];
	}
	
	if (stats->recTimeout != oldStatus.recTimeout){
		oldStatus.recTimeout = stats->recTimeout;
		sprintf(temp, "%lu", stats->recTimeout); 
		[recTimeout setStringValue: [NSString stringWithCString: temp]];
	}
	
	if (stats->rejRec != oldStatus.rejRec){
		oldStatus.rejRec = stats->rejRec;
		sprintf(temp, "%u", stats->rejRec); 
		[rejRec setStringValue: [NSString stringWithCString: temp]];
	}
	
	if (stats->rejSent != oldStatus.rejSent){
		oldStatus.rejSent = stats->rejSent;
		sprintf(temp, "%u", stats->rejSent); 
		[rejSent setStringValue: [NSString stringWithCString: temp]];
	}
	
	if (stats->resent != oldStatus.resent){
		oldStatus.resent = stats->resent;
		sprintf(temp, "%lu", stats->resent); 
		[resent setStringValue: [NSString stringWithCString: temp]];
	}
	
	if (stats->rnrRec != oldStatus.rnrRec){
		oldStatus.rnrRec = stats->rnrRec;
		sprintf(temp, "%u", stats->rnrRec); 
		[rnrRec setStringValue: [NSString stringWithCString: temp]];
	}
	
	if (stats->rnrSent != oldStatus.rnrSent){
		oldStatus.rnrSent = stats->rnrSent;
		sprintf(temp, "%u", stats->rnrSent); 
		[rnrSent setStringValue: [NSString stringWithCString: temp]];
	}
	
	if (stats->rrRec != oldStatus.rrRec){
		oldStatus.rrRec = stats->rrRec;
		sprintf(temp, "%lu", stats->rrRec); 
		[rrRec setStringValue: [NSString stringWithCString: temp]];
	}
	
	if (stats->rrSent != oldStatus.rrSent){
		oldStatus.rrSent = stats->rrSent;
		sprintf(temp, "%lu", stats->rrSent); 
		[rrSent setStringValue: [NSString stringWithCString: temp]];
	}
	
	if (stats->srejRec != oldStatus.srejRec){
		oldStatus.srejRec = stats->srejRec;
		sprintf(temp, "%u", stats->srejRec); 
		[srejRec setStringValue: [NSString stringWithCString: temp]];
	}
	
	if (stats->srejSent != oldStatus.srejSent){
		oldStatus.srejSent = stats->srejSent;
		sprintf(temp, "%u", stats->srejSent); 
		[srejSent setStringValue: [NSString stringWithCString: temp]];
	}
	
	if (stats->uFrameSent != oldStatus.uFrameSent){
		oldStatus.uFrameSent = stats->uFrameSent;
		sprintf(temp, "%lu", stats->uFrameSent); 
		[uFrameSent setStringValue: [NSString stringWithCString: temp]];
	}
	
	if (stats->uFrameRec != oldStatus.uFrameRec){
		oldStatus.uFrameRec = stats->uFrameRec;
		sprintf(temp, "%lu", stats->uFrameRec); 
		[uFrameRec setStringValue: [NSString stringWithCString: temp]];
	}
	
	if (stats->xmitTimeout != oldStatus.xmitTimeout){
		oldStatus.xmitTimeout = stats->xmitTimeout;
		sprintf(temp, "%lu", stats->xmitTimeout); 
		[xmitTimeout setStringValue: [NSString stringWithCString: temp]];
	}
	
	if (makeSound){		/* local Variable */
		NSBeep();
	}
}

kern_return_t closeDevice(io_connect_t con)
{
    return IOServiceClose(con);
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

kern_return_t openDevice(io_object_t obj, io_connect_t * con)
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

- (NSString *) GetCurrentDriverName{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	return ([defaults objectForKey: DefaultDriverKey]);
}

- (void) doTimer: (NSTimer *) myTimer{
    kern_return_t	kr;
    IrDAStatus		stats;

	size_t outputsize = sizeof(stats);

	kr = doCommand(conObj, kIrDAUserCmd_GetStatus, nil, 0, &stats, &outputsize);
	if (kr == kIOReturnSuccess) {
		[self DumpResults: &stats];
	}
	else{
		NSLog(@"IrDAStatus.m:doTimer: doCommand failed");
	}
}
- (void) InvalidateOldStatus
 {
	oldStatus.connectionState = -1;
	oldStatus.connectionSpeed = -1;
	oldStatus.dataPacketsIn = -1;
	oldStatus.dataPacketsOut = -1;
	oldStatus.crcErrors = -1;
	oldStatus.ioErrors = -1;
	oldStatus.recTimeout = -1;
	oldStatus.xmitTimeout = -1;
	oldStatus.iFrameRec = -1;
	oldStatus.iFrameSent = -1;
	oldStatus.uFrameRec = -1;
	oldStatus.uFrameSent = -1;
	oldStatus.dropped = -1;
	oldStatus.resent = -1;
	oldStatus.rrRec = -1;
	oldStatus.rrSent = -1;
	oldStatus.rnrRec = -1;
	oldStatus.rnrSent = -1;
	oldStatus.rejRec = -1;
	oldStatus.rejSent = -1;
	oldStatus.srejRec = -1;
	oldStatus.srejSent = -1;
	oldStatus.protcolErrs = -1;
 }
- (IBAction)StartTimer:(id)sender
{
	if (state){
		NSLog(@"Stop timer now!");
		[timer invalidate];
		// Display and keep track of state in button
		closeDevice(conObj);
		[sender setTitle: @"Start"];
		state = false;
	}
	else{
		mach_port_t		masterPort;
		io_object_t		netif;
		kern_return_t	kr;
		NSString		*driverName = [self GetCurrentDriverName];
		const char		*driverCStringName = [driverName cString];
		
		// Get master device port
		//
		kr = IOMasterPort(bootstrap_port, &masterPort);
		if (kr != KERN_SUCCESS) {
			return;
		}
		netif = getInterfaceWithName(masterPort, driverCStringName);
		if (netif) {
			kr = openDevice(netif, &conObj);
			if (kr == kIOReturnSuccess) {
				NSLog(@"Start timer now!");
				[self InvalidateOldStatus];
				timer = [NSTimer scheduledTimerWithTimeInterval: .1 target: self selector: @selector(doTimer:) userInfo: nil repeats: YES];
		
				// Display and keep track of state in button
				[sender setTitle: @"Stop"];
				
				state = true;
			}
			else{
				NSLog(@"IrDAStatus.m:StartTimer: openDevice failed");
			}
		IOObjectRelease(netif);
		}
		else{
			NSLog(@"IrDAStatus.m:StartTimer: getInterfaceWithName failed");
		}
	}
}

@end
