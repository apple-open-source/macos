/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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


#import "USBLogger.h"


@implementation USBLogger

- initWithListener:(id <USBLoggerListener> )listener level:(int)level {
    if (self = [super init]) {
        _listener = [listener retain];
        _loggingLevel = level;
        _isLogging = NO;
        
        if (listener == nil) {
            [NSException raise:@"USBLoggerBadListener" format:@"Listener must be non-nil"];
            [self dealloc];
            self = nil;
        }  else {
            // no problem
            IOReturn        kr;
            
            kr = IOMasterPort(NULL, &_gMasterPort);
            if (kr != KERN_SUCCESS) {
                NSLog(@"USBLogger: IOMasterPort() returned %d\n", kr);
                [self dealloc];
                self = nil;
            } else {
                kr = [self OpenUSBControllerUserClient];
                if (kr != KERN_SUCCESS) {
                    NSLog(@"USBLogger: -OpenUSBControllerUserClient returned %d\n", kr);
                    [self dealloc];
                    self = nil;
                } else {
                    kr = [self setDebuggerOptions:-1 setLevel:true level:_loggingLevel setType:true type:2];
                    if (kr != KERN_SUCCESS) {
                        NSLog(@"USBLogger: -setDebuggerOptions returned %d\n", kr);
                        [self dealloc];
                        self = nil;
                    }
                }
            }
        }
    }
    return self;
}

- (void)dealloc {
    [self invalidate];
    if ( _gQPort )
        IOConnectRelease(_gQPort);
    if ( _gKLogUserClientPort )
    {
        //Tell the logger UserClient to deactivate its data queue
        IOConnectMethodScalarIScalarO(_gKLogUserClientPort, 0, 1, 0, Q_OFF);
        if ( _gMyQueue )
            IOConnectUnmapMemory(_gKLogUserClientPort, 0, mach_task_self(), (vm_address_t)&_gMyQueue);
        IOConnectRelease(_gKLogUserClientPort);
    }
    if ( _gMasterPort )
        mach_port_deallocate(mach_task_self(), _gMasterPort);
    if (_gControllerUserClientPort)
    {
        IOConnectRelease(_gControllerUserClientPort);
    }
    [_listener release];
    [super dealloc];
}

- (kern_return_t)OpenUSBControllerUserClient
{
    kern_return_t 	kr;
    io_iterator_t 	iter;
    io_service_t	service;
    
    char *className = "IOUSBController";
    kr = IOServiceGetMatchingServices(_gMasterPort, IOServiceMatching(className ), &iter);
    if(kr != KERN_SUCCESS)
    {
        [_listener usbLoggerTextAvailable:[NSString stringWithFormat:@"USBLogger: [ERR] IOServiceGetMatchingServices for USB Controller returned %x\n", kr] forLevel:0];
        return kr;
    }
    while ((service = IOIteratorNext(iter)) != NULL)
    {
        kr = IOServiceOpen(service, mach_task_self(), 0, &_gControllerUserClientPort);
        if(kr != KERN_SUCCESS)
        {
            [_listener usbLoggerTextAvailable:[NSString stringWithFormat:@"usblogger: [ERR] Could not IOServiceOpen on USB Controller client %x\n", kr] forLevel:0];
            IOObjectRelease(iter);
            return kr;
        }
        IOObjectRelease(service);
        break;
    }
    
    // Enable logging
    kr = IOConnectMethodScalarIScalarO( _gControllerUserClientPort, kUSBControllerUserClientEnableLogger, 1, 0, 1);
    
    IOObjectRelease(iter);
    return kr;
}

- (kern_return_t)setDebuggerOptions:(int)shouldLogFlag setLevel:(bool)setLevel level:(UInt32)level setType:(bool)setType type:(UInt32)type {
    kern_return_t	kr = KERN_SUCCESS;
    if ( shouldLogFlag == 1)
        kr = IOConnectMethodScalarIScalarO( _gControllerUserClientPort, kUSBControllerUserClientEnableLogger, 1, 0, 1);
    else if (shouldLogFlag == 0) {
        kr = IOConnectMethodScalarIScalarO( _gControllerUserClientPort, kUSBControllerUserClientClose, 1, 0, 1);
        NSBeep();
    }
        
    
    if(kr != KERN_SUCCESS)
    {
        NSLog(@"USBLogger: [ERR] Could not enable/disable logger (%x)\n", kr);
        return kr;
    }
    
    if ( setLevel )
        kr = IOConnectMethodScalarIScalarO( _gControllerUserClientPort, kUSBControllerUserClientSetDebuggingLevel, 1, 0, level);
    if(kr != KERN_SUCCESS)
    {
        NSLog(@"USBLogger: [ERR] Could not set debugging level (%x)\n", kr);
        return kr;
    }
    
    if ( setType )
        kr = IOConnectMethodScalarIScalarO( _gControllerUserClientPort, kUSBControllerUserClientSetDebuggingType, 1, 0, type);
    if(kr != KERN_SUCCESS)
    {
        NSLog(@"USBLogger: [ERR] Could not set debugging type (%x)\n", kr);
        return kr;
    }
    return kr;
}

- (void)beginLogging {
    IOReturn kr = [self setDebuggerOptions:1 setLevel:false level:0 setType:false type:0];
    if (kr != KERN_SUCCESS) {
        NSLog(@"USBLogger: -beginLogging failed with: %d\n", kr);
    } else {
        _isLogging = YES;
        [NSThread detachNewThreadSelector:@selector(DumpUSBLog) toTarget:self withObject:nil];
    }
}

