/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#include <AssertMacros.h>
#include <pthread.h>
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFBase.h>
#include <IOKit/network/IOUserEthernetResourceUserClient.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOCFSerialize.h>
#include "IOUserEthernetController.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sys_domain.h>
#include <sys/kern_control.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <fcntl.h>

#define EN_CONTROL_NAME     "com.apple.userspace_ethernet"
#define EN_SOCKOPT_CONNECT	12

static IOEthernetControllerRef  __IOEthernetControllerCreate(
                                    CFAllocatorRef          allocator, 
                                    CFAllocatorContext *    context __unused);
static void                     __IOEthernetControllerRelease( CFTypeRef object );
static void                     __IOEthernetControllerRegister(void);
static void                     __IOEthernetControllerMachPortCallBack(CFMachPortRef port, void *msg, CFIndex size, void *info);
static void                     __IOEthernetControllerSetState(IOEthernetControllerRef controller, Boolean state);
static void                     __IOEthernetControllerSocketCallback(CFSocketRef s, CFSocketCallBackType type, CFDataRef address, const void *data, void *info);

static int __connect_to_kernel(void *cookie, size_t cookielen)
{
	int sock = -1;
	int result = 0;
	
	// Create a socket
	sock = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
	if (sock == -1) return sock;
	
	// Lookup the control ID for the given control name
	struct ctl_info	ctlinfo;
	bzero(&ctlinfo, sizeof(ctlinfo));
	strlcpy(ctlinfo.ctl_name, EN_CONTROL_NAME, sizeof(ctlinfo.ctl_name));
    
    result = ioctl(sock, CTLIOCGINFO, &ctlinfo);
	if (result == -1) {
		close(sock);
		return -1;
	}
	
	// Connect to the kernel control
	struct sockaddr_ctl	sac;
	bzero(&sac, sizeof(sac));
	sac.sc_len = sizeof(sac);
	sac.sc_family = AF_SYSTEM;
	sac.ss_sysaddr = AF_SYS_CONTROL;
	sac.sc_id = ctlinfo.ctl_id;
	sac.sc_unit = 0;
    
    result = connect(sock, (struct sockaddr*)&sac, sizeof(sac));
	if (result == -1) {
		close(sock);
		return -1;
	}
	
	// Bind the kernel control to an ethernet interface using the socket option
    result = setsockopt(sock, SYSPROTO_CONTROL, EN_SOCKOPT_CONNECT, cookie, cookielen);
	if (result == -1) {
		close(sock);
		return -1;
	}
    
    // make it no blocking
    result = fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);
	if (result == -1) {
		close(sock);
		return -1;
	}
	
	return sock;
}


CFTypeRef kIOEthernetHardwareAddress = CFSTR(kIOUserEthernetHardwareAddressKey);


typedef struct __IOEthernetController
{
    CFRuntimeBase                   cfBase;   // base CFType information

    io_service_t                    service;
    io_connect_t                    connect;
    CFDictionaryRef                 properties;
    
    io_object_t                     interface;
    
    CFRunLoopRef                    runLoop;
    CFStringRef                     runLoopMode;
    
    CFMachPortRef                   port;
    CFRunLoopSourceRef              portSource;

    CFSocketRef                     socket;
    CFRunLoopSourceRef              socketSource;
    Boolean                         enable;
    
    uint64_t                        cookie;
    
    struct {
        struct {
            IOEthernetControllerCallback callback;
            void *                       refcon;
        } enable, disable, packet;
                
    } callbacks;

} __IOEthernetController, *__IOEthernetControllerRef;

static const CFRuntimeClass __IOEthernetControllerClass = {
    0,                          // version
    "IOEthernetController",          // className
    NULL,                       // init
    NULL,                       // copy
    __IOEthernetControllerRelease,   // finalize
    NULL,                       // equal
    NULL,                       // hash
    NULL,                       // copyFormattingDesc
    NULL,
    NULL
};

static pthread_once_t   __controllerTypeInit            = PTHREAD_ONCE_INIT;
static CFTypeID         __kIOEthernetControllerTypeID   = _kCFRuntimeNotATypeID;
static mach_port_t      __masterPort                    = MACH_PORT_NULL;

//------------------------------------------------------------------------------
// __IOEthernetControllerRegister
//------------------------------------------------------------------------------
void __IOEthernetControllerRegister(void)
{
    __kIOEthernetControllerTypeID = _CFRuntimeRegisterClass(&__IOEthernetControllerClass);
    IOMasterPort(bootstrap_port, &__masterPort);
}

