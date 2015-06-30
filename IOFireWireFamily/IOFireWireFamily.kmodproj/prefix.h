/*
 *  prefix.h
 *  IOFireWireFamily
 *
 *  Created by Niels on Tue Mar 11 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * $Log: not supported by cvs2svn $
 * Revision 1.3  2004/03/03 19:53:37  niels
 * fix prefix header for native target building
 *
 * Revision 1.2  2003/07/21 06:52:59  niels
 * merge isoch to TOT
 *
 * Revision 1.1.2.1  2003/07/01 20:54:07  niels
 * isoch merge
 *
 */

#ifdef __cplusplus

// system
#import <IOKit/system.h>
#import <IOKit/assert.h>

#import <IOKit/IOTypes.h>
#import <IOKit/IOService.h>
#import <IOKit/IOKitKeys.h>
#import <IOKit/IOBufferMemoryDescriptor.h>
#import <IOKit/IOTimerEventSource.h>
#import <IOKit/IOMessage.h>
#import <IOKit/IODeviceTreeSupport.h>
#import <IOKit/IOWorkLoop.h>
#import <IOKit/IOCommand.h>
#import <IOKit/IOLib.h>
#import <IOKit/IOLocks.h>
#import <IOKit/IOEventSource.h>
#import <IOKit/IOReturn.h>
#import <IOKit/IOUserClient.h>
#import <IOKit/OSMessageNotification.h>

#import <libkern/OSAtomic.h>

#import <libkern/c++/OSData.h>
#import <libkern/c++/OSArray.h>
#import <libkern/c++/OSObject.h>
#import <libkern/c++/OSString.h>
#import <libkern/c++/OSIterator.h>
#import <libkern/c++/OSSet.h>
#include <libkern/c++/OSCollectionIterator.h>

#import <IOKit/firewire/IOFireWireFamilyCommon.h>

#import <sys/proc.h>

#endif
