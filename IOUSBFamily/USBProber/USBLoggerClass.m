/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

#import "USBLoggerClass.h"

//================================================================================================
//   Globals
//================================================================================================
//
bool			gSetDebuggerLevel = true;
UInt32			gDebuggerLevel = 5;
bool			gSetDebuggerType = true;
UInt32			gDebuggerType = 2;
bool			gDisableLogging = false;
mach_port_t 		gQPort = 0;
IODataQueueMemory *	gMyQueue = NULL;
io_connect_t		gControllerUserClientPort = 0;
io_connect_t		gKLogUserClientPort = 0;
mach_port_t 		gMasterPort = 0;

FILE *dumpingFile;

@implementation usbLoggerClass

- (IBAction)clearOutput:(id)sender
{
    [usbloggerOutput setString:@""];
}

- (IBAction)filterOutput:(id)sender
{
    NSMutableString *finalString = [[NSMutableString alloc] init];
    [usbloggerFilteredOutput setString:@""];
    
    if ([[filterTextField1 stringValue] length] == 0 && [[filterTextField2 stringValue] length] == 0) {
        // nothing to filter, so show all output
        //[usbloggerFilteredOutput setString:[usbloggerOutput string]];
        [finalString appendString:[usbloggerOutput string]];
    }
    else {
        NSArray *linesToFilter;
        BOOL filter1IsFound, filter2IsFound;
        UInt32 line;
        int searchType = [filterAndOrSelector indexOfSelectedItem];
            
        linesToFilter = [[usbloggerOutput string] componentsSeparatedByString:@"\n"];
        for (line=0; line < [linesToFilter count]; line++) {
            filter1IsFound = (([[linesToFilter objectAtIndex:line] rangeOfString:[filterTextField1 stringValue] options:NSCaseInsensitiveSearch].location != NSNotFound) || [[filterTextField1 stringValue] length] == 0);
            filter2IsFound = (([[linesToFilter objectAtIndex:line] rangeOfString:[filterTextField2 stringValue] options:NSCaseInsensitiveSearch].location != NSNotFound) || [[filterTextField2 stringValue] length] == 0);
            switch (searchType) {
                case 0: // AND the terms
                    if (filter1IsFound && filter2IsFound)
                        [finalString appendString:[NSString stringWithFormat:@"%@\n",[linesToFilter objectAtIndex:line]]];
                    break;
                case 1: // OR the terms
                    if (filter1IsFound || filter2IsFound)
                        [finalString appendString:[NSString stringWithFormat:@"%@\n",[linesToFilter objectAtIndex:line]]];
                    break;
            }
        }
    }
    [usbloggerFilteredOutput replaceCharactersInRange:NSMakeRange([[usbloggerFilteredOutput string] length], 0) withString:finalString];
    [finalString release];
    [[usbloggerFilteredOutput window] makeFirstResponder:usbloggerFilteredOutput];
}

- (IBAction)startLogging:(id)sender
{
    if (!shouldDisplayOutput) {
        shouldDumpToFile = NO;
        if ([usbloggerDumpToFile state])
        {
            NSSavePanel *sp;
            int result;
            NSCalendarDate *currentDate = [NSCalendarDate date];
            
            sp = [NSSavePanel savePanel];
            [sp setRequiredFileType:@"txt"];
            result = [sp runModalForDirectory:NSHomeDirectory() file:@"USB Log"];
            if (result != NSOKButton) {
                return;
            }
            dumpingFile = fopen ([[sp filename] cString],"w");
            shouldDumpToFile = YES;
            
            [currentDate setCalendarFormat:@"%b %d %H:%M:%S"];

            [self outputString:[NSString stringWithFormat:@"%@: Saving output to file %@\n\n",currentDate,[sp filename]] unconditionally:NO];
        }
        [usbloggerDumpToFile setEnabled:NO];
        [startLoggingButton setTitle:@"Stop"];
        [loggingLevelPopup setEnabled:NO];
        shouldDisplayOutput = YES;
        gSetDebuggerLevel = true;
        loggingLevelChanged = (loggingThreadIsRunning && (gDebuggerLevel != ([loggingLevelPopup indexOfSelectedItem] + 1)));
        gDebuggerLevel = [loggingLevelPopup indexOfSelectedItem] + 1;
        if (!loggingThreadIsRunning) {
            [NSThread detachNewThreadSelector: @selector(USBLogger:) toTarget:self withObject: nil];
        }
        else if (loggingLevelChanged) {
            SetDebuggerOptions(gDisableLogging, gSetDebuggerLevel, gDebuggerLevel, gSetDebuggerType, gDebuggerType);
            loggingLevelChanged = NO;
        }
    }
    else
    {
        shouldDisplayOutput = NO;
        if ([usbloggerDumpToFile state])
            fclose (dumpingFile);
        [usbloggerDumpToFile setEnabled:YES];
        [startLoggingButton setTitle:@"Start"];
        [loggingLevelPopup setEnabled:YES];
    }
}