//------------------------------------------------------------------------------
// __IOEthernetControllerCreate
//------------------------------------------------------------------------------
IOEthernetControllerRef __IOEthernetControllerCreate(   
                                CFAllocatorRef              allocator, 
                                CFAllocatorContext *        context __unused)
{
    IOEthernetControllerRef  controller = NULL;
    void *          offset  = NULL;
    uint32_t        size;

    /* allocate service */
    size  = sizeof(__IOEthernetController) - sizeof(CFRuntimeBase);
    controller = (IOEthernetControllerRef)_CFRuntimeCreateInstance(allocator, IOEthernetControllerGetTypeID(), size, NULL);
    
    if (!controller)
        return NULL;

    offset = controller;
    bzero(offset + sizeof(CFRuntimeBase), size);
    
    return controller;
}

//------------------------------------------------------------------------------
// __IOEthernetControllerRelease
//------------------------------------------------------------------------------
void __IOEthernetControllerRelease( CFTypeRef object )
{
    IOEthernetControllerRef controller = (IOEthernetControllerRef)object;
    
    if ( controller->properties ) {
        CFRelease(controller->properties);
        controller->properties = NULL;
    }
    
    if ( controller->connect ) {
        IOServiceClose(controller->connect);
        controller->connect = 0;
    }
    
    if ( controller->service ) {
        IOObjectRelease(controller->service);
        controller->service = 0;
    }

    if ( controller->interface ) {
        IOObjectRelease(controller->interface);
        controller->interface = 0;
    }
        
    if ( controller->port ) {
        CFRelease(controller->port);
        controller->port = NULL;
    }

    if ( controller->portSource ) {
        CFRelease(controller->portSource);
        controller->portSource = NULL;
    }

    if ( controller->socket ) {
        CFSocketInvalidate(controller->socket);
        CFRelease(controller->socket);
        controller->socket = NULL;
    }
        
    if ( controller->socketSource ) {
        CFRelease(controller->socketSource);
        controller->socketSource = NULL;
    }
   
}

//------------------------------------------------------------------------------
// IOEthernetControllerGetTypeID
//------------------------------------------------------------------------------
CFTypeID IOEthernetControllerGetTypeID(void) 
{
    if ( _kCFRuntimeNotATypeID == __kIOEthernetControllerTypeID )
        pthread_once(&__controllerTypeInit, __IOEthernetControllerRegister);
        
    return __kIOEthernetControllerTypeID;
}

//------------------------------------------------------------------------------
// IOEthernetControllerCreate
//------------------------------------------------------------------------------
IOEthernetControllerRef IOEthernetControllerCreate(
                                CFAllocatorRef                  allocator, 
                                CFDictionaryRef                 properties)
{
    IOEthernetControllerRef     controller;
    CFDataRef                   data;
    kern_return_t               kr;
    
    do {
        if ( !properties )
            break;
            
        controller = __IOEthernetControllerCreate(allocator, NULL);
        if ( !controller )
            break;

        controller->service = IOServiceGetMatchingService(__masterPort, IOServiceMatching(kIOUserEthernetResourceKey));
        if ( controller->service == MACH_PORT_NULL )
            return NULL;
            
        kr = IOServiceOpen(controller->service, mach_task_self(), kIOUserEthernetResourceUserClientTypeController, &controller->connect);
        if ( kr != KERN_SUCCESS )
            break;
            
        // Establish the notification port first
        CFMachPortContext portContext = {0, controller, NULL, NULL, NULL};
        
        controller->port = CFMachPortCreate(kCFAllocatorDefault, __IOEthernetControllerMachPortCallBack, &portContext, NULL);
        if (!controller->port)
            break;
            
        kr = IOConnectSetNotificationPort(controller->connect, kIOUserEthernetResourceUserClientPortTypeState, CFMachPortGetPort(controller->port), 0);
        if ( kr != KERN_SUCCESS )
            break;

        controller->portSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, controller->port, 0);
        if ( !controller->portSource )
            break;

        // Create and start the little bugger with properties
        data = IOCFSerialize(properties, 0);
        if ( !data )
            break;
            
        uint32_t count = 1;
        kr = IOConnectCallMethod(   controller->connect, 
                                    kIOUserEthernetResourceUserClientMethodCreate,
                                    NULL,
                                    0,
                                    CFDataGetBytePtr(data), 
                                    CFDataGetLength(data),
                                    &controller->cookie,
                                    &count,
                                    NULL,
                                    NULL);
                                    
        CFRelease(data);

        if ( kr != KERN_SUCCESS )
            break;
            
        // connect and bind to the data socket
        int sock = __connect_to_kernel(&controller->cookie, sizeof(controller->cookie));
        if ( sock == -1 )
            break;
            
        CFSocketContext sockContext = {0, controller, NULL, NULL, NULL};
        controller->socket = CFSocketCreateWithNative(kCFAllocatorDefault, sock, kCFSocketReadCallBack, __IOEthernetControllerSocketCallback, &sockContext);
        if ( !controller->socket )
            break;

        controller->socketSource = CFSocketCreateRunLoopSource(kCFAllocatorDefault, controller->socket, 0);
        if ( !controller->socketSource )
            break;

        return controller;
    } while ( FALSE );
    
    if ( controller )
        CFRelease(controller);

    return NULL;
}

