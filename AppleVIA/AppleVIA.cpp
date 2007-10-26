/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Josh de Cesare
 */


#include <ppc/proc_reg.h>

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOPlatformExpert.h>

#include "AppleVIA.h"


extern "C" {
extern void PE_Determine_Clock_Speeds(unsigned int via_addr,
				      int num_speeds,
				      unsigned long *speed_list);
}

static IOService * privCreateNub( IORegistryEntry * from);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IOService

OSDefineMetaClassAndStructors(AppleVIA, IOService);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool AppleVIA::start(IOService *provider)
{
  AppleVIADevice     *nub;
  IOInterruptAction  handler;
  IOReturn           error;
  IOMemoryMap        *viaMemoryMap;
  OSSymbol           *interruptControllerName;
  int                numSpeeds = 0;
  unsigned long      *speedList = 0;
  OSIterator		 *childIterator = NULL;
  childEntry 		 = NULL;
  pmuExists 		 = false;
      
  // Call super's start.
  if (!super::start(provider))
    return false;
  
   // see if 'pmu' already in device tree (newer machines)
  childIterator = provider->getChildIterator(gIODTPlane);
  if( childIterator != NULL )
	{
  	while ((childEntry = (IORegistryEntry *)(childIterator->getNextObject ())) != NULL) 
		{
		if (!strcmp ("pmu", childEntry->getName(gIOServicePlane))) 
			{
 			pmuExists = true;
			break;
			}
		}
	}
 // Do the mac-io-publishChildren the old low-risk way if no pmu in device tree
 if (!pmuExists)
	{
	if (provider->getProperty("preserveIODeviceTree") != 0)
		provider->callPlatformFunction ("mac-io-publishChildren", 0, (void*)this, (void*)0, (void*)0, (void*)0);
	}

  // Figure out what kind of via device nub to make.
  if (IODTMatchNubWithKeys(provider, "'via-cuda'"))
    viaDeviceType = kVIADeviceTypeCuda;
  else if (IODTMatchNubWithKeys(provider, "'via-pmu'"))
    viaDeviceType = kVIADeviceTypePMU;
  else viaDeviceType = -1;  // This should not happen.
  
  // get the via's base address
  viaMemoryMap = provider->mapDeviceMemoryWithIndex(0);
  if (viaMemoryMap == 0) return false;
  viaBaseAddress = viaMemoryMap->getVirtualAddress();
  viaMemoryMap->release();

  // Calculate the bus and cpu speeds if needed.
  if (provider->getProperty("BusSpeedCorrect") == 0) {
    callPlatformFunction("GetDefaultBusSpeeds", false,
                         &numSpeeds, &speedList, 0, 0);
    PE_Determine_Clock_Speeds(viaBaseAddress, numSpeeds, speedList);
  }

  // Allocate the interruptController instance.
  interruptController = new AppleVIAInterruptController;
  if (interruptController == NULL) return false;
  
  // call the interruptController's init method.
  error = interruptController->initInterruptController(provider,
						       viaBaseAddress);
  if (error != kIOReturnSuccess) return false;
  
  handler = interruptController->getInterruptHandlerAddress();
  provider->registerInterrupt(0, interruptController, handler, 0);
  
  provider->enableInterrupt(0);
  
  // Register the interrupt controller so clients can find it.
  interruptControllerName = (OSSymbol *)OSSymbol::withCStringNoCopy(kInterruptControllerName);
  getPlatform()->registerInterruptController(interruptControllerName,
					     interruptController);
  
  nub = createNub();
  if (nub == 0) return false;
  
  nub->attach(this);
        
  //Do the mac-io-publishChildren with privCreateNub if pmuExists
  if (pmuExists)
	{
	if (provider->getProperty("preserveIODeviceTree") != 0)
		provider->callPlatformFunction ("mac-io-publishChildren", 0, (void*)this, 
                                                    (void*)privCreateNub, (void*)0, (void*)0);
        }
  nub->registerService();
  
  return true;
}

static IOService * privCreateNub( IORegistryEntry * from)
{
    IOService * nub = 0;
        
    if (!strcmp ("pmu", from->getName(gIOServicePlane))) 
            {
            //IOLog("AppleVIA privCreateNub  FOUND PMU NUB skipping ...\n");
            }
    else
            {
            nub = new AppleVIADevice;
    
            if (nub && (!nub->init(from, gIODTPlane)))
                {
                nub->free();
                nub = 0;
                }
            }
    
    return( nub);
}
	