- (IBAction)timeStamp:(id)sender
{
    NSCalendarDate *currentDate;
    NSString *date;
    currentDate = [NSCalendarDate date];
    [currentDate setCalendarFormat:@"%b %d %H:%M:%S"];
    date = [[NSString alloc] initWithFormat:@"\n\t\t¥¥¥¥ %@ ¥¥¥¥\n\n", currentDate];

    [self outputString:date unconditionally:YES];
    //[date release];
}

-(void)USBLogger:(id)anObject
{
    NSAutoreleasePool *pool=[[NSAutoreleasePool alloc] init];
    loggingThreadIsRunning = YES;
    // Create a master port
    //
    require_string( KERN_SUCCESS == IOMasterPort(NULL, &gMasterPort), errExit, "IOMasterPort returned error");
    // Now, let's open the user client to the USB Controller
    //
    require_string( KERN_SUCCESS == [self OpenUSBControllerUserClient], errExit, "Could not open USB user client");
    // If necessary, send configuration information to the logger
    //
    if ( (gSetDebuggerType) || gDisableLogging || gSetDebuggerLevel )
    {
        require_string( KERN_SUCCESS == SetDebuggerOptions(gDisableLogging, gSetDebuggerLevel, gDebuggerLevel, gSetDebuggerType, gDebuggerType), errExit, "Failed to send commands to USB user client");
    }
    // Now, let's start dumping the log!
    //
    require_string( KERN_SUCCESS == [self DumpUSBLog], errExit, "Could not open dump the USB Log");
    
errExit:

    loggingThreadIsRunning = NO;
    shouldDisplayOutput = NO;
    [self CleanUp];
    if ([usbloggerDumpToFile state])
        fclose (dumpingFile);
    [usbloggerDumpToFile setEnabled:YES];
    [startLoggingButton setTitle:@"Start"];
    [loggingLevelPopup setEnabled:YES];
    [pool release];
}

//================================================================================================
//   OpenUSBControllerUserClient
//================================================================================================
//
-(kern_return_t)OpenUSBControllerUserClient
{
    kern_return_t 	kr;
    io_iterator_t 	iter;
    io_service_t	service;

    char *className = "IOUSBController";
        kr = IOServiceGetMatchingServices( gMasterPort,
                                           IOServiceMatching(className ), &iter);
        if(kr != KERN_SUCCESS)
        {
            [self outputString:[NSString stringWithFormat:@"usblogger: [ERR] IOServiceGetMatchingServices for USB Controller returned %x\n", kr] unconditionally:NO];
            return kr;
        }
            while ((service = IOIteratorNext(iter)) != NULL)
            {
                        kr = IOServiceOpen(service, mach_task_self(), 0, &gControllerUserClientPort);
                if(kr != KERN_SUCCESS)
                {
                    [self outputString:[NSString stringWithFormat:@"usblogger: [ERR] Could not IOServiceOpen on USB Controller client %x\n", kr] unconditionally:NO];
                    goto Exit;
                }
                IOObjectRelease(service);
                break;
            }
            // Enable logging
            //
        kr = IOConnectMethodScalarIScalarO( gControllerUserClientPort, kUSBControllerUserClientEnableLogger, 1, 0, 1);

Exit:
            IOObjectRelease(iter);
            return kr;
}

