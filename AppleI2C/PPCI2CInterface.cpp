/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * Implementation of the interface for the keylargo I2C interface
 *
 * HISTORY
 *
 */

#include "PPCI2CInterface.h"
#include <IOKit/IODeviceTreeSupport.h>

extern "C" vm_offset_t ml_io_map(vm_offset_t phys_addr, vm_size_t size);

#define super IOService
OSDefineMetaClassAndStructors( PPCI2CInterface, IOService )


// Uncomment the following define if you wish ro see more logging
// on the i2c interface. Note: use this only in polling mode since
// IOLog panics when used in primary interrupt context.
// #define DEBUGMODE 
// #define DEBUGPMU  // for just PMU i2c related debug logs

// Delay after accessing to an I2C register
#define I2C_REGISTERDELAY 10

// Private Methods:
// ===============

// FIXME: change the if 0 into if 1 in the following line for final
// builds so all the simple private functions can be inlined.
#if 1
#define INLINE inline
#else
#define INLINE
#endif

// --------------------------------------------------------------------------
// Method: dumpI2CRegisters
//
// Purpose:
//        Given the base of the i2c registers inits all the registers
INLINE void
PPCI2CInterface::dumpI2CRegisters()
{
#ifdef DEBUGMODE
    IOLog("mode    0x%08lx -> 0x%02x\n", (UInt32)mode,*mode);
    IOLog("control 0x%08lx -> 0x%02x\n", (UInt32)control,*control);
    IOLog("status  0x%08lx -> 0x%02x\n", (UInt32)status,*status);
    IOLog("ISR     0x%08lx -> 0x%02x\n", (UInt32)ISR,*ISR);
    IOLog("IER     0x%08lx -> 0x%02x\n", (UInt32)IER,*IER);
    IOLog("address 0x%08lx -> 0x%02x\n", (UInt32)address,*address);
    IOLog("subAddr 0x%08lx -> 0x%02x\n", (UInt32)subAddr,*subAddr);
    //IOLog("data    0x%08lx -> 0x%02x\n", (UInt32)data,*data);
#endif //DEBUGMODE
}

// --------------------------------------------------------------------------
// Method: SetI2CBase
//
// Purpose:
//        Given the base of the i2c registers inits all the registers
INLINE void
PPCI2CInterface::SetI2CBase(UInt8 *baseAddress, UInt8 steps)
{
    I2CRegister base;

    base = (I2CRegister)baseAddress;

    if (base != NULL) {
        mode = base;				// Configure the transmission mode of the i2c cell and the databit rate.
        control = mode + steps;			 // Holds the 4 bits used to start the operations on the i2c interface.
        status = control + steps;		// Status bits for the i2 cell and the i2c interface.
        ISR = status + steps;		 	// Holds the status bits for the interrupt conditions.
        IER = ISR + steps;			// Eneables the bits that allow the four interrupt status conditions.
        address = IER + steps;			  // Holds the 7 bits address and the R/W bit.
        subAddr = address + steps; // the 8bit subaddress..
        data = subAddr + steps; 			// from where we read and write data

        // Clears interrupt and status register:
        *status = 0x00;
        *ISR = 0x00;

        // And remembers the current mode:
        getMode();

#ifdef DEBUGMODE
        IOLog("PPCI2CInterface::SetI2CBase(0x%08lx,0x%02x)\n",(UInt32)baseAddress, steps);
        dumpI2CRegisters();
#endif // DEBUGMODE
    }
}

// --------------------------------------------------------------------------
// Method: shiftedMask
//
// Purpose:
//        Returns the mask to use with the register:
INLINE UInt8
PPCI2CInterface::shiftedMask(UInt8 mask, UInt8 shift)
{
    return (mask << shift);
}

// --------------------------------------------------------------------------
// Method: shiftedCompMask
//
// Purpose:
//        Returns the complement of the mask
INLINE UInt8
PPCI2CInterface::shiftedCompMask(UInt8 mask, UInt8 shift)
{
    return ~shiftedMask(mask, shift);
}

// --------------------------------------------------------------------------
// Method: writeRegisterField
//
// Purpose:
//        writes a field of a regiter with the given data. The arguments are
//        the register, the mask for the field, shift to get to the right bits
//        and the new data.
INLINE void
PPCI2CInterface::writeRegisterField(I2CRegister reg, UInt8 mask, UInt8 shift, UInt8 newData)
{
    UInt8 registerValue = *reg;
    UInt8 data = (newData & mask);
    UInt8 nMask = shiftedCompMask(mask, shift);

    *reg = (registerValue & nMask) | (data << shift);
    eieio();
    IODelay(I2C_REGISTERDELAY);
    
#ifdef DEBUGMODE
    IOLog("PPCI2CInterface::writeRegisterField(0x%08lx, 0x%02x, 0x%02x, 0x%02x)\n",(UInt32)reg, mask, shift, newData);
    IOLog("PPCI2CInterface::writeRegisterField() registerValue=0x%02x\n", registerValue);
    IOLog("PPCI2CInterface::writeRegisterField() data=0x%02x\n", data);
    IOLog("PPCI2CInterface::writeRegisterField() nMask=0x%02x\n", nMask);
    IOLog("PPCI2CInterface::writeRegisterField() *reg=0x%02x\n", *reg);
#endif // DEBUGMODE
}

// --------------------------------------------------------------------------
// Method: readRegisterField
//
// Purpose:
//        writes a field of a regiter with the given data. The arguments are
//        the register, the mask for the field and shift to get to the right
//        bits.
INLINE UInt8
PPCI2CInterface::readRegisterField(I2CRegister reg, UInt8 mask, UInt8 shift)
{
    UInt8 registerValue = *reg;
    UInt8 returnValue = ((registerValue) >> shift) & mask;

#ifdef DEBUGMODE
    IOLog("PPCI2CInterface::readRegisterField(0x%08lx, 0x%02x, 0x%02x)\n",(UInt32)reg, mask, shift);
    IOLog("PPCI2CInterface::readRegisterField() *reg=0x%02x\n", *reg);
    IOLog("PPCI2CInterface::readRegisterField() returnValue=0x%02x\n", returnValue);
#endif // DEBUGMODE

    return returnValue;
}

// Intermediate methods to access to each field of all the registers:
// ------------------------------------------------------------------

// Mode register

// Mode register
INLINE void
PPCI2CInterface::setPort(UInt8 newPort)
{
    // remember the last mode:
    portSelect = newPort;
    if (!i2cPMU)     
       writeRegisterField(mode, (UInt8)kPortMask, (UInt8)I2CPortShift, (UInt8)newPort);
}

INLINE UInt8
PPCI2CInterface::getPort()
{
    if (!i2cPMU)
        portSelect = (I2CMode)readRegisterField(mode, (UInt8)kPortMask, (UInt8)I2CPortShift);
    
    return portSelect;
}

INLINE void
PPCI2CInterface::setMode(I2CMode newMode)
{
    // remember the last mode:
    lastMode = newMode;
    if (!i2cPMU)
        writeRegisterField(mode, (UInt8)kModeMask, (UInt8)I2CModeShift, (UInt8)newMode);
}

INLINE PPCI2CInterface::I2CMode
PPCI2CInterface::getMode()
{
    if (!i2cPMU)
        lastMode = (I2CMode)readRegisterField(mode, (UInt8)kModeMask, (UInt8)I2CModeShift);
    return lastMode;
}

INLINE void
PPCI2CInterface::setSpeed(I2CSpeed newSpeed)
{
    if (!i2cPMU)
        writeRegisterField(mode, (UInt8)kSpeedMask, (UInt8)I2CSpeedShift, (UInt8)newSpeed);
   else 
        lastSpeed = newSpeed;
}

INLINE PPCI2CInterface::I2CSpeed
PPCI2CInterface::getSpeed()
{
    if (!i2cPMU)
        return( (I2CSpeed)readRegisterField(mode, (UInt8)kSpeedMask, (UInt8)I2CSpeedShift) );
    else
        return lastSpeed;
}

// Control register
INLINE void
PPCI2CInterface::setControl(I2CControl newControlValue)
{
    *control = (UInt8)newControlValue;
    eieio();
    IODelay(I2C_REGISTERDELAY);
}

INLINE PPCI2CInterface::I2CControl
PPCI2CInterface::getControl()
{
    I2CControl controlValue;
    controlValue = (I2CControl)(*control);

    return controlValue;
}