AppleVIADevice *AppleVIA::createNub(void)
{
  int               cnt;
  bool              err;
  OSSymbol          *cName;
  OSSymbol          *name = 0;
  OSArray           *array = 0, *vecArray = 0, *deviceMemoryArray;
  OSDictionary      *dict = 0;
  AppleVIADevice    *nub = 0;
  
  do {
    // create the name symbol for the interrupt controller.
    cName = (OSSymbol *)OSSymbol::withCStringNoCopy(kInterruptControllerName);
    if (cName == 0) continue;
    
    // create the vector array.
    vecArray = OSDynamicCast(OSArray, getProperty("vectors"));
    if ((vecArray == 0) || (vecArray->getCount() != kNumVectors)) continue;
    
    // create the controller array.
    array = OSArray::withCapacity(kNumVectors);
    if (array == 0) continue;
    
    // populate the names array
    err = false;
    for (cnt = 0; cnt < kNumVectors; cnt++) {
      if (!array->setObject(cName)) {
        err = true;
        break;
      }
    }
    if (err) continue;
    
    // create the deviceMemory array.
    deviceMemoryArray = getProvider()->getDeviceMemory();
    if (deviceMemoryArray == 0) continue;
    
    // create the name for the viaDevice nub
    if (viaDeviceType == kVIADeviceTypeCuda)
      name = (OSSymbol *)OSSymbol::withCStringNoCopy("cuda");
    else if (viaDeviceType == kVIADeviceTypePMU)
      name = (OSSymbol *)OSSymbol::withCStringNoCopy("pmu");
    else name = 0;
    if (name == 0) continue;
    
    // Create the dictionary for the viaDevice nub.
    dict = OSDictionary::withCapacity(1);
    if (dict == 0) continue;
    
    // add the interrupt numbers and parents to the dictionary.
    err = !dict->setObject("name", name);
    err |= !dict->setObject(gIOInterruptSpecifiersKey, vecArray);
    err |= !dict->setObject(gIOInterruptControllersKey, array);
    if (err) continue;
    
    // Create the viaDevice nub
    nub = new AppleVIADevice;
    if (nub == 0) continue;
		
    if (pmuExists && childEntry)
        {
        if(!nub->init(childEntry, gIODTPlane) )   //nub is IOService ptr
            {
            //IOLog("AppleVIA PMU CHILD DID NOT INIT\n");
                    nub->free();
                    nub = 0;
                    continue;
            }
        else  
            nub->setPropertyTable(dict);
        }
	else if (!nub->init(dict)) 
		{
                //IOLog("AppleVIA NUB DID NOT INIT\n");
                nub->release();
                nub = 0;
                continue;
		}
		    
    // set the nub's name.
    nub->setName(name);
    
    // set the nub's deviceMemory.
    nub->setDeviceMemory(deviceMemoryArray);
    
  } while(false);
  
  if(name) name->release();
  if(cName) cName->release();
  if(array) array->release();
  if(dict) dict->release();
  
  return nub;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef  super
#define super IOService

OSDefineMetaClassAndStructors(AppleVIADevice, IOService);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef  super
#define super IOInterruptController

OSDefineMetaClassAndStructors(AppleVIAInterruptController, IOInterruptController);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn AppleVIAInterruptController::initInterruptController(IOService *provider, IOLogicalAddress interruptControllerBase)
{
  int cnt;
  
  parentNub = provider;
  
  // Allocate the memory for the vectors
  vectors = (IOInterruptVector *)IOMalloc(kNumVectors *
					  sizeof(IOInterruptVector));
  if (vectors == NULL) return kIOReturnNoMemory;
  bzero(vectors, kNumVectors * sizeof(IOInterruptVector));
  
  // Allocate locks for the
  for (cnt = 0; cnt < kNumVectors; cnt++) {
    vectors[cnt].interruptLock = IOLockAlloc();
    if (vectors[cnt].interruptLock == NULL) {
      for (cnt = 0; cnt < kNumVectors; cnt++) {
	if (vectors[cnt].interruptLock != NULL)
	  IOLockFree(vectors[cnt].interruptLock);
      }
      return kIOReturnNoResources;
    }
  }
  
  // Initialize the registers.
  IEReg = (volatile unsigned char *)(interruptControllerBase + kIEOffset);
  IFReg = (volatile unsigned char *)(interruptControllerBase + kIFOffset);
  PCReg = (volatile unsigned char *)(interruptControllerBase + kPCOffset);
  
  // Disable all interrupts.
  *PCReg = 0x00; eieio();
  *IEReg = 0x00; eieio();
  *IFReg = 0x7F; eieio();
  
  return kIOReturnSuccess;
}

IOInterruptAction AppleVIAInterruptController::getInterruptHandlerAddress(void)
{

  return (OSMemberFunctionCast(IOInterruptAction, this, &AppleVIAInterruptController::handleInterrupt));   //4092033
  //return (IOInterruptAction)&AppleVIAInterruptController::handleInterrupt;
}

IOReturn AppleVIAInterruptController::handleInterrupt(void */*refCon*/,
						      IOService */*nub*/,
						      int /*source*/)
{
  int               done;
  long              events, vectorNumber;
  IOInterruptVector *vector;
  
  do {
    done = 1;
    
    // Do all the sources for events.
    events = *IFReg; eieio();
    events |= pendingEvents;
    pendingEvents = 0;
    events &= *IEReg & 0x7F; eieio();
    
    if (events) {
      done = 0;
      *IFReg = events; eieio();
    }
    
    while (events) {
      vectorNumber = 31 - cntlzw(events);
      events ^= (1 << vectorNumber);
      vector = &vectors[vectorNumber];
      
      vector->interruptActive = 1;
      sync();
      isync();
      if (!vector->interruptDisabledSoft) {
	isync();
	
	// Call the handler if it exists.
	if (vector->interruptRegistered) {
	  vector->handler(vector->target, vector->refCon,
			  vector->nub, vector->source);
	}
      } else {
	// Hard disable the source.
	vector->interruptDisabledHard = 1;
	disableVectorHard(vectorNumber, vector);
      }
      
      vector->interruptActive = 0;
    }
    
  } while (!done);
  
  return kIOReturnSuccess;
}

void AppleVIAInterruptController::disableVectorHard(long vectorNumber, IOInterruptVector */*vector*/)
{
  *IEReg = (1 << vectorNumber);
  eieio();
}

void AppleVIAInterruptController::enableVector(long vectorNumber,
					       IOInterruptVector *vector)
{
  *IEReg = 0x80 | (1 << vectorNumber);
  eieio();
}

void AppleVIAInterruptController::causeVector(long vectorNumber,
					      IOInterruptVector */*vector*/)
{
  pendingEvents |= 1 << vectorNumber;
  parentNub->causeInterrupt(0);
}