//------------------------------------------------------------------------------
// IOEthernetControllerGetIONetworkInterfaceObject
//------------------------------------------------------------------------------
io_object_t IOEthernetControllerGetIONetworkInterfaceObject(IOEthernetControllerRef controller)
{
    if ( !controller->interface ) {
    
        CFMutableDictionaryRef  matching    = NULL;
        
        do { 
            CFDictionaryRef propertyMatch   = NULL;
            CFStringRef     key             = NULL;
            CFNumberRef     number          = NULL;
            mach_port_t     masterPort      = MACH_PORT_NULL;
            kern_return_t   kr;
                        
            kr = IOMasterPort(MACH_PORT_NULL, &masterPort);
            if ( KERN_SUCCESS != kr )
                break;

            if ( !masterPort )
                break;
            
            matching = IOServiceMatching(kIONetworkInterfaceClass);
            if ( !matching )
                break;
                
            number = CFNumberCreate(kCFAllocatorDefault,kCFNumberSInt64Type,&controller->cookie);
            if ( !number )
                break;
                
            key = CFSTR(kIOUserEthernetCookieKey);
                
            propertyMatch = CFDictionaryCreate( kCFAllocatorDefault,
                        (const void **) &key, (const void **) &number, 1,
                        &kCFTypeDictionaryKeyCallBacks,
                        &kCFTypeDictionaryValueCallBacks );
                        
            CFRelease(number);
            
            if ( !propertyMatch )
                break;
                
            CFDictionarySetValue(matching, CFSTR(kIOPropertyMatchKey), propertyMatch);
            CFRelease(propertyMatch);
                
            controller->interface = IOServiceGetMatchingService(masterPort,matching);
            matching = NULL;  // consumed by IOServiceAddMatchingNotification

        } while (FALSE);
        
        if ( matching )
            CFRelease(matching);
    }
    
    return controller->interface;
}

//------------------------------------------------------------------------------
// IOEthernetControllerSetLinkStatus
//------------------------------------------------------------------------------
IOReturn IOEthernetControllerSetLinkStatus(
                                IOEthernetControllerRef         controller, 
                                Boolean                         state)
{
    uint64_t param = state;
    
    return IOConnectCallScalarMethod(controller->connect, kIOUserEthernetResourceUserClientMethodSetLinkStatus, &param, 1, NULL, NULL);
}
  
                                
//------------------------------------------------------------------------------
// __IOEthernetControllerSocketCallback
//------------------------------------------------------------------------------
void __IOEthernetControllerSocketCallback(CFSocketRef s, CFSocketCallBackType type, CFDataRef address __unused, const void *data __unused, void *info)
{
    IOEthernetControllerRef controller = (IOEthernetControllerRef)info;
                    
    if ( controller == NULL )
        return;
        
    if ( controller->socket != s )
        return;
        
    if ( kCFSocketReadCallBack != type )
        return;
        
    if ( controller->callbacks.packet.callback )
        (*controller->callbacks.packet.callback)(controller, controller->callbacks.packet.refcon);
}

//------------------------------------------------------------------------------
// IOEthernetControllerReadPacket
//------------------------------------------------------------------------------
CFIndex IOEthernetControllerReadPacket(
                                IOEthernetControllerRef         controller, 
                                uint8_t *                       buffer,
                                CFIndex                         bufferLength)
{
    return recv(CFSocketGetNative(controller->socket), (void *)buffer, bufferLength, 0);
}

