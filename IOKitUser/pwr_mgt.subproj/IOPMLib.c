/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include <sys/cdefs.h>

#include <mach/mach.h>
#include <IOKit/iokitmig.h> 	// mig generated
#include <mach/mach_init.h>

#include <IOKit/pwr_mgt/IOPM.h>
#include "IOPMLib.h"


io_connect_t IOPMFindPowerManagement( mach_port_t master_device_port )
{
    io_connect_t	fb;
    kern_return_t	kr;
    io_service_t	obj = MACH_PORT_NULL;

    obj = IORegistryEntryFromPath( master_device_port, kIOPowerPlane ":/IOPowerConnection/IOPMrootDomain");
    if( obj ) {
        kr = IOServiceOpen( obj,mach_task_self(), 0, &fb);
        if ( kr == kIOReturnSuccess ) {
            IOObjectRelease(obj);
            return fb;
        }
        IOObjectRelease(obj);
    }
    return 0;
}


IOReturn IOPMGetAggressiveness ( io_connect_t fb, unsigned long type, unsigned long * aggressiveness )
{
    mach_msg_type_number_t	len = 1;
    kern_return_t	err;
    int		param = type;

    err = io_connect_method_scalarI_scalarO( (io_connect_t) fb, kPMGetAggressiveness, &param, 1, (int *)aggressiveness, &len);

    if (err==KERN_SUCCESS)
      return kIOReturnSuccess;
    else
      return kIOReturnError;
}


IOReturn IOPMSetAggressiveness ( io_connect_t fb, unsigned long type, unsigned long aggressiveness )
{
   mach_msg_type_number_t	len = 1;
   kern_return_t	        err;
   int		                params[2];
   int                      ret_val = kIOReturnSuccess;

    params[0] = (int)type;
    params[1] = (int)aggressiveness;

    err = io_connect_method_scalarI_scalarO( (io_connect_t) fb, kPMSetAggressiveness, params, 2, &ret_val, &len);

    if (err==KERN_SUCCESS)
      return ret_val;
    else
      return kIOReturnError;
}


IOReturn IOPMSleepSystem ( io_connect_t fb )
{
    mach_msg_type_number_t	len = 1;
    kern_return_t  err;
    int             ret_val = kIOReturnSuccess;
    
    err = io_connect_method_scalarI_scalarO( (io_connect_t) fb, kPMSleepSystem, NULL, 0, &ret_val, &len);

    if (err==KERN_SUCCESS)
      return ret_val;
    else
      return kIOReturnError;
}


IOReturn IOPMCopyBatteryInfo( mach_port_t masterPort, CFArrayRef * oInfo )
{
    io_registry_entry_t         root_domain;
    io_registry_entry_t	        entry;
    IOReturn		            kr = kIOReturnUnsupported;
    
    // SMU case
    // Battery location is published under IOPMrootDomain
    root_domain = IORegistryEntryFromPath( masterPort, 
                        kIOPowerPlane ":/IOPowerConnection/IOPMrootDomain");
    if(!root_domain) return kIOReturnUnsupported;
    *oInfo = IORegistryEntryCreateCFProperty( root_domain, CFSTR(kIOBatteryInfoKey),
                    kCFAllocatorDefault, kNilOptions);
    IOObjectRelease(root_domain);
    
    if(*oInfo) {
        // Successfully read battery info from IOPMrootDomain
        return kIOReturnSuccess;
    } else {
        // Attempt to read battery data from PMU-generated battery node
        entry = IORegistryEntryFromPath( masterPort,
                                         kIODeviceTreePlane ":mac-io/battery");
        if( !entry) entry = IORegistryEntryFromPath( masterPort,
                                         kIODeviceTreePlane ":mac-io/via-pmu/battery");
        if(entry) 
        {
            *oInfo = IORegistryEntryCreateCFProperty( entry, CFSTR(kIOBatteryInfoKey),
                            kCFAllocatorDefault, kNilOptions);
            IOObjectRelease(entry);
        }
        if( *oInfo)
            kr = kIOReturnSuccess;
        return kr;
    }
}


io_connect_t IORegisterApp( void * refcon,
                            io_service_t theDriver,
                            IONotificationPortRef * thePortRef,
                            IOServiceInterestCallback callback,
                            io_object_t * notifier )

