/*
 *  AppleDallasDriver.cpp
 *  AppleDallasDriver
 *
 *  Created by Keith Cox on Tue Jul 17 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IORegistryEntry.h>

#include "AppleDallasDriver.h"
#include "DallasROM.h"
#include "AudioHardwareUtilities.h"
extern "C" {
#include <pexpert/pexpert.h>
}

// Define my superclass
#define super IOService

// REQUIRED! This macro defines the class's constructors, destructors,
// and several other methods I/O Kit requires.  Do NOT use super as the
// second parameter.  You must use the literal name of the superclass.
OSDefineMetaClassAndStructors(AppleDallasDriver, IOService)

bool ROMReset (UInt8 *gpioPtr)
{
    AbsoluteTime start, now, finish;
    UInt64       ns;
    UInt8	 bit;
	UInt8		outputZero, outputTristate;
    bool         correctTiming;
    bool         bitEdge;
    int          errorCount = 0;
    bool         res;    
	
	outputZero      = (*gpioPtr & 0x80) |  kGPIODriveOut;  // set output select0, altoe=0, ddir=1, output value = 0
														   // save bit 7
	outputTristate  = (*gpioPtr & 0x80) | !kGPIODriveOut;  // output bit = 0, but output tristated to get a 1

    res = TRUE;
    // Make sure the ROM is present
    *gpioPtr = outputTristate; OSSynchronizeIO();
    if ((*gpioPtr&kGPIOPin)==0) {
        IOSleep(1);
        FailWithAction ((*gpioPtr&kGPIOPin)==0, debugIOLog("[DALLAS] ROM Reset: No speaker ROM detected\n"), exit);
    }

    correctTiming = FALSE;
    while (!correctTiming)
    {
        debugIOLog("[DALLAS] ROM Reset\n");
    
        correctTiming = TRUE;
        
        clock_get_uptime(&start);
        *gpioPtr = outputZero; OSSynchronizeIO();
        IODelay(kROMResetPulseMin);
        *gpioPtr = outputTristate; OSSynchronizeIO();
        
        ns = kROMResetPulseMax*1000;
        nanoseconds_to_absolutetime(ns, &finish);
        ADD_ABSOLUTETIME(&finish, &start);
        
        bitEdge = FALSE;  // Wait for rising edge of reset pulse
        while (!bitEdge && correctTiming)
        {
            bit = *gpioPtr & kGPIOPin;
            clock_get_uptime(&now);
            
            if (CMP_ABSOLUTETIME(&now, &finish)>0) {
                correctTiming = FALSE;
                debugIOLog("[DALLAS] ROM Reset: Failed Reset Rising Edge\n");
            } else if (bit) {
                bitEdge = TRUE;
            } else {
                IODelay(1);
            }
        }
        
        ns = kROMPresenceDelayMax*1000;
        nanoseconds_to_absolutetime(ns, &finish);
        ADD_ABSOLUTETIME(&finish, &now);
        
        bitEdge = FALSE;  // Wait for falling edge of presence pulse
        while (!bitEdge && correctTiming)
        {
            bit = *gpioPtr & kGPIOPin;
            clock_get_uptime(&now);
            
            if (CMP_ABSOLUTETIME(&now, &finish)>0) {
                correctTiming = FALSE;
                IOSleep(1);
//                debugIOLog("[DALLAS] ROM Reset: Failed to detect Presence Pulse\n");
            } else if (!bit) {
                bitEdge = TRUE;
            } else {
                IODelay(1);
            }
        }
        
        ns = kROMPresencePulseMax*1000;
        nanoseconds_to_absolutetime(ns, &finish);
        ADD_ABSOLUTETIME(&finish, &now);
        
        bitEdge = FALSE;  // Wait for rising edge of presence pulse
        while (!bitEdge && correctTiming)
        {
            bit = *gpioPtr & kGPIOPin;
            clock_get_uptime(&now);
            
            if (CMP_ABSOLUTETIME(&now, &finish)>0) {
                correctTiming = FALSE;
//                debugIOLog("[DALLAS] ROM Reset: Presence Pulse too long\n");
            } else if (bit) {
                bitEdge = TRUE;
            } else {
                IODelay(1);
            }
        }
        
        if (!correctTiming)
        {
            FailWithAction(++errorCount > 10, debugIOLog("[DALLAS] ROM Reset: Too many failures, bailing out\n"), exit);
        }
        IOSleep(1);  // Give up the cpu after all attempts
    }

    res = FALSE;
    
exit:
    return res;
}

bool ROMSendByte (UInt8 *gpioPtr, UInt8 theByte)
{
    AbsoluteTime start, now, finish;
    UInt64       ns;
    UInt8	 bit;
	UInt8		outputZero, outputTristate;
    bool         correctTiming;
    bool         bitEdge;
    bool         res;
    int          i;
    
    debug2IOLog("[DALLAS] ROM Send 0x%02X\n", theByte);
	
	outputZero      = (*gpioPtr & 0x80) |  kGPIODriveOut;  // set output select0, altoe=0, ddir=1, output value = 0
														   // save bit 7
	outputTristate  = (*gpioPtr & 0x80) | !kGPIODriveOut;  // output bit = 0, but output tristated to get a 1

    res = FALSE;		// assume success
    for (i=0; i<8; i++)
    {
        correctTiming = TRUE;
        if (theByte&(1<<i)) {	// bit=1
            clock_get_uptime(&start);
            *gpioPtr = outputZero; OSSynchronizeIO();
            IODelay(kROMWrite1Min);
            *gpioPtr = outputTristate; OSSynchronizeIO();
            
            ns = kROMWrite1Max*1000;
            nanoseconds_to_absolutetime(ns, &finish);
            ADD_ABSOLUTETIME(&finish, &start);
            
            bitEdge = FALSE;  // Wait for rising edge of bit
            while (!bitEdge && correctTiming)
            {
                bit = *gpioPtr & kGPIOPin;
                clock_get_uptime(&now);
                
                if (CMP_ABSOLUTETIME(&now, &finish)>0) {
                    correctTiming = FALSE;
                    debug2IOLog("[DALLAS] ROM Send Byte: Failed Bit %d Rising Edge\n", i);
                } else if (bit) {
                    bitEdge = TRUE;
                } else {
                    IODelay(1);
                }
            }
        } else {		// bit = 0
            clock_get_uptime(&start);
            ns = kROMWrite0Max*1000;
            nanoseconds_to_absolutetime(ns, &finish);
            ADD_ABSOLUTETIME(&finish, &start);
            
            *gpioPtr = outputZero; OSSynchronizeIO();
            IODelay(kROMWrite0Min);
            *gpioPtr = outputTristate; OSSynchronizeIO();
            
            bitEdge = FALSE;  // Wait for rising edge of bit
            while (!bitEdge && correctTiming)
            {
                bit = *gpioPtr & kGPIOPin;
                clock_get_uptime(&now);
                
                if (CMP_ABSOLUTETIME(&now, &finish)>0) {
                    correctTiming = FALSE;
                    debug2IOLog("[DALLAS] ROM Send Byte: Failed Bit %d Rising Edge\n", i);
                } else if (bit) {
                    bitEdge = TRUE;
                } else {
                    IODelay(1);
                }
            }
        }
        
        if (!correctTiming)
            res = TRUE;

        ns = kROMTSlot*1000;		// Minimum delay to guarantee timing between bits
        nanoseconds_to_absolutetime(ns, &finish);
        ADD_ABSOLUTETIME(&finish, &start);
        clock_get_uptime(&now);
        if (CMP_ABSOLUTETIME(&now, &finish)<0) {
            SUB_ABSOLUTETIME(&finish, &now);
            absolutetime_to_nanoseconds(finish, &ns);
            IODelay((ns/1000)+1);
        }
	}

	IOSleep(1);					// Give up CPU every byte
    return res;
}

bool ROMReadByte (UInt8 *gpioPtr, UInt8* theByte)
{
    AbsoluteTime start, finish, setup, before, after, lastZero;
    AbsoluteTime setupLimit, bit0Limit, bit1Limit;
    UInt64       ns;
    UInt8	 bit;
	UInt8		outputZero, outputTristate;
    UInt8        theValue = 0;
    bool         correctTiming;
    bool         bitEdge;
    bool         res;
    int          i;
    
	
	outputZero      = (*gpioPtr & 0x80) |  kGPIODriveOut;  // set output select0, altoe=0, ddir=1, output value = 0
														   // save bit 7
	outputTristate  = (*gpioPtr & 0x80) | !kGPIODriveOut;  // output bit = 0, but output tristated to get a 1

    res = FALSE;		// assume success
    for (i=0; i<8; i++)
    {
        correctTiming = TRUE;
        
        clock_get_uptime(&start);
        *gpioPtr = outputZero; OSSynchronizeIO();
        IODelay(kROMReadSetup);
        *gpioPtr = outputTristate; OSSynchronizeIO();
        clock_get_uptime(&setup);

        ns = kROMTSlot*1000;
        nanoseconds_to_absolutetime(ns, &finish);
        ADD_ABSOLUTETIME(&finish, &start);
        
       
        bitEdge = FALSE;  // Wait for rising edge of bit
        while (!bitEdge && correctTiming)
        {
            clock_get_uptime(&before);
            bit = *gpioPtr & kGPIOPin;
            clock_get_uptime(&after);
            
            if (CMP_ABSOLUTETIME(&after, &finish)>0) {
                correctTiming = FALSE;
                debugIOLog("[DALLAS] Late rising edge\n");
            } else if (bit) {
                bitEdge = TRUE;
            } else {
                lastZero = before;
                IODelay(1);
            }
        }
        
        ns = (kROMReadSetup+kROMReadMargin)*1000;
        nanoseconds_to_absolutetime(ns, &setupLimit);
        ADD_ABSOLUTETIME(&setupLimit, &start);
        
        if (CMP_ABSOLUTETIME(&setup, &setupLimit)>0) {
            correctTiming = FALSE;
            debugIOLog("[DALLAS] Failed setup time\n");
        }
                
        if (correctTiming) {
            ns = kROMRead1TimeMax*1000;
            nanoseconds_to_absolutetime(ns, &bit1Limit);
            ADD_ABSOLUTETIME(&bit1Limit, &start);
                
            ns = kROMRead0TimeMin*1000;
            nanoseconds_to_absolutetime(ns, &bit0Limit);
            ADD_ABSOLUTETIME(&bit0Limit, &start);
            
            if (CMP_ABSOLUTETIME(&after, &bit1Limit)<0) {
                theValue |= 1 << i;
                debug2IOLog("[DALLAS] 1 (%ld ns)", (UInt32) ns);
            } else if (CMP_ABSOLUTETIME(&lastZero, &bit0Limit)>0) {
                theValue |= 0 << i;
                debug2IOLog("0 (%ld ns)", (UInt32) ns);
            } else {
                correctTiming = FALSE;
                debugIOLog("[DALLAS] Failed to meet bit timing\n");
            }
        }
        
        if (!correctTiming) {
            res = TRUE;
        }
        
        ns = kROMTSlot*1000;		// Minimum delay to guarantee timing between bits
        nanoseconds_to_absolutetime(ns, &finish);
        ADD_ABSOLUTETIME(&finish, &start);
        clock_get_uptime(&after);
        if (CMP_ABSOLUTETIME(&after, &finish)<0) {
            SUB_ABSOLUTETIME(&finish, &after);
            absolutetime_to_nanoseconds(finish, &ns);
            IODelay((ns/1000)+1);
        }
    }

    IOSleep(1);			// give up CPU every byte
	*theByte = theValue;
    debug2IOLog("[DALLAS]   ROM Read 0x%02x\n", *theByte);
    return res;
}

void ROMCheckCRC(UInt8 *bROM)
{
    int i, j;
    UInt8 crc[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    UInt8 currByte, newBit;
    UInt8 finalCRC;
    
    for (i=0; i<7; i++) {
        currByte = bROM[i];
        for (j=0; j<8; j++) {
            newBit = crc[7] ^ (currByte&0x01);
            crc[7] = crc[6];
            crc[6] = crc[5];
            crc[5] = crc[4] ^ newBit;
            crc[4] = crc[3] ^ newBit;
            crc[3] = crc[2];
            crc[2] = crc[1];
            crc[1] = crc[0];
            crc[0] = newBit;
            currByte >>= 1;
        }
    }
    
    finalCRC = 0;
    for (i=0; i<8; i++) {
        finalCRC = (finalCRC << 1) | crc[i];
    }
    
    if (finalCRC != bROM[7]) {
        debug3IOLog("[DALLAS] CRC Mismatch! 0x%02x 0x%02x\n", finalCRC, bROM[7]);
    } else {
        debug2IOLog("[DALLAS] ROM CRC Match: 0x%02x\n", finalCRC);
    }
    
}

IORegistryEntry * FindEntryByProperty (const IORegistryEntry * start, const char * key, const char * value) 
{
	OSIterator				*iterator;
	IORegistryEntry			*theEntry;
	IORegistryEntry			*tmpReg;
	OSData					*tmpData;

	theEntry = NULL;
	iterator = start->getChildIterator (gIODTPlane);
	FailIf (NULL == iterator, Exit);

	while (NULL == theEntry && (tmpReg = OSDynamicCast (IORegistryEntry, iterator->getNextObject ())) != NULL) {
		tmpData = OSDynamicCast (OSData, tmpReg->getProperty (key));
		if (NULL != tmpData && tmpData->isEqualTo (value, strlen (value))) {
			theEntry = tmpReg;
		}
	}

Exit:
	if (NULL != iterator) {
		iterator->release ();
	}
	return theEntry;
}

bool AppleDallasDriver::init (OSDictionary *dict)
{
    bool res = super::init (dict);
    debugIOLog ("[DALLAS] AppleDallasDriver Initializing\n");
    return res;
}

void AppleDallasDriver::free (void)
{
    debugIOLog ("AppleDallasDriver Freeing\n");
    CLEAN_RELEASE (gpioRegMem)
    super::free ();
}

IOService *AppleDallasDriver::probe (IOService *provider, SInt32 *score)
{
    
    IOService *res = super::probe (provider, score);
    debugIOLog ("[DALLAS] AppleDallasDriver Probing\n");
    return res;
}

bool AppleDallasDriver::readApplicationRegister (UInt8 *bAppReg)
{
    IOMemoryMap     *map = NULL;
    UInt8			*gpioPtr = NULL;
    int			i;
    bool			failure;
    bool			resultSuccess;
	int			retryCount;
    
    resultSuccess = FALSE;
    map = gpioRegMem->map (0);
    FailIf (!map, exit);
    gpioPtr = (UInt8*)map->getVirtualAddress ();
    debug2IOLog("[DALLAS] GPIO16 Register Value = 0x%02x\n", *gpioPtr);

    FailIf (!gpioPtr, exit);
    
    // Look for ROM present
    FailWithAction(ROMReset(gpioPtr), debugIOLog("No speaker ROM detected\n"), exit);
    
    // Read 64b Application Register
    failure = TRUE;
    debugIOLog("[DALLAS] Reading 64b Application Register\n");
	retryCount = kRetryCountSeed;
    for (i=0; i<8; i++) {
        do {
            while (failure && retryCount) {
                ROMReset(gpioPtr);
                failure = ROMSendByte(gpioPtr, kROMSkipROM);
                failure = failure || ROMSendByte(gpioPtr, kROMReadAppReg);
                failure = failure || ROMSendByte(gpioPtr, i);
				retryCount--;
            }
            failure = ROMReadByte(gpioPtr, &bAppReg[i]);
        } while (failure && retryCount);
    }
    if ( !failure ) {
		resultSuccess = TRUE;
	}
	
exit:
    return resultSuccess;
}

bool AppleDallasDriver::readDataROM (UInt8 *bEEPROM,int dallasAddress, int size)
{
#pragma unused ( dallasAddress )
    IOMemoryMap     *map = NULL;
    UInt8           *gpioPtr = NULL;
    int              i;
    bool             failure;
    bool             resultSuccess;
	int				retryCount;
    
    resultSuccess = FALSE;			//	assume that it failed
	
	FailIf ( NULL == bEEPROM, exit );

    map = gpioRegMem->map (0);
    FailIf (!map, exit);
    gpioPtr = (UInt8*)map->getVirtualAddress ();
    debug2IOLog("[DALLAS] GPIO16 Register Value = 0x%02x\n", *gpioPtr);
    FailIf (!gpioPtr, exit);
    
    // Look for ROM present
    FailWithAction(ROMReset(gpioPtr), debugIOLog("No speaker ROM detected\n"), exit);
    
    // Read 256b EEPROM
    failure = TRUE;
    debugIOLog("[DALLAS] Reading 256b EEPROM\n");
	retryCount = kRetryCountSeed;
    while (failure && retryCount) {
        ROMReset(gpioPtr);
        failure = ROMSendByte(gpioPtr, kROMSkipROM);
        failure = failure || ROMSendByte(gpioPtr, kROMReadMemory);
		if( failure ) {
			retryCount--;
		}
    }

	FailIf( failure, exit );
	
    failure = TRUE;
	retryCount = kRetryCountSeed;
    for (i=0; i<size; i++) {
		bEEPROM[0] = 0;		//	device family
		bEEPROM[1] = 0;		//	device type
		bEEPROM[2] = 0;		//	device subtype
		bEEPROM[3] = 0;		//	device reserved
       do {
            while ( failure ) {
                ROMReset ( gpioPtr );
                failure = ROMSendByte ( gpioPtr, kROMSkipROM );
                failure = failure || ROMSendByte ( gpioPtr, kROMReadScratch );
                failure = failure || ROMSendByte ( gpioPtr, i);
				retryCount--;
            }
            failure = ROMReadByte ( gpioPtr, &bEEPROM[i] );
        } while ( failure && retryCount && ( 0xFF != bEEPROM[0] ) );
    }
	if ( !failure ) {
		resultSuccess = TRUE;
	}

exit:
    debug2IOLog("[DALLAS] readDataROM returns %dx\n", (unsigned int)resultSuccess);
    return resultSuccess;
}

bool AppleDallasDriver::readSerialNumberROM (UInt8 *bROM)
{
    IOMemoryMap     *map = NULL;
    UInt8           *gpioPtr = NULL;
    int              i;
    bool             failure;
    bool             resultSuccess;
	int				retryCount;
    
    resultSuccess = FALSE;
    map = gpioRegMem->map (0);
    FailIf (!map, exit);
    gpioPtr = (UInt8*)map->getVirtualAddress ();
    debug2IOLog("[DALLAS] GPIO16 Register Value = 0x%02x\n", *gpioPtr);
    FailIf (!gpioPtr, exit);
    
    // Look for ROM present
    FailWithAction(ROMReset(gpioPtr), debugIOLog("[DALLAS] No speaker ROM detected\n"), exit);
    
    // Read 64b ROM
    failure = TRUE;
    debugIOLog("[DALLAS] Reading 64b ROM\n");
	retryCount = kRetryCountSeed;
    while (failure && retryCount) {
        while (failure && retryCount) {
            ROMReset(gpioPtr);
            failure = ROMSendByte(gpioPtr, kROMReadROM);
			retryCount--;
        }

        for (i=0; i<8 && !failure; i++)
            failure = failure || ROMReadByte(gpioPtr, &bROM[i]);
    }
    if ( !failure ) {
		resultSuccess = TRUE;
	}
exit:
    return resultSuccess;
}

bool AppleDallasDriver::start(IOService *provider)
{
    IORegistryEntry *gpio;
    IORegistryEntry *dallasGPIO;
    OSData          *tmpData  = NULL;
    UInt32          *gpioAddr = NULL;
    UInt8            bROM[8];
    UInt8            bEEPROM[32];
    UInt8            bAppReg[8];
    bool             result;
    int				 i;

    debugIOLog ("AppleDallasDriver Starting\n");
    
    result = super::start(provider);
	FailIf (FALSE == result, exit);
	result = FALSE;

    gpio = provider->getParentEntry (gIODTPlane);
    FailIf (!gpio, exit);
        
	dallasGPIO = FindEntryByProperty (gpio, "AAPL,driver-name", ".DallasDriver");
    FailIf (!dallasGPIO, exit);

    // get the hard coded memory address
    tmpData = OSDynamicCast (OSData, dallasGPIO->getProperty ("AAPL,address"));
    FailIf (!tmpData, exit);
    gpioAddr = (UInt32*)tmpData->getBytesNoCopy ();
    FailIf (!gpioAddr, exit);

    //  and convert it a virtual address
    gpioRegMem = IODeviceMemory::withRange (*gpioAddr, sizeof (UInt8));
    FailIf (!gpioRegMem, exit);

	for (i = 0; i < 8; i++)
		bROM[i] = 0;
		
	for (i = 0; i < 32; i++)
		bEEPROM[i] = 0;

	for (i = 0; i < 8; i++)
		bAppReg[i] = 0;

   // (void)readROM (bROM, bEEPROM, bAppReg);
   // ROMCheckCRC (bROM);

	registerService ();
	result = TRUE;

exit:
    return result;
}

void AppleDallasDriver::stop (IOService *provider)
{
    debugIOLog ("AppleDallasDriver Stopping\n");
    super::stop (provider);
}

//	The parameters are of the form:
//		UInt8				bROM[8];
//		UInt8				bEEPROM[32];  <--- use this for speaker id
//		UInt8				bAppReg[8];
bool AppleDallasDriver::getSpeakerID (UInt8 *bEEPROM)
{
	bool				resultSuccess;
    IOMemoryMap *		map = NULL;
    UInt8 *				gpioPtr = NULL;
	UInt8				savedGPIO;
	
    debug2IOLog("+[DALLAS] getSpeakerID( 0x%x )\n", (unsigned int)bEEPROM);

	resultSuccess = FALSE;
	
    map = gpioRegMem->map (0);
    FailIf (!map, exit);
    gpioPtr = (UInt8*)map->getVirtualAddress ();

    debug2IOLog("[DALLAS] GPIO16 Register Value = 0x%02x\n", *gpioPtr);
    FailIf (!gpioPtr, exit);

	savedGPIO = *gpioPtr;

	if ( NULL != bEEPROM ) {
 		resultSuccess = readDataROM (bEEPROM, kDallasIDAddress, sizeof ( SpkrID ) );
	}

	*gpioPtr = savedGPIO;
	OSSynchronizeIO();
	
exit:
    debug3IOLog("-[DALLAS] %d = getSpeakerID( 0x%x )\n", (unsigned int)resultSuccess, (unsigned int)bEEPROM);

	return resultSuccess;
}