// Status register
INLINE void
PPCI2CInterface::setStatus(I2CStatus newStatusValue)
{
    *status = (UInt8)newStatusValue;
    eieio();
    IODelay(I2C_REGISTERDELAY);
}

INLINE PPCI2CInterface::I2CStatus
PPCI2CInterface::getStatus()
{
    I2CStatus statusValue;
    statusValue = (I2CStatus)(*status);
    return statusValue;
}

// Interrupt status
INLINE void
PPCI2CInterface::setInterruptStatus(I2CInterruptStatus newStatusValue)
{
    *ISR = (UInt8)newStatusValue;
    eieio();
    IODelay(I2C_REGISTERDELAY);
}

INLINE PPCI2CInterface::I2CInterruptStatus
PPCI2CInterface::getInterruptStatus()
{
    I2CInterruptStatus intStatus;
    intStatus = (I2CInterruptStatus)(*ISR);
    return intStatus;
}

// Interrupt enable
INLINE void
PPCI2CInterface::setInterruptEnable(I2CInterruptEnable newInterruptEnable)
{
    *IER = (UInt8)newInterruptEnable;
    eieio();
    IODelay(I2C_REGISTERDELAY);
}

INLINE PPCI2CInterface::I2CInterruptEnable
PPCI2CInterface::setInterruptEnable()
{
    I2CInterruptEnable interEnable;
    interEnable = (I2CInterruptEnable)(*IER);
    return interEnable;
}

// Address Register:
INLINE void
PPCI2CInterface::setAddress(UInt8 newAddress)
{
    writeRegisterField(address, (UInt8)kADDRMask, (UInt8)I2CAddressShift, (UInt8)newAddress);
    eieio();
}

INLINE void
PPCI2CInterface::setAddressRegister(UInt8 newAddress, I2CRWMode readMode)
{
    newAddress &= kADDRMask;
    
    if (i2cPMU) {
        currentAddress = (newAddress << I2CAddressShift) | readMode;
        }
    else {
        *address = (newAddress << I2CAddressShift) | readMode;
        IODelay(I2C_REGISTERDELAY);

#ifdef DEBUGMODE
    IOLog("setAddressRegister( 0x%02x, %d) = 0x%02x\n", newAddress, (UInt8)readMode, (UInt8)*address);
#endif // DEBUGMODE

        eieio();
        }
}

INLINE UInt8
PPCI2CInterface::getAddress()
{
    return readRegisterField(address, (UInt8)kADDRMask, (UInt8)I2CAddressShift);
}

INLINE void
PPCI2CInterface::setReadWrite(I2CRWMode readMode)
{
    writeRegisterField(address, (UInt8)kRWMask, (UInt8)I2CRWShift, (UInt8)readMode);
    eieio();
}

INLINE PPCI2CInterface::I2CRWMode
PPCI2CInterface::getReadWrite()
{
    return (I2CRWMode)readRegisterField(address, (UInt8)kRWMask, (UInt8)I2CRWShift);
}

// SubAddress register
INLINE void
PPCI2CInterface::setSubAddress(UInt8 newSubAddress)
{
    *subAddr = newSubAddress;
    eieio();
}

INLINE UInt8
PPCI2CInterface::getSubAddress()
{
    return (*subAddr);
}

// Data register
INLINE void
PPCI2CInterface::setData(UInt8 newByte)
{
    *data = newByte;
    eieio();
    IODelay(I2C_REGISTERDELAY);
}

INLINE UInt8
PPCI2CInterface::getData()
{
    return (*data);
}

// --------------------------------------------------------------------------
// Method: setAddressAndDirection
//
// Purpose:
//        sets the control register with the correct address and data direction.
INLINE bool
PPCI2CInterface::setAddressAndDirection()
{
    // Since we still have to begin the transfer:
    transferWasSuccesful = true;

    // sets the address register:
    if (isReading) {
        setAddressRegister(currentAddress, kReadADDR);        
    }
    else {
        setAddressRegister(currentAddress, kWriteADDR);       
    }

    if (i2cPMU)
        return(true);  //setSubAddress, setControl, etc. not needed for PMU

    // sets the subAddress:
    if ((lastMode == kStandardSubMode) || (lastMode == kCombinedMode))
        setSubAddress(currentSubaddress);
        
    // Set the state BEFORE to set the control
 	setInterruptStatus(kISRMask);  // clear any pending interrupts 
    currentState = ki2cStateWaitingForIADDR;
    setControl((I2CControl)(getControl() | kXAddrCNTRL));

#ifdef DEBUGMODE
    IOLog("PPCI2CInterface::setAddressAndDirection()\n");
#endif // DEBUGMODE

    return(true);
}

// --------------------------------------------------------------------------
// Method: i2cStandardSubModeInterrupts
//
// Purpose:
//        handles the interrupts and the state machine for the Standard + SubAddress mode:
bool
PPCI2CInterface::i2cStandardSubModeInterrupts(UInt8 interruptStatus)
{
    bool success = true;

    switch (currentState) {
        case ki2cStateWaitingForIADDR:
#ifdef DEBUGMODE            
            IOLog("ki2cStateWaitingForIADDR: ");
#endif // DEBUGMODE            

            // verify that correct interrupt bit set
            if ((interruptStatus & kIAddrISR) == 0) {
#ifdef DEBUGMODE            
                IOLog("PPCI2CInterface::i2cStandardSubModeInterrupts (ki2cStateWaitingForIADDR): bad state ");
#endif
                success = false;
            }

            if (success) {
                if (getStatus() & kLastAakSTATUS) {
                        if (isReading) {
                            // if multiple bytes are to be read, set AAK bit
                            if (nBytes > 1)
                                setControl((I2CControl)(getControl() | kAakCNTRL));
                        }
                        else {
                            // write operation
                            setData(*dataBuffer++);
                            nBytes--;
                        }

                        currentState = ki2cStateWaitingForIDATA;
                    }
                    else {
                        // no slave responded, so wait for STOP (control bit does not need to be set,
                        // since NAAK sets the control bit automatically)
#ifdef DEBUGMODE            
                        IOLog("ki2cStateWaitingForIADDR...no slave...");
#endif
                        currentState = ki2cStateWaitingForISTOP;
                        success = false;
                    }
                }
                else {
                    // bad state, so abort command
                    abortTransfer();
                }

                // clear IADDR bit and go
                setInterruptStatus(kIAddrISR);
            break;

        case ki2cStateWaitingForIDATA:
#ifdef DEBUGMODE            
            IOLog("ki2cStateWaitingForIDATA: ");
#endif
            // verify that correct interrupt bit set
            if ((interruptStatus & kIDataISR) == 0) {
#ifdef DEBUGMODE            
                IOLog("PPCI2CInterface::i2cStandardSubModeInterrupts (ki2cStateWaitingForIDATA): bad state ");
#endif
                success = false;
            }

                if (success) {
                    if (isReading) {
                        *dataBuffer++ = getData();
                        nBytes--;

                        if (nBytes == 0)			// read operation completed
                            currentState = ki2cStateWaitingForISTOP;
                        else if (nBytes > 1)
                            setControl((I2CControl)(getControl() | kAakCNTRL));
                        else //if (nBytes == 1)			// if next byte is last byte, clear AAK bit, and state remains unchanged
                            setControl(kClrCNTRL);		// clears ALL control bits (bits other than AAK bit are "don't cares")
                     }
                    else {
                        // write operation
                        if (getStatus() & kLastAakSTATUS) {
                            if (nBytes == 0) {
                                // write operation completed
                                currentState = ki2cStateWaitingForISTOP;
                                setControl((I2CControl)(getControl() | kStopCNTRL));
                            }
                            else {
                                // send an other byte
                                setData(*dataBuffer++);
                                nBytes--;
                            }
                        }
                       else {
                           // no slave responded, so wait for STOP (control bit does not need to be set,
                           // since NAAK sets the control bit automatically)
#ifdef DEBUGMODE            
                           IOLog("ki2cStateWaitingForIDATA...no slave...");
#endif
                           currentState = ki2cStateWaitingForISTOP;
                           success = false;
                        }
                    }
                }
                else {
                    // bad state, so abort command
                    abortTransfer();
                }

                // clear IDATA bit and go
                setInterruptStatus(kIDataISR);
            break;

        case ki2cStateWaitingForISTOP:
#ifdef DEBUGMODE
            IOLog("ki2cStateWaitingForISTOP: ");
#endif
            // verify that correct interrupt bit set
            if ((interruptStatus & kIStopISR) == 0) {
#ifdef DEBUGMODE
                IOLog("PPCI2CInterface::i2cStandardSubModeInterrupts (ki2cStateWaitingForISTOP): bad state ");
#endif
                success = false;
            }

            setInterruptStatus(kIStopISR);
            currentState = ki2cStateIdle;
            semaphore_signal(mySync);
           break;

        case ki2cStateWaitingForISTART:
#ifdef DEBUGMODE
            IOLog("ki2cStateWaitingForISTART: is not supposed to happen in this state machine ");
#endif
            break;

        case ki2cStateIdle:
#ifdef DEBUGMODE
            IOLog("ki2cStateIdle: ");
#endif
            break;

        default:
             break;
    }
#ifdef DEBUGMODE
    IOLog(" %s\n", (success ? "true" : "false"));
#endif

    // tell the system that this interrupt has been serviced
    return success;
}

