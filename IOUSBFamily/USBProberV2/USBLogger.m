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
            [NSException raise:@"USB ProberBadListener" format:@"Listener must be non-nil"];
            [self dealloc];
            self = nil;
        }  else {
            // no problem
            IOReturn        kr;
            
            kr = IOMasterPort(MACH_PORT_NULL, &_gMasterPort);
            if (kr != KERN_SUCCESS) {
                NSLog(@"USB Prober: IOMasterPort() returned %d\n", kr);
                [self dealloc];
                self = nil;
            } else {
                kr = [self OpenUSBControllerUserClient];
                if (kr != KERN_SUCCESS) {
                    NSLog(@"USB Prober: -OpenUSBControllerUserClient returned %d\n", kr);
                    [self dealloc];
                    self = nil;
                } else {
                    kr = [self setDebuggerOptions:-1 setLevel:true level:_loggingLevel setType:true type:2];
                    if (kr != KERN_SUCCESS) {
                        NSLog(@"USB Prober: -setDebuggerOptions returned %d\n", kr);
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
        [self callUSBControllerUserClient:_gKLogUserClientPort methodIndex:0  inParam:Q_OFF];
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
        [_listener usbLoggerTextAvailable:[NSString stringWithFormat:@"USB Prober: [ERR] IOServiceGetMatchingServices for USB Controller returned %x\n", kr] forLevel:0];
        return kr;
    }
    while ((service = IOIteratorNext(iter)) != IO_OBJECT_NULL)
    {
        kr = IOServiceOpen(service, mach_task_self(), 0, &_gControllerUserClientPort);
        if(kr != KERN_SUCCESS)
        {
            [_listener usbLoggerTextAvailable:[NSString stringWithFormat:@"USB Prober: [ERR] Could not IOServiceOpen on USB Controller client %x\n", kr] forLevel:0];
            IOObjectRelease(iter);
            return kr;
        }
        IOObjectRelease(service);
        break;
    }
    
    // Enable logging
    kr = [self callUSBControllerUserClient:_gControllerUserClientPort methodIndex:kUSBControllerUserClientEnableLogger  inParam:1];
    
    IOObjectRelease(iter);
    return kr;
}

- (kern_return_t)setDebuggerOptions:(int)shouldLogFlag setLevel:(bool)setLevel level:(UInt32)level setType:(bool)setType type:(UInt32)type {
    kern_return_t	kr = KERN_SUCCESS;
    if ( shouldLogFlag == 1)
        kr = [self callUSBControllerUserClient:_gControllerUserClientPort methodIndex:kUSBControllerUserClientEnableLogger  inParam:1];
    else if (shouldLogFlag == 0) {
        kr = [self callUSBControllerUserClient:_gControllerUserClientPort methodIndex:kUSBControllerUserClientClose  inParam:1];
        NSBeep();
    }
        
    
    if(kr != KERN_SUCCESS)
    {
        NSLog(@"USB Prober: [ERR] Could not enable/disable logger (%x)\n", kr);
        return kr;
    }
    
    if ( setLevel )
        kr = [self callUSBControllerUserClient:_gControllerUserClientPort methodIndex:kUSBControllerUserClientSetDebuggingLevel  inParam:level];
    if(kr != KERN_SUCCESS)
    {
        NSLog(@"USB Prober: [ERR] Could not set debugging level (%x)\n", kr);
        return kr;
    }
    
    if ( setType )
        kr = [self callUSBControllerUserClient:_gControllerUserClientPort methodIndex:kUSBControllerUserClientSetDebuggingType  inParam:type];
    if(kr != KERN_SUCCESS)
    {
        NSLog(@"USB Prober: [ERR] Could not set debugging type (%x)\n", kr);
        return kr;
    }
    return kr;
}

- (void)beginLogging {
    IOReturn kr = [self setDebuggerOptions:1 setLevel:false level:0 setType:false type:0];
    if (kr != KERN_SUCCESS) {
        NSLog(@"USB Prober: -beginLogging failed with: %d\n", kr);
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
        NSLog(@"USB Prober: -setLevel failed with: %d\n", kr);
    }
    _loggingLevel = level;
}

-(void)DumpUSBLog
{
    NSAutoreleasePool *		pool = [[NSAutoreleasePool alloc] init];
    io_iterator_t			iter;
    io_service_t			service;
    kern_return_t			kr;
    kern_return_t			res;
    vm_size_t				bufSize;
    UInt32					memSize;
    unsigned char			QBuffer[BUFSIZE];
    char					msgBuffer[BUFSIZE];
    struct timeval			initialTime;
	struct klog64_timeval	msgTime64;
    static struct klog64_timeval 	initialTime64;
	
    struct timezone			tz;
    int						level, tag;
    char *					className = "com_apple_iokit_KLog";
    static bool				calledOnce = false;
    
    if (calledOnce == false) {
        [_listener usbLoggerTextAvailable:@"Timestamp Lvl  \tMessage\n" forLevel:0];
        [_listener usbLoggerTextAvailable:@"--------- ---\t--------------------------------------\n" forLevel:0];
        gettimeofday(&initialTime, &tz);
		initialTime64.tv_sec = initialTime.tv_sec;
		initialTime64.tv_usec = initialTime.tv_usec;
        calledOnce = true;
    }
    
    kr = IOServiceGetMatchingServices( _gMasterPort, IOServiceMatching(className ), &iter);
    if(kr != KERN_SUCCESS)
    {
        [_listener usbLoggerTextAvailable:[NSString stringWithFormat:@"USB Prober: [ERR] IOMasterPort returned %x\n", kr] forLevel:0];
        [pool release];
        return;
    }
    while ((service = IOIteratorNext(iter)) != IO_OBJECT_NULL)
    {
        kr = IOServiceOpen(service, mach_task_self(), 0, &_gKLogUserClientPort);
        if(kr != KERN_SUCCESS)
        {
            [_listener usbLoggerTextAvailable:[NSString stringWithFormat:@"USB Prober: [ERR] Could not open object %d\n", kr] forLevel:0];
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
#if !__LP64__
	kr = IOConnectMapMemory(_gKLogUserClientPort, 0, mach_task_self(), (vm_address_t *)&_gMyQueue, &bufSize, kIOMapAnywhere);
#else
	kr = IOConnectMapMemory(_gKLogUserClientPort, 0, mach_task_self(), (mach_vm_address_t *)&_gMyQueue, (mach_vm_size_t *)&bufSize, kIOMapAnywhere);
#endif
	
    if(kr != KERN_SUCCESS)
    {
        [_listener usbLoggerTextAvailable:[NSString stringWithFormat:@"LogUser: [ERR] Could not connect memory map\n"] forLevel:0];
        [pool release];
        return;
    }
    //Tell the logger UserClient to activate its data queue
    kr = [self callUSBControllerUserClient:_gKLogUserClientPort methodIndex:0  inParam:Q_ON];
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
        res = IODataQueueDequeue(_gMyQueue, (void*)QBuffer, (uint32_t * )&memSize);
        if(res != KERN_SUCCESS)
        {
            continue;
        }
        //pull in the timestamp stuff and set a null for %s access
        memcpy(&msgTime64, QBuffer, _T_STAMP);
        memcpy(&tag, QBuffer+_T_STAMP, _TAG);
        memcpy(&level, QBuffer+_T_STAMP+_TAG, _LEVEL);
        QBuffer[memSize+1] = 0;
        
        logString = [[NSString alloc] initWithFormat:@"%5d.%3.3d [%d]\t%.*s\n",(uint32_t)(msgTime64.tv_sec-initialTime64.tv_sec),(uint32_t)(msgTime64.tv_usec/1000), level, (int)(memSize-_OFFSET), QBuffer+_OFFSET];
        if (_isLogging)
            [_listener usbLoggerTextAvailable:logString forLevel:level];
        [logString release];
        
    }
    
    [pool release];
    return; 
}

- (kern_return_t)callUSBControllerUserClient:(io_connect_t)port methodIndex:(UInt32)methodIndex inParam:(UInt32)inParam
{
    kern_return_t 	kr=KERN_SUCCESS;
#if defined(MAC_OS_X_VERSION_10_5) 
    UInt64 		inParam64=inParam;

    kr = IOConnectCallScalarMethod(port, methodIndex, &inParam64, 1, NULL, NULL);
#else
    kr = IOConnectMethodScalarIScalarO(port, methodIndex, 1, 0, inParam);
#endif // MAC_OS_X_VERSION_10_5

    return kr;
}


@end