- (void)invalidate {
    _isLogging = NO;
}

- (void)setLevel:(int)level {
    IOReturn kr = [self setDebuggerOptions:-1 setLevel:true level:level setType:false type:0];
    if (kr != KERN_SUCCESS) {
        NSLog(@"USBLogger: -setLevel failed with: %d\n", kr);
    }
    _loggingLevel = level;
}

-(void)DumpUSBLog
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    io_iterator_t	iter;
    io_service_t	service;
    kern_return_t 	kr;
    kern_return_t 	res;
    vm_size_t 		bufSize;
    UInt32 		memSize;
    unsigned char 	QBuffer[BUFSIZE];
    char 		msgBuffer[BUFSIZE];
    struct timeval 	msgTime;
    static struct timeval 	initialTime;
    struct timezone 	tz;
    int 		level, tag;
    char *className = "com_apple_iokit_KLog";
    static bool calledOnce = false;
    
    if (calledOnce == false) {
        [_listener usbLoggerTextAvailable:@"Timestamp Lvl  \tMessage\n" forLevel:0];
        [_listener usbLoggerTextAvailable:@"--------- ---\t--------------------------------------\n" forLevel:0];
        gettimeofday(&initialTime, &tz);
        calledOnce = true;
    }
    
    kr = IOServiceGetMatchingServices( _gMasterPort, IOServiceMatching(className ), &iter);
    if(kr != KERN_SUCCESS)
    {
        [_listener usbLoggerTextAvailable:[NSString stringWithFormat:@"usblogger: [ERR] IOMasterPort returned %x\n", kr] forLevel:0];
        [pool release];
        return;
    }
    while ((service = IOIteratorNext(iter)) != NULL)
    {
        kr = IOServiceOpen(service, mach_task_self(), 0, &_gKLogUserClientPort);
        if(kr != KERN_SUCCESS)
        {
            [_listener usbLoggerTextAvailable:[NSString stringWithFormat:@"usblogger: [ERR] Could not open object %d\n", kr] forLevel:0];
            IOObjectRelease(iter);
            [pool release];
            return;
        }
        IOObjectRelease(service);
        break;
    }
    IOObjectRelease(iter);
    //mach port for IODataQueue
    _gQPort = IODataQueueAllocateNotificationPort();
    if(_gQPort == MACH_PORT_NULL)
    {
        [_listener usbLoggerTextAvailable:[NSString stringWithFormat:@"LogUser: [ERR] Could not allocate DataQueue notification port\n"] forLevel:0];
        [pool release];
        return;
    }
    kr = IOConnectSetNotificationPort(_gKLogUserClientPort, 0, _gQPort, 0);
    if(kr != KERN_SUCCESS)
    {
        [_listener usbLoggerTextAvailable:[NSString stringWithFormat:@"LogUser: [ERR] Could not set notification port (%x)\n",kr] forLevel:0];
        [pool release];
        return;
    }
    //map memory
    kr = IOConnectMapMemory(_gKLogUserClientPort, 0, mach_task_self(), (vm_address_t*)&_gMyQueue, &bufSize, kIOMapAnywhere);
    if(kr != KERN_SUCCESS)
    {
        [_listener usbLoggerTextAvailable:[NSString stringWithFormat:@"LogUser: [ERR] Could not connect memory map\n"] forLevel:0];
        [pool release];
        return;
    }
    //Tell the logger UserClient to activate its data queue
    kr = IOConnectMethodScalarIScalarO(_gKLogUserClientPort, 0, 1, 0, Q_ON);
    if(kr != KERN_SUCCESS)
    {
        [_listener usbLoggerTextAvailable:[NSString stringWithFormat:@"LogUser: [ERR] Could not open data queue\n"] forLevel:0];
        [pool release];
        return;
    }
    
    NSString *logString;
    while( _isLogging )
    {
        //reset size of expected buffer
        memSize = sizeof(msgBuffer);
        //if no data available in queue, wait on port...
        if(!IODataQueueDataAvailable(_gMyQueue))
        {
            res = IODataQueueWaitForAvailableData(_gMyQueue, _gQPort);
            if(res != KERN_SUCCESS)
            {
                [_listener usbLoggerTextAvailable:[NSString stringWithFormat:@"ERR: [IODataQueueWaitForAvailableData] res\n"] forLevel:0];
                continue;
            }
        }
        //once dequeued check result for errors
        res = IODataQueueDequeue(_gMyQueue, (void*)QBuffer, &memSize);
        if(res != KERN_SUCCESS)
        {
            continue;
        }
        //pull in the timestamp stuff and set a null for %s access
        memcpy(&msgTime, QBuffer, _T_STAMP);
        memcpy(&tag, QBuffer+_T_STAMP, _TAG);
        memcpy(&level, QBuffer+_T_STAMP+_TAG, _LEVEL);
        QBuffer[memSize+1] = 0;
        
        logString = [[NSString alloc] initWithFormat:@"%5d.%3.3d [%d]\t%.*s\n",(msgTime.tv_sec-initialTime.tv_sec),(msgTime.tv_usec/1000), level, (int)(memSize-_OFFSET), QBuffer+_OFFSET];
        if (_isLogging)
            [_listener usbLoggerTextAvailable:logString forLevel:level];
        [logString release];
        
    }
    
    [pool release];
    return;
}

@end