// --------------------------------------------------------------------------
// Method: abortTransfer
//
// Purpose:
//        ends the data transfer.
INLINE bool
PPCI2CInterface::abortTransfer()
{
    currentState = ki2cStateWaitingForISTOP;
    setControl((I2CControl)(getControl() | kStopCNTRL));
    return false;
}

// --------------------------------------------------------------------------
// Method: waitForCompletion
//
// Purpose:
//        waits until the last command was executed correctly and when it
//        happens it calls the interrupt handler. This is useful for when
//        we use this object without an interrupt handler.
bool
PPCI2CInterface::waitForCompletion()
{
    // increase timeout for AppleTexasAudio (part can take seconds to come back)
    // UInt16 loop = 50 + nBytes * 1500;   //Old timeout
	UInt16 loop = 15000;   				   //New timeout fixed at 15 seconds
    
    UInt8 intStat = getInterruptStatus();
 
    // First call to the interrupt handler in case the last command
    // is already completed
    
    //IOLog("1] Interrupt Status = 0x%02x\n", intStat);
    if (intStat & kISRMask)
        handleI2CInterrupt();

    // makes 10 loops for each expected interrupt with 1 millisecond delay for each loop:
    while((loop--) && (currentState != ki2cStateIdle)) {
		// If at interrupt context, use spin delay instead of sleep
		if (ml_at_interrupt_context())
			IODelay (1000);
		else
			IOSleep(1);

        // If an interrupt occured handle it:
        intStat = getInterruptStatus();

        if (intStat & kISRMask)
            handleI2CInterrupt();
    }
    
    if (currentState != ki2cStateIdle) {
#ifdef DEBUGMODE
        IOLog("PPCI2CInterface::waitForCompletion error loop is incomplete\n");
#endif
        return false;
    }
    else {
#ifdef DEBUGMODE
        IOLog("PPCI2CInterface::waitForCompletion loop is complete\n");
#endif
        return true;        
    }
}

// Protected setup methods:
// These instead set the speed:
bool
PPCI2CInterface::setKhzSpeed(UInt speed)
{

    switch (speed)
    {
        case 100:
            setSpeed(k100KhzMode);
            break;

        case 50:
            setSpeed(k50KhzMode);
            break;

        case 25:
            setSpeed(k25KhzMode);
            break;

        default:
#ifdef DEBUGMODE
            IOLog("PPCI2CInterface::setKhzSpeed Can not set bus speed %d is not allowed\n", speed);
#endif
            return false;
            break;
    }

    return true;
}

// Initialize the address of the registers and the registers themselfs:
bool
PPCI2CInterface::initI2CBus(UInt8 *baseAddress, UInt8 steps)
{
    pollingMode = true;
    currentState = ki2cStateIdle;
    SetI2CBase(baseAddress, steps);

    return true;
}

/* static */ void
PPCI2CInterface::handleHardwareInterrupt(OSObject *target, void *refCon, IOService *nub, int source)
{
    PPCI2CInterface *ppcI2C = OSDynamicCast(PPCI2CInterface, target);

    if (ppcI2C != NULL) {
        ppcI2C->myProvider->disableInterrupt(0);
        ppcI2C->handleI2CInterrupt();
        ppcI2C->myProvider->enableInterrupt(0);
    }
}

// The interrupt handler:
// dispatches to the correct interrupt handler according to the current mode set:
bool
PPCI2CInterface::handleI2CInterrupt()
{
    bool success = false;
    
    switch(lastMode)
    {
        case kDumbMode:
#ifdef DEBUGMODE
            IOLog("PPCI2CInterface::handleI2CInterrupt kDumbMode is an unsupported mode (for now)\n");
#endif
            break;

        case kStandardMode:
        case kStandardSubMode:
        case kCombinedMode:
            success = i2cStandardSubModeInterrupts(getInterruptStatus());
            break;
    }

    // Update the transfer status:
    transferWasSuccesful = (transferWasSuccesful && success);

#ifdef DEBUGMODE
    IOLog("PPCI2CInterface::handleI2CInterrupt transferWasSuccesful = %s success= %s.\n",
              ((transferWasSuccesful) ? "true" : "false"),
              (success ? "true" : "false"));
#endif // DEBUGMODE

    // By default we fail ...
    return success;
}

// Public Methods:
// ===============