//------------------------------------------------------------------------------
// IOEthernetControllerWritePacket
//------------------------------------------------------------------------------
IOReturn IOEthernetControllerWritePacket(
                                IOEthernetControllerRef         controller, 
                                const uint8_t *                 buffer,
                                CFIndex                         bufferLength)
{
    CFDataRef       data    = NULL;
    CFSocketError   error   = kCFSocketError;
    
    data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, buffer, bufferLength, kCFAllocatorNull);
    if ( data ) {
        error = CFSocketSendData(controller->socket, NULL, data, 0);
        CFRelease(data);
    }
    
    return (error == kCFSocketSuccess) ? kIOReturnSuccess : kIOReturnError;
}

//------------------------------------------------------------------------------
// IOEthernetControllerScheduleWithRunLoop
//------------------------------------------------------------------------------
void IOEthernetControllerScheduleWithRunLoop(
                                IOEthernetControllerRef         controller, 
                                CFRunLoopRef                    runLoop,
                                CFStringRef                     runLoopMode)
{
    CFRunLoopAddSource(runLoop, controller->portSource, runLoopMode);
    CFRunLoopAddSource(runLoop, controller->socketSource, runLoopMode);

    controller->runLoop     = runLoop;
    controller->runLoopMode = runLoopMode;
}

//------------------------------------------------------------------------------
// IOEthernetControllerUnscheduleFromRunLoop
//------------------------------------------------------------------------------
void IOEthernetControllerUnscheduleFromRunLoop(
                                IOEthernetControllerRef         controller, 
                                CFRunLoopRef                    runLoop,
                                CFStringRef                     runLoopMode)
{
    CFRunLoopRemoveSource(runLoop, controller->socketSource, runLoopMode);
    CFRunLoopRemoveSource(runLoop, controller->portSource, runLoopMode);

    controller->runLoop     = NULL;
    controller->runLoopMode = NULL;
}

//------------------------------------------------------------------------------
// __IOEthernetControllerMachPortCallBack
//------------------------------------------------------------------------------
void __IOEthernetControllerMachPortCallBack(CFMachPortRef port __unused, void *msg __unused, CFIndex size __unused, void *info)
{
    IOEthernetControllerRef controller  = (IOEthernetControllerRef)info;
    uint64_t                state       = 0;
    uint32_t                count       = 1;
    
    IOConnectCallScalarMethod(controller->connect, kIOUserEthernetResourceUserClientMethodGetState, NULL, 0, &state, &count);
    
    __IOEthernetControllerSetState(controller, state);
}

//------------------------------------------------------------------------------
// __IOEthernetControllerSetState
//------------------------------------------------------------------------------
void __IOEthernetControllerSetState(
                                IOEthernetControllerRef         controller, 
                                Boolean                         state)
{    
    if ( state == controller->enable)
        return;
        
    controller->enable = state;

    if ( state ) {
        if ( controller->callbacks.enable.callback )
         (*controller->callbacks.enable.callback)(controller, controller->callbacks.enable.refcon);
    }
    else {
        if ( controller->callbacks.disable.callback )
         (*controller->callbacks.disable.callback)(controller, controller->callbacks.disable.refcon);
    }
    
}

//------------------------------------------------------------------------------
// IOEthernetControllerRegisterEnableCallback
//------------------------------------------------------------------------------
void IOEthernetControllerRegisterEnableCallback(
                                IOEthernetControllerRef         controller, 
                                IOEthernetControllerCallback    callback, 
                                void *                          refcon)
{
    controller->callbacks.enable.callback  = callback;
    controller->callbacks.enable.refcon    = refcon;
}

//------------------------------------------------------------------------------
// IOEthernetControllerRegisterDisableCallback
//------------------------------------------------------------------------------
void IOEthernetControllerRegisterDisableCallback(
                                IOEthernetControllerRef         controller, 
                                IOEthernetControllerCallback    callback, 
                                void *                          refcon)
{
    controller->callbacks.disable.callback  = callback;
    controller->callbacks.disable.refcon    = refcon;
}

//------------------------------------------------------------------------------
// IOEthernetControllerRegisterPacketAvailableCallback
//------------------------------------------------------------------------------
void IOEthernetControllerRegisterPacketAvailableCallback(
                            IOEthernetControllerRef             controller, 
                            IOEthernetControllerCallback        callback, 
                            void *                              refcon)
{
    controller->callbacks.packet.callback  = callback;
    controller->callbacks.packet.refcon    = refcon;
}