//================================================================================================
//   SetDebuggerOptions
//================================================================================================
//
kern_return_t SetDebuggerOptions( bool disableLogging, bool setLevel, UInt32 level, bool setType, UInt32 type )
{
    kern_return_t	kr = KERN_SUCCESS;
        if ( disableLogging )
            kr = IOConnectMethodScalarIScalarO( gControllerUserClientPort, kUSBControllerUserClientEnableLogger, 1, 0, 0);
            if(kr != KERN_SUCCESS)
            {
                printf("usblogger: [ERR] Could not disable logger (%x)\n", kr);
                return kr;
            }
                if ( setLevel )
                    kr = IOConnectMethodScalarIScalarO( gControllerUserClientPort, kUSBControllerUserClientSetDebuggingLevel, 1, 0, level);
                if(kr != KERN_SUCCESS)
                {
                    printf("usblogger: [ERR] Could not set debugging level (%x)\n", kr);
                    return kr;
                }
                    if ( setType )
                        kr = IOConnectMethodScalarIScalarO( gControllerUserClientPort, kUSBControllerUserClientSetDebuggingType, 1, 0, type);
                    if(kr != KERN_SUCCESS)
                    {
                        printf("usblogger: [ERR] Could not set debugging level (%x)\n", kr);
                        return kr;
                    }
                        return kr;
}

//================================================================================================
//   DumpUSBLog
//================================================================================================
//
-(kern_return_t)DumpUSBLog
{
    io_iterator_t	iter;
    io_service_t	service;
    kern_return_t 	kr;
    kern_return_t 	res;
    vm_size_t 		bufSize;
    UInt32 		memSize;
    unsigned char 	QBuffer[BUFSIZE];
    char 		msgBuffer[BUFSIZE];
    struct timeval 	msgTime;
    struct timeval 	initialTime;
    struct timezone 	tz;
    int 		level, tag;
    char *className = "com_apple_iokit_KLog";

    [self outputString:[NSString stringWithFormat:@"Timestamp Lvl  \tMessage\n--------- ---\t--------------------------------------\n"] unconditionally:NO];
    gettimeofday(&initialTime, &tz);
    kr = IOServiceGetMatchingServices( gMasterPort,
                                        IOServiceMatching(className ), &iter);
    if(kr != KERN_SUCCESS)
    {
        [self outputString:[NSString stringWithFormat:@"usblogger: [ERR] IOMasterPort returned %x\n", kr] unconditionally:NO];
        return kr;
    }
        while ((service = IOIteratorNext(iter)) != NULL)
        {
                    kr = IOServiceOpen(service, mach_task_self(), 0, &gKLogUserClientPort);
            if(kr != KERN_SUCCESS)
            {
                [self outputString:[NSString stringWithFormat:@"usblogger: [ERR] Could not open object %d\n", kr] unconditionally:NO];
                IOObjectRelease(iter);
                return kr;
            }
            IOObjectRelease(service);
            break;
        }
        IOObjectRelease(iter);
        //mach port for IODataQueue
    gQPort = IODataQueueAllocateNotificationPort();
    if(gQPort == MACH_PORT_NULL)
    {
        [self outputString:[NSString stringWithFormat:@"LogUser: [ERR] Could not allocate DataQueue notification port\n"] unconditionally:NO];
        return kIOReturnNoMemory;
    }
        kr = IOConnectSetNotificationPort(gKLogUserClientPort, 0, gQPort, 0);
    if(kr != KERN_SUCCESS)
    {
        [self outputString:[NSString stringWithFormat:@"LogUser: [ERR] Could not set notification port (%x)\n",kr] unconditionally:NO];
        return kIOReturnNoMemory;
    }
        //map memory
    kr = IOConnectMapMemory(gKLogUserClientPort, 0, mach_task_self(), (vm_address_t*)&gMyQueue, &bufSize, kIOMapAnywhere);
    if(kr != KERN_SUCCESS)
    {
        [self outputString:[NSString stringWithFormat:@"LogUser: [ERR] Could not connect memory map\n"] unconditionally:NO];
        return kIOReturnNoMemory;
    }
        //Tell the logger UserClient to activate its data queue
    kr = IOConnectMethodScalarIScalarO(gKLogUserClientPort, 0, 1, 0, Q_ON);
    if(kr != KERN_SUCCESS)
    {
        [self outputString:[NSString stringWithFormat:@"LogUser: [ERR] Could not open data queue\n"] unconditionally:NO];
        return kr;
    }

    while( TRUE )
    {
        //reset size of expected buffer
        memSize = sizeof(msgBuffer);
        //if no data available in queue, wait on port...
        if(!IODataQueueDataAvailable(gMyQueue))
        {
            res = IODataQueueWaitForAvailableData(gMyQueue, gQPort);
            if(res != KERN_SUCCESS)
            {
                [self outputString:[NSString stringWithFormat:@"ERR: [IODataQueueWaitForAvailableData] res"] unconditionally:NO];
                continue;
            }
        }
        //once dequeued check result for errors
        res = IODataQueueDequeue(gMyQueue, (void*)QBuffer, &memSize);
        if(res != KERN_SUCCESS)
        {
            continue;
        }
        //pull in the timestamp stuff and set a null for %s access
        memcpy(&msgTime, QBuffer, _T_STAMP);
        memcpy(&tag, QBuffer+_T_STAMP, _TAG);
        memcpy(&level, QBuffer+_T_STAMP+_TAG, _LEVEL);
        QBuffer[memSize+1] = 0;
        
        [self outputString:[NSString stringWithFormat:@"%5d.%3.3d [%d]\t%.*s\n",(msgTime.tv_sec-initialTime.tv_sec),(msgTime.tv_usec/1000), level, (int)(memSize-_OFFSET), QBuffer+_OFFSET] unconditionally:NO];

    }
    return KERN_SUCCESS;
}