{
    io_connect_t		fb = MACH_PORT_NULL;
    kern_return_t		kr;
    
    *notifier = MACH_PORT_NULL;

    if ( theDriver != MACH_PORT_NULL ) {
        kr = IOServiceOpen(theDriver,mach_task_self(), 0, &fb);
        if ( kr == kIOReturnSuccess ) {
            if ( fb != MACH_PORT_NULL ) {
                kr = IOServiceAddInterestNotification(*thePortRef,theDriver,kIOAppPowerStateInterest,
                                                        callback,refcon,notifier);
                if ( kr == KERN_SUCCESS ) {
                    return fb;
                }
            }
        }
    }
    if ( fb != MACH_PORT_NULL ) {
        IOServiceClose(fb);
    }
    if ( *notifier != MACH_PORT_NULL ) {
        IOObjectRelease(*notifier);
    }
    return MACH_PORT_NULL;
}


io_connect_t IORegisterForSystemPower ( void * refcon,
                                        IONotificationPortRef * thePortRef,
                                        IOServiceInterestCallback callback,
                                        io_object_t * root_notifier )
{
    mach_port_t			master_device_port;
    io_connect_t		fb = MACH_PORT_NULL;
    IONotificationPortRef	notify = NULL;
    kern_return_t		kr;
    io_service_t		obj = MACH_PORT_NULL;
     
    *root_notifier = MACH_PORT_NULL;

    IOMasterPort(bootstrap_port,&master_device_port);
    notify = IONotificationPortCreate( master_device_port );
    obj = IORegistryEntryFromPath( master_device_port, kIOPowerPlane ":/IOPowerConnection/IOPMrootDomain");
    if( obj != MACH_PORT_NULL ) {
        kr = IOServiceOpen( obj,mach_task_self(), 0, &fb);
        if ( kr == kIOReturnSuccess ) {
            if ( fb != MACH_PORT_NULL ) {
                kr = IOServiceAddInterestNotification(notify,obj,kIOAppPowerStateInterest,
                                                        callback,refcon,root_notifier);
                IOObjectRelease(obj);
                if ( kr == KERN_SUCCESS ) {
                    *thePortRef = notify;
                    return fb;
                }
            }
        }
    }
    if ( obj != MACH_PORT_NULL ) {
        IOObjectRelease(obj);
    }
    if ( notify != MACH_PORT_NULL ) {
        IONotificationPortDestroy(notify);
    }
    if ( fb != MACH_PORT_NULL ) {
        IOServiceClose(fb);
    }
    if ( *root_notifier != MACH_PORT_NULL ) {
        IOObjectRelease(*root_notifier);
    }
    
    return MACH_PORT_NULL;
}


IOReturn IODeregisterApp ( io_object_t * notifier )
{
    if ( *notifier ) {
        IOObjectRelease(*notifier);
        *notifier = MACH_PORT_NULL;
    }
    return kIOReturnSuccess;
}


IOReturn IODeregisterForSystemPower ( io_object_t * root_notifier )
{
    if ( *root_notifier ) {
        IOObjectRelease(*root_notifier);
        *root_notifier = MACH_PORT_NULL;
    }
    return kIOReturnSuccess;
}


IOReturn IOAllowPowerChange ( io_connect_t kernelPort, long notificationID )
{
    kern_return_t               err;
    mach_msg_type_number_t	len = 0;

    err =  io_connect_method_scalarI_scalarO( kernelPort, kPMAllowPowerChange, (int *)&notificationID, 1, NULL, &len);

    if (err==KERN_SUCCESS)
      return kIOReturnSuccess;
    else
      return kIOReturnError;
}


IOReturn IOCancelPowerChange ( io_connect_t kernelPort, long notificationID )
{
    kern_return_t               err;
    mach_msg_type_number_t	len = 0;

    err = io_connect_method_scalarI_scalarO( kernelPort, kPMCancelPowerChange, (int *)&notificationID, 1, NULL, &len);

    if (err==KERN_SUCCESS)
      return kIOReturnSuccess;
    else
      return kIOReturnError;
}


boolean_t IOPMSleepEnabled ( void ) {
    mach_port_t			masterPort;
    io_registry_entry_t		root;
    kern_return_t		kr;
    boolean_t			flag = false;
    
    kr = IOMasterPort(bootstrap_port,&masterPort);

    if ( kIOReturnSuccess == kr ) {
        root = IORegistryEntryFromPath(masterPort,kIOPowerPlane ":/IOPowerConnection/IOPMrootDomain");
        if ( root ) {
            CFTypeRef data;
            
            data = IORegistryEntryCreateCFProperty(root,CFSTR("IOSleepSupported"),kCFAllocatorDefault,kNilOptions);
            if ( data ) {
                flag = true;
                CFRelease(data);
            }
            IOObjectRelease(root);
        }
    }
    return flag;
}
