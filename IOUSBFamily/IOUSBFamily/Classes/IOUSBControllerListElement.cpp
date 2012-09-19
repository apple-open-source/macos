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


#include <IOKit/usb/IOUSBControllerListElement.h>
#include <IOKit/usb/IOUSBLog.h>

#define IOUSBCONTROLLERLISTELEMENT_USE_KPRINTF 0

// Convert USBLog to use kprintf debugging
// The switch is in the header file, but the work is done here because the header is included by the companion controllers
#if IOUSBCONTROLLERLISTELEMENT_USE_KPRINTF
#undef USBLog
#undef USBError
void kprintf(const char *format, ...)
__attribute__((format(printf, 1, 2)));
#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= IOUSBCONTROLLERLISTELEMENT_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif

#define super OSObject
// -----------------------------------------------------------------
//		IOUSBControllerListElement
// -----------------------------------------------------------------
OSDefineMetaClass( IOUSBControllerListElement, OSObject )
OSDefineAbstractStructors(IOUSBControllerListElement, OSObject)


void
IOUSBControllerListElement::print(int level)
{
#pragma unused (level)
    USBLog(level, "IOUSBControllerListElement[%p]::print - _sharedPhysical[%p]", this, (void *)_sharedPhysical);
    USBLog(level, "IOUSBControllerListElement::print - _sharedLogical[%p]", _sharedLogical);
    USBLog(level, "IOUSBControllerListElement::print - _logicalNext[%p]", _logicalNext);
}


#undef super
#define super IOUSBControllerListElement
// -----------------------------------------------------------------
//		IOUSBControllerListElement
// -----------------------------------------------------------------
OSDefineMetaClass( IOUSBControllerIsochListElement, IOUSBControllerListElement )
OSDefineAbstractStructors( IOUSBControllerIsochListElement, IOUSBControllerListElement )



void
IOUSBControllerIsochListElement::print(int level)
{
    super::print(level);
    USBLog(level, "IOUSBControllerIsochListElement::print - _pEndpoint[%p]", _pEndpoint);
    USBLog(level, "IOUSBControllerIsochListElement::print - _pFrames[%p]", _pFrames);
    USBLog(level, "IOUSBControllerIsochListElement::print - completion[%p, %p, %p]", _completion.action, _completion.target, _completion.parameter);
    USBLog(level, "IOUSBControllerIsochListElement::print - _lowLatency[%s]", _lowLatency ? "true" : "false");
    USBLog(level, "IOUSBControllerIsochListElement::print - frameNumber[%qd]", _frameNumber);
    USBLog(level, "IOUSBControllerIsochListElement::print - frameIndex[%d]", (uint32_t)_frameIndex);
}


#undef super
#define super OSObject
OSDefineMetaClassAndStructors(IOUSBControllerIsochEndpoint, OSObject)
bool 
IOUSBControllerIsochEndpoint::init()
{
	toDoList =  toDoEnd = NULL;
	doneQueue = doneEnd = NULL;
	deferredQueue = deferredEnd = NULL;
	firstAvailableFrame = 0;
	maxPacketSize = 0;
	inSlot = 0;
	activeTDs = 0;
	onToDoList = 0;
	onDoneQueue = 0;
	scheduledTDs = 0;
	deferredTDs = 0;
	onProducerQ = 0;
	onReversedList = 0;
	inSlot = 0;
	functionAddress = 0;
	endpointNumber = 0;
	accumulatedStatus = kIOReturnSuccess;
	interval = 0;
	direction = 0;
	aborting = false;
	return true;
}