//================================================================================================
//   CleanUp
//================================================================================================
//
-(void)CleanUp
{
    if ( gQPort )
        IOConnectRelease(gQPort);
    if ( gKLogUserClientPort )
    {
        //Tell the logger UserClient to deactivate its data queue
        IOConnectMethodScalarIScalarO(gKLogUserClientPort, 0, 1, 0, Q_OFF);
        if ( gMyQueue )
            IOConnectUnmapMemory(gKLogUserClientPort, 0, mach_task_self(), (vm_address_t)&gMyQueue);
        IOConnectRelease(gKLogUserClientPort);
    }
    if ( gMasterPort )
        mach_port_deallocate(mach_task_self(), gMasterPort);
        if (gControllerUserClientPort)
        {
            IOConnectRelease(gControllerUserClientPort);
        }
}

+(void)CleanUp // So logger can be cleaned up externally (i.e. from the main controller @ Quit);
{
    if ( gQPort )
        IOConnectRelease(gQPort);
    if ( gKLogUserClientPort )
    {
        //Tell the logger UserClient to deactivate its data queue
        IOConnectMethodScalarIScalarO(gKLogUserClientPort, 0, 1, 0, Q_OFF);
        if ( gMyQueue )
            IOConnectUnmapMemory(gKLogUserClientPort, 0, mach_task_self(), (vm_address_t)&gMyQueue);
        IOConnectRelease(gKLogUserClientPort);
    }
    if ( gMasterPort )
        mach_port_deallocate(mach_task_self(), gMasterPort);
    if (gControllerUserClientPort)
    {
        IOConnectRelease(gControllerUserClientPort);
    }
}

- (void)outputString:(NSString *)string unconditionally:(BOOL)unconditionally
{
    if (!shouldDisplayOutput && !unconditionally)
        return;
    if (shouldDumpToFile) {
        fprintf(dumpingFile, [string cString]);
        fflush(dumpingFile);
    }

    // send the output string to the main thread, so it can update the UI
    // It's not a good idea to update NSTextViews from a secondary thread, plus the NSTextView needs time to
    // refresh. That is why we do this.
    [[NSNotificationCenter defaultCenter] postNotificationName:@"com.apple.USBProber.usblogger" object:string];
    return;
}

@end