bool
PPCI2CInterface::start(IOService *provider)
{
    OSData *t;
    UInt32 baseAddress;
    UInt32 addressSteps;
    UInt32 rate;
    OSIterator *iterator;
    IORegistryEntry *registryEntry;
    IOService *nub;
    
    // makes sure this is uninitialized.
    i2cRegisterMap = NULL;

    if (!super::start(provider)) return false;	//this was just added..
    
    //see if we are attached to pmu-i2c node, and if so return i2cPMU = TRUE.
    retrieveProperty(provider);

    // Creates the mutex lock to provide atomic access to the services of the I2C bus:
    mutexLock = IORecursiveLockAlloc();
    if (mutexLock == NULL) {
#ifdef DEBUGMODE
        IOLog("PPCI2CInterface::start  can not create the IORecursiveLock so we bail off\n");
#endif
        return false;
    }
    IORecursiveLockLock(mutexLock);  //Keep clients from accessing until start is complete

    // publish children for Uni-N's i2c bus only, KeyLargo will handle the others (PMU and Mac-IO).
    if (i2cUniN == TRUE)
      {    
        if( (iterator = provider->getChildIterator(gIODTPlane)) )
        {
            while( (registryEntry = (IORegistryEntry *)iterator->getNextObject()) != 0)
            {
                if ((nub = new PPCI2CInterface) !=0)
				{
					if(!nub->init(registryEntry, gIODTPlane) )   //nub is IOService ptr
						{
							nub->free();
							nub = 0;
						}
					else
						{
							nub->attach(this);
							nub->registerService();                                
						}
				}
            }    
            iterator->release();
        }
    }
    else if (provider->getProperty("preserveIODeviceTree") != 0)
		provider->callPlatformFunction("mac-io-publishChildren",0,(void*)this,
                                                        (void*)0,(void*)0,(void*)0);


#ifdef DEBUGMODE
    IOLog("PPCI2CInterface::start(%s)\n", provider->getName());
#endif
    
    // sets up the interface:
#if 1

    //if PMU-I2C, skip the following... 
    if (!i2cPMU) {

    t = OSDynamicCast(OSData, provider->getProperty("AAPL,address"));
    if (t != NULL) {
        baseAddress = *((UInt32*)t->getBytesNoCopy());
        baseAddress = ml_io_map(baseAddress, 0x1000);
        //baseAddress = (UInt32)pmap_extract(kernel_pmap,(vm_address_t)baseAddress);
    }
    else {
#ifdef DEBUGMODE
        IOLog( "PPCI2CInterface::start missing property AAPL,address in i2c registry\n");
#endif
        return false;
    }
#else
    i2cRegisterMap = provider->mapDeviceMemoryWithIndex(1);
    if ( i2cRegisterMap == NULL ) {
#ifdef DEBUGMODE
        IOLog( "PPCI2CInterface::start missing i2cRegisterMap\n");
#endif
        return false;
    }
    else
        baseAddress = (UInt32)pmap_extract(kernel_pmap,(vm_address_t)i2cRegisterMap->getVirtualAddress());
#endif

    t = OSDynamicCast(OSData, provider->getProperty("AAPL,address-step"));
    if (t != NULL)
        addressSteps = *((UInt32*)t->getBytesNoCopy());
    else {
#ifdef DEBUGMODE
        IOLog( "PPCI2CInterface::start missing property AAPL,address-step in i2c registry\n");
#endif
        return false;
    }

    t = OSDynamicCast(OSData, provider->getProperty("AAPL,i2c-rate"));
    if (t != NULL)
        rate = *((UInt32*)t->getBytesNoCopy());
    else {
#ifdef DEBUGMODE
        IOLog( "PPCI2CInterface::start missing property AAPL,i2c-rate in i2c registry\n");
#endif
        return false;
    }

    if (!initI2CBus((UInt8*)baseAddress, (UInt8)addressSteps))   
    return false;

    } // This bracket ends the !i2cPMU case.
        
    if (i2cPMU) {
        setKhzSpeed(100);     	 // PMU case specifics
    	clientMutexLock = NULL;  // Creates the mutex lock to protect the PMU clients list
        clientMutexLock = IOLockAlloc();
        }
        
    else if (!setKhzSpeed((UInt)rate))
        return false;

    // remembers the provider:
    myProvider = provider;

    // by default we work in polling mode:
    pollingMode = true;

    // Makes sure that other drivers can use it:
    publishResource(getResourceName(), this );
    IORecursiveLockUnlock(mutexLock);

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Retrive any properties from the provider and stores the data we need locally
bool PPCI2CInterface::retrieveProperty(IOService *provider)
{
    i2cPMU = false;
    i2cUniN = false;
    i2cmacio = false;
    OSData *propertyPtr = 0;
    IOService *nub = provider;
    registeredForPmuI2C = false;  // checked when someone tries to register for a PMU interrupt
    
    nub = nub->getProvider();
 
    // Are we attaching to the PMU I2C bus?
    if ( (propertyPtr = OSDynamicCast(OSData, provider->getProperty("name"))) )
    {
        if ( strncmp("pmu-i2c", (const char *)propertyPtr->getBytesNoCopy(), 7) == 0 )
        {
    #ifdef DEBUGPMU
            IOLog("                                      APPLE I2C:    pmu-i2c COMPATIBLE !\n");
    #endif // DEBUGPMU
            i2cPMU = true;

            return true;
        }
    }

    // Are we attaching to the Uni-N I2C bus?
    if ( (propertyPtr = OSDynamicCast(OSData, provider->getProperty("AAPL,driver-name"))) )
    {
        if ( strncmp(".i2c-uni-n", (const char *)propertyPtr->getBytesNoCopy(), 10) == 0 )
        {
    #ifdef DEBUGPMU
            IOLog("                                      APPLE I2C:    UniN-i2c COMPATIBLE !\n");
    #endif // DEBUGPMU
            i2cUniN = true;

            return true;
        }        
    }

    // Must be attaching to the Mac-IO I2C bus?    
    i2cmacio = true;
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////
void
PPCI2CInterface::free()
{
    if (mutexLock != NULL)
        IORecursiveLockFree(mutexLock);

    if (i2cRegisterMap != NULL)
        i2cRegisterMap->release();
        
    if (i2cPMU) {
        if (clientMutexLock != NULL) 
            { // Release the mutex lock used to protect PMU clients
            IOLockFree (clientMutexLock);
            clientMutexLock = NULL;
            }
        clearI2CclientList();
    }
}

// The default behavior of this class is to work in "polling mode"
// where the interrupt handler is called by a polling routine.
// However it is always possible to attach the interrupt handler
// to an hardware interrupt and handle the transfer in the proper
// way. (the polling mode was created for the sole purpose to
// debug the state machine).  
bool
PPCI2CInterface::setPollingMode(bool newPollingMode)
{
#ifdef DEBUGMODE
    // polling is the only supported mode in debug
    newPollingMode = true;
#endif // DEBUGMODE
    // If I am not the owner of the lock returns without doing anything.
	// Interrupt context calls do not (should not) call setPollingMode currently...
    if (!IORecursiveLockHaveLock(mutexLock)) {
#ifdef DEBUGMODE
        IOLog("PPCI2CInterface::setPollingMode(%s) I am not the owner of the lock returns without doing anything.\n",
              (newPollingMode ? "true" : "false"));
#endif // DEBUGMODE
        return false;   
    }

    // if the previous mode was non polling the interrupts are on and
    // enabled. So we turn them off:
    if (!pollingMode) {
        myProvider->disableInterrupt(0);
        myProvider->unregisterInterrupt(0);
    }

    // sets the new mode:
    pollingMode = newPollingMode;

    // if the new mode is polling off I assume the drive should behave as interrupt driven:
    if (!newPollingMode) {
        if (myProvider->registerInterrupt(0, this, handleHardwareInterrupt, 0 ) != kIOReturnSuccess) {
			IOLog("AppleI2C: setting hardware INTERRUPT failed; using POLLED mode instead.\n");
            // since we can not register the interrupts:
            pollingMode = true;
            return false;
        }
    }

    return true;
}

// These are to setup the mode for the I2C bus:
void
PPCI2CInterface::setDumbMode()
{
    // If I am not the owner of the lock returns without doing anything.
    if (!IORecursiveLockHaveLock(mutexLock)) {
#ifdef DEBUGMODE
        IOLog("PPCI2CInterface::setDumbMode I am not the owner of the lock returns without doing anything.\n");
#endif // DEBUGMODE
        return;
    }

    if (i2cPMU)
        setMode(kSimpleI2CStream);
    else
        setMode(kDumbMode);
}

void
PPCI2CInterface::setStandardMode()
{
    // If I am not the owner of the lock returns without doing anything.
    if (!IORecursiveLockHaveLock(mutexLock)) {
#ifdef DEBUGMODE
        IOLog("PPCI2CInterface::setStandardMode I am not the owner of the lock returns without doing anything.\n");
#endif // DEBUGMODE
        return;
    }

    if (i2cPMU)
        setMode(kSimpleI2CStream);
    else
        setMode(kStandardMode);
}
void
PPCI2CInterface::setStandardSubMode()
{
    // If I am not the owner of the lock returns without doing anything.
    if (!IORecursiveLockHaveLock(mutexLock)) {
#ifdef DEBUGMODE
        IOLog("PPCI2CInterface::setStandardSubMode I am not the owner of the lock returns without doing anything.\n");
#endif // DEBUGMODE
        return;
    }

    if (i2cPMU)
        setMode(kSubaddressI2CStream);
    else
        setMode(kStandardSubMode);
}

void
PPCI2CInterface::setCombinedMode()
{
    // If I am not the owner of the lock returns without doing anything.
    if (!IORecursiveLockHaveLock(mutexLock)) {
#ifdef DEBUGMODE
        IOLog("PPCI2CInterface::setCombinedMode I am not the owner of the lock returns without doing anything.\n");
#endif // DEBUGMODE
        return;
    }

    if (i2cPMU)
        setMode(kCombinedI2CStream);
    else
        setMode(kCombinedMode);
}

// Test to read the values set by the funtions above:
bool
PPCI2CInterface::isInDumbMode()
{
    // If I am not the owner of the lock returns without doing anything.
    if (!IORecursiveLockHaveLock(mutexLock)) {
#ifdef DEBUGMODE
        IOLog("PPCI2CInterface::isInDumbMode I am not the owner of the lock returns without doing anything.\n");
#endif // DEBUGMODE
        return false;
    }
    
    return (getMode() == kDumbMode);
}

bool
PPCI2CInterface::isInStandardMode()
{
    // If I am not the owner of the lock returns without doing anything.
    if (!IORecursiveLockHaveLock(mutexLock)) {
#ifdef DEBUGMODE
        IOLog("PPCI2CInterface::isInStandardMode I am not the owner of the lock returns without doing anything.\n");
#endif // DEBUGMODE
        return false;
    }

    
    return (getMode() == kStandardMode);
}

bool
PPCI2CInterface::isInStandardSubMode()
{
    // If I am not the owner of the lock returns without doing anything.
    if (!IORecursiveLockHaveLock(mutexLock)) {
#ifdef DEBUGMODE
        IOLog("PPCI2CInterface::isInStandardSubMode I am not the owner of the lock returns without doing anything.\n");
#endif // DEBUGMODE
        return false;
    }
    
    return (getMode() == kStandardSubMode);
}

bool
PPCI2CInterface::isInCombinedMode()
{
    // If I am not the owner of the lock returns without doing anything.
    if (!IORecursiveLockHaveLock(mutexLock)) {
#ifdef DEBUGMODE
        IOLog("PPCI2CInterface::isInCombinedMode I am not the owner of the lock returns without doing anything.\n");
#endif // DEBUGMODE
        return false;
    }

    return (getMode() == kCombinedMode);
}

// These instead returns the speed:
UInt
PPCI2CInterface::getKhzSpeed()
{
    // If I am not the owner of the lock returns without doing anything.
    if (!IORecursiveLockHaveLock(mutexLock)) {
#ifdef DEBUGMODE
            IOLog("PPCI2CInterface::getKhzSpeed I am not the owner of the lock returns without doing anything.\n");
#endif // DEBUGMODE
        return 0;
    }
    switch((int)getSpeed())
    {
        case k100KhzMode:
            return (100);
            break;

        case k50KhzMode:
            return (50);
            break;

        case k25KhzMode:
            return (25);
            break;
    }

    // Returns 0 since the speed was not set to a real value
    return (0);
}

// Starts the use of the interface:
bool
PPCI2CInterface::openI2CBus(UInt8 port)
{
	// Interrupt context calls do not (should not) call openI2CBus ...
    // Take the lock, so only this thread will be allowed to
    // touch the i2C bus (until we close it).

#ifdef DEBUGMODE
    //if (!IORecursiveLockTryLock(mutexLock)) {
    //    IOLog("PPCI2CInterface::openI2CBus PPCI2CInterface->mutexLock is already locked and I can't take it.\n");
    //    return false;
    //}

    IOLog("PPCI2CInterface::openI2CBus attempting to access to PPCI2CInterface->mutexLock.\n");
    IORecursiveLockLock(mutexLock);
    IOLog("PPCI2CInterface::openI2CBus PPCI2CInterface->mutexLock locked.\n");
#else // !DEBUGMODE
    IORecursiveLockLock(mutexLock);
#endif // DEBUGMODE

    if (i2cPMU)  {
        lastMode = kSimpleI2CStream;	//Default case, unless changed after openI2CBus with set...Mode command
#ifdef DEBUGPMU
        IOLog(" APPLE I2C-PMU  Try to open I2CBus..................port = 0x%2x, mode = 0x%2x\n", port, lastMode);
#endif // DEBUGPMU
        }

    setPort(port);

    if (!i2cPMU)  {
        // by default we go on polling, if the client wishes to change
        // the driver behavior it should call setPollingMode(false).
        setPollingMode(true);
		setInterruptEnable((I2CInterruptEnable)(kEDataIER | kEAddrIER | kEStopIER | kEStartIER));
		// don't ever setInterruptEnable for i2cPMU case: hwclock panics!
    }
    else  pollingMode = true;


    return true;
}

// Writes a block of data at a given address:
bool
PPCI2CInterface::writeI2CBus(UInt8 address, UInt8 subAddress, UInt8 *newData, UInt16 len)
{    
	mach_timespec_t  timeout;
	// Don't check lock if I'm being called at interrupt context - if you're calling me at interrupt context
	// you better know what you're doing
	if (!ml_at_interrupt_context())
		// If I am not the owner of the lock returns without doing anything.
		if (!IORecursiveLockHaveLock(mutexLock)) {
	#if DEBUGMODE
			IOLog("PPCI2CInterface::writeI2CBus I am not the owner of the lock returns without doing anything.\n");
	#endif // DEBUGMODE
			return false;
		}

    bool success = true;
    
    // pointer to the data to be transfered
    dataBuffer = newData;

    // and the number of bytes still to transfer
    nBytes = len;

    // the current transfer address:
    currentAddress = address;

    // the current transfer subAddress:
    currentSubaddress = subAddress;

    // the data direction:
    isReading = false;
    
    if (pollingMode) {
        if ((success = setAddressAndDirection()) == true ) {
            if (i2cPMU) 
                {
                success = writePmuI2C(currentAddress, currentSubaddress, dataBuffer, (IOByteCount) nBytes );
                return(success);
                }
            else
                success = waitForCompletion();
        }
    }
    else {
        // We are interrupt driven, so create the syncer so
        // that we know we will wait here until the transfer is
        // finished
        semaphore_create(current_task(), (semaphore**)&mySync, SYNC_POLICY_FIFO, 0);

        // Enables the interrupts:
        myProvider->enableInterrupt(0);

        if ((success = setAddressAndDirection()) == true ) {
            // Waits until the transfer is finished (look in the interrupt driver where this
            // gets the signal).
            //semaphore_wait(mySync);
			timeout.tv_sec = 15;  // 15 second timeout
			timeout.tv_nsec = 0;

            if (0 != semaphore_timedwait( mySync, timeout ))  {
				// timeout exceeded
				success = false;
				semaphore_destroy(current_task(), mySync);  // JUST SO IOLOG IS OK
				myProvider->disableInterrupt(0);   // JUST SO IOLOG IS OK
				IOLog("APPLE I2C WRITE SEMAPHORE EXCEEDED TIMEOUT OR OTHER ERROR\n");
				}
			else 
				{
				semaphore_destroy(current_task(), mySync); // JUST SO IOLOG IS OK
				myProvider->disableInterrupt(0); // JUST SO IOLOG IS OK
				}
       } else {

        // Since we are here we had been already uunblocked and we
        // do not need the semaphore anymore:
        semaphore_destroy(current_task(), mySync);

        // We do not need interrupts anymore so:
        myProvider->disableInterrupt(0);
		}
		if (!(transferWasSuccesful && success))
			IOLog("APPLE I2C WRITE FAILED INTERRUPT TRANSFER, address = 0x%2x\n", address);

    }
   return  (transferWasSuccesful && success);
}

// Reads a block of data at a given address:
bool
PPCI2CInterface::readI2CBus(UInt8 address, UInt8 subAddress, UInt8 *newData, UInt16 len)
{
	mach_timespec_t  timeout;
	// Don't check lock if I'm being called at interrupt context - if you're calling me at interrupt context
	// you better know what you're doing
	if (!ml_at_interrupt_context())
		// If I am not the owner of the lock returns without doing anything.
		if (!IORecursiveLockHaveLock(mutexLock)) {
	#if DEBUGMODE
			IOLog("PPCI2CInterface::readI2CBus I am not the owner of the lock returns without doing anything.\n");
	#endif // DEBUGMODE
			return false;
		}

    bool success = false;
    
    // pointer to the data to be received
    dataBuffer = newData;

    // and the number of bytes still to receive
    nBytes = len;

    // the current transfer address:
    currentAddress = address;

    // the current transfer subAddress:
    currentSubaddress = subAddress;

    // the data direction:
    isReading = true;

    if (pollingMode) {
        if ((success = setAddressAndDirection()) == true ) {
             if (i2cPMU) 
                {
                success = readPmuI2C(currentAddress, currentSubaddress, dataBuffer, (IOByteCount) nBytes );
                return(success);
                }
              else
                success = waitForCompletion();
        }
    }
    else {
        // CURRENTLY THERE ARE NOT DEVICES THAT SUPPORT READING ACCESS TO THE I2C
        // BUS. So To avoid to hang here waiting for an answer that is not going to
        // come we return false immedialty:
        // Later, we should start a timer that will fire if no data has arrived.
        // return false;

        // We are interrupt driven, so create the syncer so
        // that we know we will wait here until the transfer is
        // finished
        semaphore_create(current_task(), (semaphore**)&mySync, SYNC_POLICY_FIFO, 0);

        // Enables the interrupts:
        myProvider->enableInterrupt(0);

        if ((success = setAddressAndDirection()) == true ) {
            // Waits until the transfer is finished (look in the interrupt driver where this
            // gets the signal).
            //semaphore_wait(mySync);
			timeout.tv_sec = 15;  // 15 second timeout
			timeout.tv_nsec = 0;

            if (0 != semaphore_timedwait( mySync, timeout ))  {
				// timeout exceeded
				success = false;
				semaphore_destroy(current_task(), mySync);  // JUST SO IOLOG IS OK
				myProvider->disableInterrupt(0);   // JUST SO IOLOG IS OK
				IOLog("APPLE I2C READ SEMAPHORE EXCEEDED TIMEOUT OR OTHER ERROR\n");
				}
			else 
				{
				semaphore_destroy(current_task(), mySync); // JUST SO IOLOG IS OK
				myProvider->disableInterrupt(0); // JUST SO IOLOG IS OK
				}
       } else {

        // Since we are here we had been already uunblocked and we
        // do not need the semaphore anymore:
        semaphore_destroy(current_task(), mySync);

        // We do not need interrupts anymore so:
        myProvider->disableInterrupt(0);
		}
		if (!(transferWasSuccesful && success))
			IOLog("APPLE I2C READ FAILED INTERRUPT TRANSFER, address = 0x%2x\n", address);
	}
    return  (transferWasSuccesful && success);
}

// End using the interface:
bool
PPCI2CInterface::closeI2CBus()
{
	// Interrupt context calls do not (should not) call openI2CBus ...
    // If I am not the owner of the lock returns without doing anything.
    if (!IORecursiveLockHaveLock(mutexLock)) {
#if DEBUGMODE
            IOLog("PPCI2CInterface::closeI2CBus I am not the owner of the lock returns without doing anything.\n");
#endif // DEBUGMODE
        return false;
    }

    // just in case the client set the driver to function as interrupt driven
    // this sets it back in polling mode. (basically disables the interrupts
    // if needed).
    setPollingMode(true);

    // And releases the lock:
    IORecursiveLockUnlock(mutexLock);

#if DEBUGMODE
    IOLog("PPCI2CInterface::closeI2CBus PPCI2CInterface->mutexLock unlocked.\n");
#endif // DEBUGMODE
#ifdef DEBUGPMU
    if (i2cPMU)
        IOLog("      APPLE I2C-PMU  CLOSED ,  SUCCESSFULLY! \n");
#endif // DEBUGPMU

    
    return true;
}

// Returns the name of the interface depending from
// where the interface attaches:
const char *
PPCI2CInterface::getResourceName()
{
    OSData *t;
    char *dot = ".";
    // creates the bottom part of the string from the driver name:
    strcpy(resourceName, getName());

    // builds the resource name:
    if (!i2cPMU)
        t = OSDynamicCast(OSData, getProvider()->getProperty("AAPL,driver-name"));
    else {
        t = OSDynamicCast(OSData, getProvider()->getProperty("name"));
        strncat(resourceName, (char*)dot, 1);
        }
    if (t != NULL)
        strncat(resourceName, (char*)t->getBytesNoCopy(), t->getLength());
    
#ifdef DEBUGMODE
    IOLog("PPCI2CInterface::getResourceName returns \"%s\"\n",resourceName);
#endif // DEBUGMODE
    
    return (resourceName);
}

// Method names for the callPlatformFunction:
#define kWriteI2Cbus				"writeI2CBus"
#define kReadI2Cbus					"readI2CBus"
#define kOpenI2Cbus					"openI2CBus"
#define kCloseI2Cbus				"closeI2CBus"
#define kSetDumbMode				"setDumbMode"
#define kSetPollingMode				"setPollingMode"
#define kSetStandardMode			"setStandardMode"
#define kSetStandardSubMode			"setStandardSubMode"
#define kSetCombinedMode			"setCombinedMode"
#define kRegisterForI2cInterrupts 	"registerForI2cInterrupts"
#define kDeRegisterI2cClient 		"deRegisterI2cClient"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// Method: callPlatformFunction
//
// Purpose:
//         Overides the standard callPlatformFunction to catch all the calls
//         that may be directed to the PPCI2CInterface.
IOReturn
PPCI2CInterface::callPlatformFunction( const OSSymbol *functionSymbol,
                                       bool waitForFunction,
                                       void *param1, void *param2,
                                       void *param3, void *param4 ) {
    
	const char *functionName = functionSymbol->getCStringNoCopy();   // for 3203380
#ifdef DEBUGPMU
    IOLog("APPLE I2C::CALL PLATFORM FUNCTION calling %s %s 0x%08lx 0x%08lx 0x%08lx 0x%08lx\n",
          functionName, (waitForFunction ? "TRUE" : "FALSE"),
          param1, param2, param3, param4);
#endif // DEBUGPMU

	if (strcmp(functionName, kWriteI2Cbus) == 0) {
    
            if (writeI2CBus((UInt8)param1, (UInt8)param2, (UInt8 *)param3, (UInt16)param4))  
                return (kIOReturnSuccess);
            else 
               return (kIOReturnBadArgument);
    }
    else if (strcmp(functionName, kReadI2Cbus) == 0) {
            if (readI2CBus((UInt8)param1, (UInt8)param2, (UInt8 *)param3, (UInt16)param4))  
                return (kIOReturnSuccess);
            else 
               return (kIOReturnBadArgument);
    }
    else if (strcmp(functionName, kOpenI2Cbus) == 0) {
            if (openI2CBus((UInt8) param1))    // param1 = UInt8 bus ID
                return (kIOReturnSuccess);
            else 
               return (kIOReturnBadArgument);
    }
    else if (strcmp(functionName, kCloseI2Cbus) == 0) {
            if (closeI2CBus())  
                return (kIOReturnSuccess);
            else 
               return (kIOReturnBadArgument);
    }
    else if (strcmp(functionName, kRegisterForI2cInterrupts) == 0) {
            assert((param2 != 0) && (param3 != 0));  // OK for param1 to be 0
       
            if (registerForI2cInterrupts((UInt32)param1, (AppleI2Cclient)param2, (IOService *) param3))  
                return (kIOReturnSuccess);
            else 
               return (kIOReturnBadArgument);
    }
    else if (strcmp(functionName, kDeRegisterI2cClient) == 0) {
            assert(param2 != 0);  // OK for param1 to be 0
            
            if (deRegisterI2cClient((UInt32)param1, (IOService *)param2))  
                return (kIOReturnSuccess);
            else 
               return (kIOReturnBadArgument);
    }
    else if (strcmp(functionName, kSetDumbMode) == 0) {
             setDumbMode();  
                return (kIOReturnSuccess);
    }
    else if (strcmp(functionName, kSetStandardMode) == 0) {
             setStandardMode();  
                return (kIOReturnSuccess);
    }
    else if (strcmp(functionName, kSetStandardSubMode) == 0) {
             setStandardSubMode();  
                return (kIOReturnSuccess);
    }
    else if (strcmp(functionName, kSetCombinedMode) == 0) {
             setCombinedMode();  
                return (kIOReturnSuccess);
    }
    else if (strcmp(functionName, kSetPollingMode) == 0) {
            if (setPollingMode((bool) param1))
                return (kIOReturnSuccess);
            else 
               return (kIOReturnBadArgument);
    }
    
    // If we are here it means we failed to find the correct call
    // so we pass the parameters to the function above:
    return super::callPlatformFunction (functionSymbol, waitForFunction,
										param1, param2, param3, param4);
}
IOReturn
PPCI2CInterface::callPlatformFunction( const char *functionName,
                                       bool waitForFunction,
                                       void *param1, void *param2,
                                       void *param3, void *param4 )
{
    // just pass it along; avoids 3203380
    return super::callPlatformFunction(functionName, waitForFunction,
                                       param1, param2, param3, param4);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* ApplePMUSendMiscCommand						 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*static*/ IOReturn
PPCI2CInterface::ApplePMUSendMiscCommand( UInt32 command,
                        IOByteCount sendLength, UInt8 * sendBuffer,
                        IOByteCount * readLength, UInt8 * readBuffer )
{
    struct SendMiscCommandParameterBlock {
        int command;  
        IOByteCount sLength;
        UInt8 *sBuffer;
        IOByteCount *rLength; 
        UInt8 *rBuffer;
    };
    IOReturn ret = kIOReturnError;
    static IOService * pmu = 0;

    // Wait for the PMU to show up:
    if( !pmu)
        pmu = IOService::waitForService(IOService::serviceMatching("ApplePMU"));

    SendMiscCommandParameterBlock params = { command, sendLength, sendBuffer,
                                                      readLength, readBuffer };
    if( pmu)
        ret = pmu->callPlatformFunction( "sendMiscCommand", true,
                                         (void*)&params, NULL, NULL, NULL );
    return( ret );
}
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* readPmuI2C								 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*static*/ bool
PPCI2CInterface::readPmuI2C( UInt8 address, UInt8 subAddress, UInt8 * readBuf, IOByteCount count )
{
    IOReturn 		ioResult = kIOReturnError;
    PMUI2CPB		iicPB;
    IOByteCount		readLen;
    UInt32		retries;		// loop counter for retry attempts
    bool 		success;
    UInt8		myreadbuffer[256];
    IOByteCount		length;			

    success = false;			// assume error
    retries = MAXIICRETRYCOUNT;
    while (retries--) {

        iicPB.bus		= portSelect;
        iicPB.xferType		= lastMode;		// assumes kSimpleI2Cstream on open, unless changed after open
        iicPB.secondaryBusNum	= 0;			// not currently supported
        iicPB.address		= address;
        iicPB.subAddr		= subAddress;		// (don't care if kSimpleI2Cstream)
        iicPB.combAddr		= address;		// (don't care in kSimpleI2Cstream)
        iicPB.dataCount		= count;                // (count set to count + 3 in clock-spread clients) 
            if (lastMode == kCombinedI2CStream)  
                    iicPB.address	&= 0xFE;	// combined mode has "write" version of address                
        readLen 		= count;	       	// added one byte for the leading pmu status byte	
        length 			= (short) ((UInt8 *)&iicPB.data - (UInt8 *)&iicPB.bus);   

        ioResult = ApplePMUSendMiscCommand( kPMUI2CCmd, length, (UInt8 *) &iicPB, &readLen, myreadbuffer );   

        if ((ioResult == kIOReturnSuccess) && (myreadbuffer[0] == STATUS_OK)) 
            break;					// if pb accepted, proceed to status/read phase

        IOSleep( 15 );
    }
#ifdef DEBUGPMU
                IOLog("READ PMU MID STATUS, retries = 0x%02x\n", MAXIICRETRYCOUNT - retries);
#endif // DEBUGPMU
            

    if (myreadbuffer[0] == STATUS_OK)
    {
    	ioResult = kIOReturnError;
        success = false;			// assume error again
        retries = MAXIICRETRYCOUNT;
        while (retries--)
        {
            IOSleep( 15 );

            iicPB.bus		= kI2CStatusBus;
            readLen 		= count+1;		// added one byte for the leading pmu status byte
            myreadbuffer[0]	= 0xff;

            ioResult = ApplePMUSendMiscCommand( kPMUI2CCmd, 1, (UInt8 *) &iicPB, &readLen, myreadbuffer );

            if ( (ioResult == kIOReturnSuccess) && ( ( myreadbuffer[0] == STATUS_DATAREAD ) ||  ((SInt8)myreadbuffer[0] >= STATUS_OK ) ) )
            {
                if ((SInt8)myreadbuffer[0] >= STATUS_DATAREAD )
                    {
                    bcopy( 1 + myreadbuffer, readBuf, readLen-1 ); // strip pmu I2C status byte
                    success = true;
                    };
#ifdef DEBUGPMU
        IOLog("READ I2C PMU END - STATUS OK:  retries = 0x%02x\n", MAXIICRETRYCOUNT - retries);
        IOLog("addr = 0x%02x, subAd = 0x%02x, rdBuf = 0x%02x, count = 0x%ld)\n", address, subAddress, myreadbuffer[0], count);
#endif // DEBUGPMU
                break;
            }
        }
    }
    return(success);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* writePmuI2C								 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*static*/ bool
PPCI2CInterface::writePmuI2C( UInt8 address, UInt8 subAddress, UInt8 * buffer, IOByteCount count )
{
    IOReturn 		ioResult = kIOReturnError;
    PMUI2CPB		iicPB;
    IOByteCount		readLen;
    UInt8		readBuf[8];
    UInt32		retries;		// loop counter for retry attempts
    bool 		success;
    IOByteCount		length;	

#ifdef DEBUGPMU
    IOLog("WRITE PMU START\n");
    IOLog("addr = 0x%02x, subAd = 0x%02x, wrBuf = 0x%02x, count = 0x%ld)\n", address, subAddress, buffer[0], count);
#endif // DEBUGPMU

    ioResult = kIOReturnError;
    success = false;				// assume error
    retries = MAXIICRETRYCOUNT;
    while (retries--) {
        UInt8 cnt;
        iicPB.bus		= portSelect;
        iicPB.xferType		= lastMode;		// assumes kSimpleI2Cstream on open, unless changed after open
        iicPB.secondaryBusNum	= 0;			// not currently supported
        iicPB.address		= address;
        iicPB.subAddr		= subAddress;		// (don't care in kSimpleI2Cstream)
        iicPB.combAddr		= address;		// (don't care in kSimpleI2Cstream)
        iicPB.dataCount		= count;                // (count set to count + 3 in clock-spread clients)
            if (lastMode == kCombinedI2CStream)  
                    iicPB.address	&= 0xFE;	// combined mode has "write" version of address                

        for (cnt = 0; cnt < count; cnt++)		// copy to client's buffer
            iicPB.data[cnt] = buffer[cnt];			
                
        readLen 		= 1;			// status return only
        readBuf[0]		= 0xff;
        length = (short) (&iicPB.data[iicPB.dataCount] - &iicPB.bus);  


        ioResult = ApplePMUSendMiscCommand( kPMUI2CCmd, length, (UInt8 *) &iicPB, &readLen, readBuf );

        if ( (ioResult == kIOReturnSuccess) && (readBuf[0] == STATUS_OK) )
            break;			// if pb accepted, proceed to status phase

        IOSleep( 15 );
    }
#ifdef DEBUGPMU
                IOLog("WRITE PMU MID STATUS, retries = 0x%02x\n", MAXIICRETRYCOUNT - retries);
#endif // DEBUGPMU

   if (readBuf[0] == STATUS_OK) {

        ioResult = kIOReturnError;
        success = false;			// assume error
        retries = MAXIICRETRYCOUNT;
        while (retries--) {

            IOSleep( 15 );

            // attempt to recover status

            iicPB.bus		= kI2CStatusBus;
            readLen 		= sizeof( readBuf );
            readBuf[0]		= 0xff;

            ioResult = ApplePMUSendMiscCommand( kPMUI2CCmd, 1, (UInt8 *) &iicPB, &readLen, readBuf );

            if ( (ioResult == kIOReturnSuccess) && (readBuf[0] == STATUS_OK) ) {
#ifdef DEBUGPMU
                IOLog("WRITE PMU STATUS OK, retries = 0x%02x\n", MAXIICRETRYCOUNT - retries);
#endif // DEBUGPMU
                success = true;
                break;
            }
        }
    }

    return(success);
}
//****************************************************************************************************************
// Client handling methods:
// ------------------------

// --------------------------------------------------------------------------
//
// Method: addI2Cclient
//
// Purpose:
//         adds a client to the list. The arguments are the addressInfo mask that
//         activates the notification for this client and obviously the callback
//         and the object representing the client.  
//
bool PPCI2CInterface::addI2Cclient(UInt32 addressInfo, AppleI2Cclient function, IOService * caller)
{
    bool result = false;
    
    if (clientMutexLock != NULL)
        IOLockLock(clientMutexLock);
    
    I2CclientPtr newClient = (I2CclientPtr)IOMalloc(sizeof(I2Cclient));

    if (newClient != NULL) {
        // Just creates a new link and attaches to the top of the list (there
        // is no order in the list so we can attach it anywhere).
        newClient->nextClient = listHead;
        newClient->addressInfo = addressInfo;
        newClient->client = caller;
        newClient->callBackFunction = function;

        listHead = newClient;
        
#ifdef DEBUGPMU
		IOLog("APPLEI2C::addI2Cclient - NEW INTERRUPT CLIENT ADDED\n");
		char 		debugInfo[128];
		sprintf(debugInfo, "APPLE_PMU ADD CLIENT: interruptMask = 0x01\n");
		setProperty("PMU_AddClient", debugInfo);   

        IOLog ("APPLEI2C::addI2Cclient - NEW INTERRUPT CLIENT ADDED\n");
#endif // DEBUGPMU

        // The only possible problem was the lack of memory.
        result = true;
    }

    if (clientMutexLock != NULL)
         IOLockUnlock(clientMutexLock);

    return result;
}

// --------------------------------------------------------------------------
//
// Method: removeI2Cclient
//
// Purpose:
//         removes a client to the list.  
bool
PPCI2CInterface::removeI2Cclient(UInt32 addressInfo, IOService * caller)
{

    if (clientMutexLock != NULL)
        IOLockLock(clientMutexLock);

    I2CclientPtr tmp, tmpPrev = NULL;

    for (tmp = listHead; tmp != NULL; tmp = tmp->nextClient) {
        if ((tmp->client == caller) && (tmp->addressInfo == addressInfo)) 
            {

            // detaches the link from the list:
            if (tmp == listHead) {
                listHead = tmp->nextClient;
            }
            else if (tmpPrev !=NULL) {
                tmpPrev->nextClient = tmp->nextClient;
            }
            else {
                // If it is not the head of the list tmpPrev should be differnt
                // than 0, so perhaps an assertion would be more proper here.
                // for now let's just log an error and report a failure:

#ifdef DEBUGPMU
                IOLog("PPCI2CInterface:removeI2Cclient, REMOVED INTERRUPT CLIENT\n");
#endif // DEBUGPMU

                return false;
            }

            // And since we are here it means that the client is unlinked and found
            // so we can remove it and report a success.
            IOFree(tmp, sizeof(I2Cclient));

            if (clientMutexLock != NULL)
                 IOLockUnlock(clientMutexLock);
            
            return true;
        }
        else {
            tmpPrev = tmp;
        }
    }

    if (clientMutexLock != NULL)
         IOLockUnlock(clientMutexLock);
    
    // If we are here it means we did not find the client with the
    // given object.
    return false;
}

// --------------------------------------------------------------------------
//
// Method: clearI2CclientList
//
// Purpose:
//         removes all clients from the list.  
bool
PPCI2CInterface::clearI2CclientList()
{
    // Do not be tempted to add a  lock here.
    // removeI2Cclient already has its mutex lock
    // so here is not only un-necessary but also
    // harmful.

    bool success = true;

    while (listHead != NULL) {
        // I could just remove all from here. But doing a call
        // to the removeI2Cclient allows me to change the structure
        // of the list (if one day I'll wish to do so) and to test
        // the removeI2Cclient function.
        success &= removeI2Cclient(listHead->addressInfo, listHead->client);
    }

    return success;
}

// --------------------------------------------------------------------------
//
// Method: calli2cClient
//
// Purpose:
//         calls a client in the list with the data from the current interrupt.
//	   
bool
PPCI2CInterface::calli2cClient(UInt8 interruptMask, UInt32 length, UInt8 * buffer)
{
#define kMaxClients 64
    I2CclientPtr	clientToCall, clientsCalled[kMaxClients];
    long		clientCalledCount, index;
    bool 		success = false;
    UInt8		mybuffer[256];

    clientCalledCount = 0;
    UInt8 myTypeInfo = buffer[0];
   
    if ( !( (interruptMask == 0x01) && (myTypeInfo == 0x00) ) )  // FIXME: so that ApplePMU strips mask and type off
        {
        return (false);
        }

    UInt32 myAddrInfo = (((((UInt32) buffer[1]) << 16 ) & 0xFF0000) |   	//bus
                         ((((UInt32) buffer[2]) << 8 ) & 0xFF00) |		//address
                          ((((UInt32) buffer[3]) << 0 ) & 0xFF));		//subAddr
	/*
	 * Walk through the list looking for clients to call.  There are three loops here:
	 *
	 *	1) The innermost for() loop simply walks through the clientCalled array to make
	 *		sure we haven't already called this client
	 *	2) The for() loop surrounding that walks through the list of clients to find
	 *		one to call.  It starts from the beginning of the list everytime.
	 *
	 *	Both the above loops are protected by the clientMutexLock so the list doesn't
	 *	get changed behind our backs.
	 *
	 *	3) In the outermost do() loop, once a clientToCall has been identified, we release the
	 *		lock and call it, then return to look for more clients to call.  We make the
	 * 		client callBackFunction outside the lock because if the callback doesn't return
	 *		we never get a chance to unlock the lock and we start leaking threads [2743482,
	 *		2749519].  Having released the lock, we return to loop (2) and start it at the
	 *		beginning of the list in case the list has changed.
	 */
	do {
		if (clientMutexLock != NULL)
			IOLockLock(clientMutexLock);
		
		// Find a client matching the given interrupt mask and
		// call it with the given buffer. NOTE: the clients are
		// not allowed to modify the buffer. If they need to make
		// changes they are supposed to make a copy of it.
	
		for (clientToCall = listHead; clientToCall != NULL; clientToCall = clientToCall->nextClient) 
                {
#ifdef DEBUGPMU
//                    IOLog ("APPLEI2C::callI2Cclient - Check For Match: clientAddressInfo = 0x%08lx, myAddrInfo = 0x%08lx!!\n", clientToCall->addressInfo, myAddrInfo);
#endif // DEBUGPMU

			if (clientToCall->addressInfo == myAddrInfo)  
                        {                             
				// See if this client has already been called
				for (index = 0; index < clientCalledCount; index++) 
					// If clientToCall is in clientsCalled list, then we skip it
					if (clientToCall == clientsCalled[index]) 
						break;
				/*
				 * If we got all the way through the clientsCalled array without finding
				 * the current clientToCall (index == clientCalledCount), then we break out
				 * of this loop and call it
				 */
				if (index == clientCalledCount) 
					// Did not find client in list of clients we've already
					// called, so break out and call this one
					break;
			}
		}
		
		if (clientMutexLock != NULL)
			IOLockUnlock(clientMutexLock);
		
		// Call this client
		if (clientToCall != NULL) {
            // since we found one client (at least one client) we can
            // consider the call successful:
            success = true;
			
                    bcopy( 4 + buffer, mybuffer, length-4 );   // strip off 4 leading bytes (type, bus, addr, subaddr)
                                                              // FIXME: so that ApplePMU strips type off.
#ifdef DEBUGPMU
                    IOLog ("APPLEI2C::callI2Cclient - calling back client.........\n");
#endif // DEBUGPMU

			(*clientToCall->callBackFunction)(clientToCall->client, clientToCall->addressInfo, length-4, mybuffer);
			
			// Remember that we've called this client
			clientsCalled[clientCalledCount++] = clientToCall;
			if (clientCalledCount >= kMaxClients) {
				// Shouldn't happen
				IOLog ("PPCI2CInterface::callI2Cclient - too many clients to call\n");
				break;
			}
		}
		
	} while (clientToCall != NULL);

    return (success);
}
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* registerForI2cInterrupts						 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*static*/ bool
PPCI2CInterface::registerForI2cInterrupts(UInt32 addressInfo, AppleI2Cclient function, IOService * caller)
{
    IOReturn ret = kIOReturnError;
    static IOService * pmu;

    // Wait for the PMU to show up.
    // If already register with PMU, don't bother registering again
    if ( !registeredForPmuI2C && !pmu) 
        {
#ifdef DEBUGPMU
        IOLog ("APPLEI2C::RegisterI2Cclient1\n");
#endif // DEBUGPMU
        pmu = IOService::waitForService(IOService::serviceMatching("ApplePMU"));
        }
        
    if ( !registeredForPmuI2C && pmu ) 
        {
#ifdef DEBUGPMU
        IOLog ("APPLEI2C::RegisterI2Cclient2\n");
#endif // DEBUGPMU
        ret = pmu->callPlatformFunction( "registerForPMUInterrupts", true, (void*)0x01, (void*)handlePMUi2cInterrupt, (void*)this, NULL );
        }
    if ( (ret==kIOReturnSuccess) || registeredForPmuI2C )
        {
#ifdef DEBUGPMU
        IOLog ("APPLEI2C::RegisterI2Cclient3 - clientAddressInfo = 0x%08lx\n", addressInfo);
#endif // DEBUGPMU
        registeredForPmuI2C = true;  //one registration with PMU is sufficient for all clients handled here
        return (addI2Cclient(addressInfo, function, caller)); 
        }

    else 
        {
#ifdef DEBUGPMU
        IOLog("APPLEI2C::RegisterI2Cclient FAILED to register for interrupts !!\n");
#endif // DEBUGPMU
        return( false );
        }

}    
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* deRegisterI2cClient						 	*/
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
// Method: deRegisterI2cClient
//
// Purpose:
//        This is to de-register from the clients that wish to be aware of i2c transactions
bool
PPCI2CInterface::deRegisterI2cClient(UInt32 addressInfo, IOService * caller)
{    
    // Just calls into the private method:
    return removeI2Cclient(addressInfo, caller);
}



/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* handlePMUi2cInterrupt						 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* static */ void
PPCI2CInterface::handlePMUi2cInterrupt(IOService *client, UInt8 interruptMask, UInt32 length, UInt8 *buffer)
{
    PPCI2CInterface *i2c = OSDynamicCast(PPCI2CInterface, client);

    if (i2c != NULL)
        i2c->calli2cClient(interruptMask, length, buffer);  //FIXME: interruptMask should be stripped out in ApplePMU
}

