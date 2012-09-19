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

#include "AppleUSBOHCI.h"
#include <libkern/OSByteOrder.h>

#define nil (0)

#define super IOUSBControllerV3
#define self this

void AppleUSBOHCI::DoOptiFix(AppleOHCIEndpointDescriptorPtr pHead)
{
    AppleOHCIEndpointDescriptorPtr 		pED;
    AppleOHCIGeneralTransferDescriptorPtr 	pTD;
    int 					i = 0;

    for (i=0; i<27; i++) 
    {
        // allocate ED
        pED = AllocateED();
        pED->pLogicalNext = nil;
        //make ED and FA = 0
        pED->pShared->flags = 0;
        pTD = AllocateTD();
        if ( pTD == NULL )
        {
            break;
        }
        
        pED->pShared->tdQueueHeadPtr = HostToUSBLong ((UInt32) pTD->pPhysical);
        pTD->pShared->nextTD = pED->pShared->tdQueueTailPtr;
        pTD->pEndpoint = pED;
        pTD->pType = kOHCIOptiLSBug;
        pED->pShared->tdQueueTailPtr = HostToUSBLong ((UInt32) pTD->pPhysical);

        pED->pShared->nextED = pHead->pShared->nextED;
        pHead->pShared->nextED = HostToUSBLong((UInt32) pED->pPhysical);
        pHead = pED;
    }
}

void AppleUSBOHCI::OptiLSHSFix(void)
{

//  Do Opti Errata stuff here!!!!!!
    int i;
    AppleOHCIEndpointDescriptorPtr	pControlED;
    UInt32				controlState;

    _OptiOn = 1;

    //Turn off list processing
    controlState = USBToHostLong(_pOHCIRegisters->hcControl);
    _pOHCIRegisters->hcControl = HostToUSBLong(kOHCIFunctionalState_Operational << kOHCIHcControl_HCFSPhase);

    // wait a millisecond
    IOSleep(2);             

    // add dummy EDs to 8ms interrupts
    for ( i = 0; i< 8; i++)
        DoOptiFix(_pInterruptHead[48 + i].pHead);

    //Assign Tail of Control to point to head of Bulk

    pControlED = _pControlTail;
    pControlED->pShared->nextED = _pOHCIRegisters->hcBulkHeadED;

    // add dummy EDs to end of Control
    DoOptiFix(_pControlTail);

    // turn on everything previous but Bulk
    controlState &= ~kOHCIHcControl_BLE;
    OSWriteLittleInt32(&_pOHCIRegisters->hcControl, 0, controlState);

    //End of Opti Fix
}
