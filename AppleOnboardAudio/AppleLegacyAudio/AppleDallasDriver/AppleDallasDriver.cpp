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

/*================================================================================
	The bus master (this driver) asserts a reset pulse.  The target device
	responds with a "Presence" pulse prior to expiration of tPDH max.

			                   >|         tRSTH              |<
	Vpullup	_____                 _______              _______
	Vih MIN	     \               /       \            /       \
			      \             /         \          /         \
	Vil MAX	       \           /           \        /           \
	0v		        \_________/             \______/             \
			                    >| tPDH  |<
			                 >|tR|<      |
			       >|  tRSTL  |<        >|  tPDL  |<

	Where:	15 µS ² tPDH < 60 µS
			60 µS ² tPDL < 240 µS
			480 µS ² tRSTH
			480 µS ² tRSTL < °
			tRSTL + tR + tRSTH < 960 µS
*/
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
	
	outputZero      = (*gpioPtr & ( dualEdge << intEdgeSEL )) | ( gpioDDR_OUTPUT << gpioDDR );	// set output select0, altoe=0, ddir=1, output value = 0
																								// save bit 7
	outputTristate  = (*gpioPtr & ( dualEdge << intEdgeSEL )) | ( gpioDDR_INPUT << gpioDDR );	// output bit = 0, but output tristated to get a 1

    res = TRUE;
    // Make sure the ROM is present.  The ROM's one-wire-bus is pulled low if
	//	no rom is present through switching of the speaker jack connector and
	//	pulled high if the ROM is present.  The ROM itself will not pull the
	//	bus low unless in response to a reset or data query.
    *gpioPtr = outputTristate; OSSynchronizeIO();
    if ( ( *gpioPtr & ( 1 << gpioPIN_RO ) ) == 0 ) {
        IOSleep(1);
        FailWithAction ((*gpioPtr & ( 1 << gpioPIN_RO )) == 0, debugIOLog (3, "AppleDallasDriver::ROMReset No speaker ROM detected"), exit);
    }

    correctTiming = FALSE;
    while (!correctTiming)
    {
        debugIOLog (3, "[DALLAS] ROM Reset");
        correctTiming = TRUE;
        
        clock_get_uptime(&start);
        *gpioPtr = outputZero; OSSynchronizeIO();
        IODelay(kROMResetPulseMin);
        *gpioPtr = outputTristate; OSSynchronizeIO();
        
        ns = kROMResetPulseMax * kNANOSECONDS_PER_MICROSECOND;
        nanoseconds_to_absolutetime(ns, &finish);
        ADD_ABSOLUTETIME(&finish, &start);
        
		// Wait for rising edge of reset pulse or HIGH level for quiescent bus state
        bitEdge = FALSE;  
        while (!bitEdge && correctTiming)
        {
            bit = *gpioPtr & ( 1 << gpioPIN_RO );
            clock_get_uptime(&now);
            
            if (CMP_ABSOLUTETIME(&now, &finish) > 0) {
                correctTiming = FALSE;
                debugIOLog (3, "[DALLAS] ROM Reset: Failed Reset Rising Edge");
            } else if (bit) {
                bitEdge = TRUE;
            } else {
                IODelay(1);
            }
        }
        
		//	BEGIN the RESET PULSE
        ns = kROMPresenceDelayMax * kNANOSECONDS_PER_MICROSECOND;
        nanoseconds_to_absolutetime(ns, &finish);
        ADD_ABSOLUTETIME(&finish, &now);
        
        bitEdge = FALSE;  // Wait for falling edge of presence pulse
        while (!bitEdge && correctTiming)
        {
            bit = *gpioPtr & ( 1 << gpioPIN_RO );
            clock_get_uptime(&now);
            
            if (CMP_ABSOLUTETIME(&now, &finish)>0) {
                correctTiming = FALSE;
                IOSleep(1);
				debugIOLog (3, "AppleDallasDriver::ROMReset  Failed to detect Presence Pulse");
            } else if (!bit) {
                bitEdge = TRUE;
            } else {
                IODelay(1);
            }
        }
        
		//	BEGIN detection of the PRESENCE PULSE
        ns = kROMPresencePulseMax * kNANOSECONDS_PER_MICROSECOND;
        nanoseconds_to_absolutetime(ns, &finish);
        ADD_ABSOLUTETIME(&finish, &now);
        
        bitEdge = FALSE;  // Wait for rising edge of presence pulse
        while (!bitEdge && correctTiming)
        {
            bit = *gpioPtr & ( 1 << gpioPIN_RO );
            clock_get_uptime(&now);
            
            if (CMP_ABSOLUTETIME(&now, &finish)>0) {
                correctTiming = FALSE;
//                debugIOLog (3, "[DALLAS] ROM Reset: Presence Pulse too long");
            } else if (bit) {
                bitEdge = TRUE;
            } else {
                IODelay(1);
            }
        }
        
        if (!correctTiming)
        {
            FailWithAction(++errorCount > 10, debugIOLog (3, "[DALLAS] ROM Reset: Too many failures, bailing out"), exit);
        }
        IOSleep(1);  // Give up the cpu after all attempts
    }

    res = FALSE;
    
exit:
    return res;
}

/*================================================================================
	Write-1 Time Slot:
	Vpullup	_____                 ____________________________
	Vih MIN	     \               /                            \
			      \             /                              \
	Vil MAX	       \           /                                \
	0v		        \_________/                                  \

			       >|   tLOW1    |<
			       >|           tLOW0            |<  >| tREC |<
			       >|            tSLOT                |<

	Where:	1 µS ² tLOW1 < 15 µS
			60 µS ² tSLOT ² 120 µS
			1 µS ² tREC < °

	Write-0 Time Slot:
	Vpullup	_____                                      _______
	Vih MIN	     \                                    /       \
			      \                                  /         \
	Vil MAX	       \                                /           \
	0v		        \______________________________/             \

			       >|   tLOW1    |<
			       >|           tLOW0            |<  >| tREC |<
			       >|            tSLOT                |<

	Where:	60 µS ² tLOW0 < 120 µS
			60 µS ² tSLOT ² 120 µS
			1 µS ² tREC < °

	Data is transacted lsb first.  Returns TRUE if failed!

	Althought the tLOW1 is specified as 1µS minimum, it is unlikely that our hardware
	will propagate such a short duration pulse so the pulse is done at half the maximum.
	EMI filtering on the speaker jack can impact the timing such that an RC charge curve
	occurs as the signal is driven low and released high.  This curve causes the actual
	time period that the signal is below the minimum input voltage (i.e. Vil MAX) to be
	reduced and may result in timing less than the minimum allowed for the one-wire-bus
	protocol.  Using longer timing, which does not violate the maximum timing, allows 
	the charge curve to progress deeper toward the desired bus state and will result
	in timing that does not violate the minimum timing for the signal assertion.  This
	rule has been applied throughout this driver.  rbm 16 Oct 2002	[3053696]
*/
bool ROMSendByte ( UInt8 *gpioPtr, UInt8 theByte, UInt8 msgRefCon )
{
    AbsoluteTime	start, now, finish, tLOW;
    UInt64			ns;
    UInt8			bit;
	UInt8			outputZero, outputTristate;
    bool			correctTiming;
    bool			bitEdge;
    bool			res;
    int				bitIndex;
    
    debugIOLog (3, "[DALLAS] ROM Send 0x%02X", theByte);
	
	outputZero      = (*gpioPtr & ( dualEdge << intEdgeSEL )) | ( gpioDDR_OUTPUT << gpioDDR );	// set output select0, altoe=0, ddir=1, output value = 0
																								// save bit 7
	outputTristate  = (*gpioPtr & ( dualEdge << intEdgeSEL )) | ( gpioDDR_INPUT << gpioDDR );	// output bit = 0, but output tristated to get a 1

    res = FALSE;		// assume success
    for ( bitIndex = 0; bitIndex < kBITS_PER_BYTE; bitIndex++)
    {
		clock_get_uptime(&start);
		ns = kTSLOT_maximum * kNANOSECONDS_PER_MICROSECOND;				//	tSLOT 120 µS limit
		nanoseconds_to_absolutetime(ns, &finish);
		ADD_ABSOLUTETIME(&finish, &start);

        correctTiming = TRUE;
        if ( theByte & ( 1 << bitIndex ) ) {							// bit=1
            ns = kTLOW1_maximum * kNANOSECONDS_PER_MICROSECOND;			//	tLOW1 15 µS limit
            nanoseconds_to_absolutetime(ns, &tLOW);
            ADD_ABSOLUTETIME(&tLOW, &start);

            *gpioPtr = outputZero; OSSynchronizeIO();					//	assert to '0' bus level
            IODelay( kTLOW1_maximum / 2 );								//	1 µS ² tLOW1 < 15 µS	rbm 16 Oct 2002	[3053696]
            *gpioPtr = outputTristate; OSSynchronizeIO();				//	release to '1' bus level
                        
            bitEdge = FALSE;  // Wait for rising edge of bit
            while (!bitEdge && correctTiming)
            {
                bit = *gpioPtr & ( 1 << gpioPIN_RO );
                clock_get_uptime(&now);
                
				//	Check to see that bus released within tLOW1 maximum limit of 15 µS
                if ( CMP_ABSOLUTETIME ( &now, &tLOW ) > 0 ) {
                    correctTiming = FALSE;
                    debugIOLog (3,  "... ROM Send Byte: theByte %X, Failed Bit %d Rising Edge, msgRefCon %d", 
						(unsigned int)theByte, 
						(unsigned int)bitIndex, 
						(unsigned int)msgRefCon );
                } else if (bit) {
                    bitEdge = TRUE;
                } else {
                    IODelay(1);
                }
            }
        } else {														// bit = 0
            ns = kTLOW0_maximum * kNANOSECONDS_PER_MICROSECOND;			//	60 µS ² tLOW0 ² tSLOT ² 120 µS
            nanoseconds_to_absolutetime(ns, &tLOW);
            ADD_ABSOLUTETIME(&tLOW, &start);
            
            *gpioPtr = outputZero; OSSynchronizeIO();					//	assert to '0' bus level
            IODelay( ( kTLOW0_minimum + kTLOW0_maximum ) / 2 );			//	use 90 µS	rbm 16 Oct 2002	[3053696]
            *gpioPtr = outputTristate; OSSynchronizeIO();				//	release to '1' bus level
            
            bitEdge = FALSE;  // Wait for rising edge of bit
            while (!bitEdge && correctTiming)
            {
                bit = *gpioPtr & ( 1 << gpioPIN_RO );
                clock_get_uptime(&now);
                
                if (CMP_ABSOLUTETIME(&now, &tLOW)>0) {
                    correctTiming = FALSE;
                    debugIOLog (3,  "... ROM Send Byte: theByte %X, Failed Bit %d Rising Edge, msgRefCon %d", 
						(unsigned int)theByte, 
						(unsigned int)bitIndex, 
						(unsigned int)msgRefCon );
                } else if (bit) {
                    bitEdge = TRUE;
                } else {
                    IODelay(1);
                }
            }
        }

		IODelay ( kTSLOT_minimum + kTREC );									//	make sure remaining tSLOT + tREC is greater than 611 µS
        
        if (!correctTiming)
            res = TRUE;
	}

	IOSleep(1);					// Give up CPU every byte
	if ( res ) { debugIOLog (3,  "ROMSendByte FAILED!" ); }
    return res;
}

/*================================================================================
	Read Data Time Slot:
	Vpullup	_____                 ____________________________
	Vih MIN	     \               /                    /       \
			      \             /                    /         \
	Vil MAX	       \           /                    /           \
	0v		        \_________/___________________ /             \

			       >|            tSLOT                |<
			       >|   tLOWR    |<    >| tRELEASE |<
			       >|        tRDV       |<           >| tREC |<

	Where:	1 µS ² tLOWR < 15 µS
			60 µS ² tSLOT ² 120 µS
			1 µS ² tRELEASE ² 45 µS
			15 µS = tRDV
			1 µS ² tREC < °

	Data is transacted lsb first.
*/
bool ROMReadByte (UInt8 *gpioPtr, UInt8* theByte)
{
    AbsoluteTime	start, finish, tRdv, after;
    UInt64			ns;
    UInt8			bit, failedTRdv, failedTRelease, outputZero, outputTristate, theValue = 0;
    bool			correctTiming, bitEdge, res;
    int				bitIndex;
	
	outputZero      = (*gpioPtr & ( dualEdge << intEdgeSEL )) | ( gpioDDR_OUTPUT << gpioDDR );	// set output select0, altoe=0, ddir=1, output value = 0
																								// save bit 7
	outputTristate  = (*gpioPtr & ( dualEdge << intEdgeSEL )) | ( gpioDDR_INPUT << gpioDDR );	// output bit = 0, but output tristated to get a 1

    res = FALSE;		// assume success
	failedTRelease = failedTRdv = 0;
    for ( bitIndex = 0; bitIndex < kBITS_PER_BYTE; bitIndex++)
    {
        correctTiming = TRUE;
        
		//	There also was a timing violation in the ROMReadByte method where 1 µS < tSU was used to 
		//	assert the bus to a low state at the start of reading a bit cell.  The tSU timing parameter 
		//	actualy refers to how fast the device asserts the bus after sensing a low on the bus and 
		//	does not refer to the timing that the master should continue to assert the bus to a low state. 
		//	The bus master should continue to drive the bus low up to tLOWR maximum or < 15 µS.  The 
		//	same concerns about RC charge currents applies here so I've lengthened the timing to half 
		//	the maximum or 7.5 µS.  Care must be taken to make sure that tROMSlot timing is not measured
		//	from any signal element other than the initial falling edge (i.e. '0' bus level state) to
		//	avoid accumulating timing error over multiple bit cells.		rbm 16 Oct 2002	[3053696]
		clock_get_uptime ( &start );
        ns = kTSLOT_maximum * kNANOSECONDS_PER_MICROSECOND;			//	tSlot begins at start of bit cell	rbm 16 Oct 2002	[3053696]
        nanoseconds_to_absolutetime ( ns, &finish );
        ADD_ABSOLUTETIME(&finish, &start);
		ns = ( kTRDV + 1 ) * kNANOSECONDS_PER_MICROSECOND;
        nanoseconds_to_absolutetime ( ns, &tRdv );
        ADD_ABSOLUTETIME(&tRdv, &start);							//	masterSampleWindow indicates tRDV relative to start of bit cell		
		
        *gpioPtr = outputZero; OSSynchronizeIO();					//	assert to '0' bus level starts the bit cell
        IODelay( kTLOWR_maximum / 3 );								//	1 µS ² tLOWR < 15 µS	rbm 16 Oct 2002	[3053696]
        *gpioPtr = outputTristate; OSSynchronizeIO();				//	release to '1' bus level
        
		//	Once the bus has been released from the '0' state at the beginning of the bit cell, the
		//	device will have asserted asserted a '0' or released to a '1' in parallel with the master
		//	'0' assertion on the bus and within 1 µS of the master asserting '0'.  The data is then
		//	read between tLOWR maximum and tRDV (i.e. MASTER SAMPLING WINDOW).  The tRDV indicates
		//	the point where the data is to be sampled (see Dallas application note 126: ReadX function).
		//	rbm	16 Oct 2002	[3053696]
		
		IODelay ( kTRDV - ( kTLOWR_maximum / 3 ) - 1 );
		bit = *gpioPtr & ( 1 << gpioPIN_RO );
		clock_get_uptime(&after);
		if ( CMP_ABSOLUTETIME ( &after, &tRdv ) > 0 ) {
			correctTiming = FALSE;
			res = TRUE;
			failedTRdv |= ( 1 << bitIndex );
		}
		
		//	Now wait for release (necessary when the bit cell is a '0'.  This must occur within 45 µS
		//	of tRDV (i.e. 0 µS ² tRELEASE < 45 µS).
		if ( 0 == bit ) {
			bitEdge = FALSE;
			correctTiming = TRUE;
			while ( !bitEdge && correctTiming ) {
				if ( *gpioPtr & ( 1 << gpioPIN_RO ) ) {
					bitEdge = TRUE;
				}
				clock_get_uptime(&after);
				if ( CMP_ABSOLUTETIME ( &after, &finish ) > 0 ) {
					correctTiming = FALSE;
					res = TRUE;
					failedTRelease |= ( 1 << bitIndex );
				}
			}
		}
		IODelay( kTSLOT_minimum + kTREC );
		
		//	'bit' now contains the bit sample state within acceptable timing margins if 'correctTiming' is TRUE.
                
        theValue |= ( bit == 0 ? 0 << bitIndex : 1 << bitIndex );
    }
	if ( res ) {
		debugIOLog (3, "[DALLAS] ROMReadByte tRDV failed on bits %X, tRELEASE failed on bits %X, data %X", (unsigned int)failedTRdv, (unsigned int)failedTRelease, (unsigned int)theValue );
	}
	
    IOSleep(1);														// give up CPU every byte
	*theByte = theValue;
    debugIOLog (3, "[DALLAS]   ROM Read 0x%02x", *theByte);
	if ( res ) { debugIOLog (3,  "ROMReadByte FAILED!" ); }
    return res;
}

//================================================================================
void ROMCheckCRC(UInt8 *bROM)
{
    int index, j;
    UInt8 crc[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    UInt8 currByte, newBit;
    UInt8 finalCRC;
    
    for (index=0; index<7; index++) {
        currByte = bROM[index];
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
    for (index=0; index<8; index++) {
        finalCRC = (finalCRC << 1) | crc[index];
    }
    
    if (finalCRC != bROM[7]) {
        debugIOLog (3, "[DALLAS] CRC Mismatch! 0x%02x 0x%02x", finalCRC, bROM[7]);
    } else {
        debugIOLog (3, "[DALLAS] ROM CRC Match: 0x%02x", finalCRC);
    }
    
}

//================================================================================
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

//================================================================================
bool AppleDallasDriver::init (OSDictionary *dict)
{
    bool res = super::init (dict);
    debugIOLog (3, "[DALLAS] AppleDallasDriver Initializing");
    return res;
}

//================================================================================
void AppleDallasDriver::free (void)
{
    debugIOLog (3, "AppleDallasDriver Freeing");
    CLEAN_RELEASE (gpioRegMem)
    super::free ();
}

//================================================================================
IOService *AppleDallasDriver::probe (IOService *provider, SInt32 *score)
{
    
    IOService *res = super::probe (provider, score);
    debugIOLog (3, "[DALLAS] AppleDallasDriver Probing");
    return res;
}

//================================================================================
bool AppleDallasDriver::readApplicationRegister (UInt8 *bAppReg)
{
    IOMemoryMap	*map = NULL;
    UInt8		*gpioPtr = NULL;
    int			index;
    bool		failure;
    bool		resultSuccess;
	int			retryCount;
    
    resultSuccess = FALSE;
    map = gpioRegMem->map (0);
    FailIf (!map, exit);
    gpioPtr = (UInt8*)map->getVirtualAddress ();
    debugIOLog (3, "[DALLAS] GPIO16 Register Value = 0x%02x", *gpioPtr);

    FailIf (!gpioPtr, exit);
    
    // Look for ROM present
    FailWithAction(ROMReset(gpioPtr), debugIOLog (3, "No speaker ROM detected"), exit);
    
    // Read 64b Application Register
    failure = TRUE;
    debugIOLog (3, "[DALLAS] Reading 64b Application Register");
	retryCount = kRetryCountSeed;
    for (index=0; index<8; index++) {
        do {
            while (failure && retryCount) {
                ROMReset(gpioPtr);
                failure = ROMSendByte(gpioPtr, kROMSkipROM, 0 );
                failure = failure || ROMSendByte(gpioPtr, kROMReadAppReg, 1 );
                failure = failure || ROMSendByte(gpioPtr, index, 2 );
				retryCount--;
            }
            failure = ROMReadByte(gpioPtr, &bAppReg[index]);
        } while (failure && retryCount);
    }
    if ( !failure ) {
		resultSuccess = TRUE;
	}
	
exit:
    return resultSuccess;
}

//=========================================================================================
//	Dallas 2430A ROM EEPROM read transaction:
//
//	BUS PHASE			VALUE	ACTION					DESCRIPTION
//	--------------		----	---------------------	-----------------------------------
//	RESET				...		...						...
//	PRESENCE PULSE		...		...						...
//	SEND BYTE			0xCC	SKIP ROM				Bypass lasered ROM
//	SEND BYTE			0xF0	READ MEMORY				Copy EEPROM to Scratchpad (Note 1)
//	RESET				...		...						...
//	PRESENCE PULSE		...		...						...
//	SEND BYTE			0xCC	SKIP ROM				Bypass lasered ROM
//	SEND BYTE			0xAA	READ SCRATCHPAD			Copy Scratchpad to CPU
//	SEND BYTE			0x00	SCRATCHPAD ADDRESS		Beginning address modulus 32
//	READ BYTE			0x__	Read 'deviceFamily'		speaker ID field
//	READ BYTE			0x__	Read 'deviceType'		speaker ID field
//	READ BYTE			0x__	Read 'deviceSubtype'	speaker ID field	(Note 2)
//	READ BYTE			0x__	Read 'deviceReserved'	speaker ID field	(Note 2)
//	
//	Note 1:		No address is conveyed during the READ MEMORY command which results
//				in copying the entire EEPROM to the scratchpad memory.
//	Note 2:		Only the 'deviceFamily' and 'deviceType' fields are used.  The
//				'deviceSubtype' and 'deviceReserved' fields have not been defined
//				for any shipping product and are intended for future expansion only.
//				Reading 'deviceSubtype' and 'deviceReserved' fields is optional.
//
//	Restructured the AppleDallasDriver::readDataROM method to implement a state machine with a 
//	conditional compile flag that allows the retry method to be implemented with more flexibility  
//	in recovery options.  The Dallas ROM is segmented into four sections which consist of a  
//	lasered ROM (i.e. serial number), application register, scratchpad memory and EEPROM.  The  
//	speaker data is held in the EEPROM but cannot be accessed directly.  The driver must transfer  
//	the data from the EEPROM to the scratchpad memory and then read the scratchpad memory.  This  
//	takes two separate transactions.  The state machine was implemented to avoid increasing nesting  
//	of conditional execution statements in providing for recovery mechanisms that allow failed data  
//	values from the second transaction to provide for recovery by initiating a retry at the transfer  
//	of the EEPROM data to scratchpad memory.  If the scratchpad memory had incorrect data then  
//	retrying the scratchpad transaction would only result in fetching incorrect data on each retry.
//	rbm 16 Oct 2002	[3053696]
bool AppleDallasDriver::readDataROM (UInt8 *bEEPROM,int dallasAddress, int size)
{
    IOMemoryMap     *map = NULL;
    UInt8           tempData, *gpioPtr = NULL;
    int				index;
    bool			resultSuccess;
	int				retryCount, stateMachine;
    
	debugIOLog (3,  "+ AppleDallasDriver::readDataROM ( %X, %X, %X )", (unsigned int)bEEPROM, (unsigned int)dallasAddress, (unsigned int)size );
    resultSuccess = FALSE;			//	assume that it failed
	
	FailIf ( NULL == bEEPROM, exit );
	((DallasIDPtr)bEEPROM)->deviceFamily = kDeviceFamilyUndefined;
	((DallasIDPtr)bEEPROM)->deviceType = kDallasSpeakerRESERVED;
	((DallasIDPtr)bEEPROM)->deviceSubType = 0;
	((DallasIDPtr)bEEPROM)->deviceReserved = 0;

    map = gpioRegMem->map (0);
    FailIf (!map, exit);
    gpioPtr = (UInt8*)map->getVirtualAddress ();
    debugIOLog (3,  "[DALLAS] GPIO16 Register Value = 0x%02x", *gpioPtr );
    FailIf (!gpioPtr, exit);
	
	stateMachine = kSTATE_RESET_READ_MEMORY;
	retryCount = kRetryCountSeed;
	index = 0;
	while( ( kSTATE_COMPLETED != stateMachine ) && ( 0 != retryCount ) ) {
		switch ( stateMachine ) {
			case kSTATE_RESET_READ_MEMORY:
				if ( ROMReset ( gpioPtr ) ) {
					debugIOLog (3,  "... failed at kSTATE_RESET_READ_MEMORY %d, retryCount %d", (unsigned int)stateMachine, (unsigned int)retryCount );
					stateMachine = kSTATE_RESET_READ_MEMORY;
					if ( retryCount ) { retryCount--; }
				} else {
					stateMachine = kSTATE_CMD_SKIPROM_READ_MEMORY;
				}
				break;
			case kSTATE_CMD_SKIPROM_READ_MEMORY:
				if ( ROMSendByte ( gpioPtr, kROMSkipROM, stateMachine ) ) {
					debugIOLog (3,  "... failed at kSTATE_CMD_SKIPROM_READ_MEMORY %d, retryCount %d", (unsigned int)stateMachine, (unsigned int)retryCount );
					stateMachine = kSTATE_RESET_READ_MEMORY;
					if ( retryCount ) { retryCount--; }
				} else {
					stateMachine = kSTATE_CMD_READ_MEMORY;
				}
				break;
			case kSTATE_CMD_READ_MEMORY:
				if ( ROMSendByte ( gpioPtr, kROMReadMemory, stateMachine ) ) {
					debugIOLog (3,  "... failed at kSTATE_CMD_READ_MEMORY %d, retryCount %d", (unsigned int)stateMachine, (unsigned int)retryCount );
					stateMachine = kSTATE_RESET_READ_MEMORY;
					if ( retryCount ) { retryCount--; }
				} else {
					if ( 0 != dallasAddress ) {
						stateMachine = kSTATE_READ_MEMORY_ADDRESS;
					} else {
						stateMachine = kUSE_DESCRETE_BYTE_TRANSFER ? kSTATE_READ_MEMORY_ADDRESS : kSTATE_RESET_READ_SCRATCHPAD;
					}
				}
				break;
			case kSTATE_READ_MEMORY_ADDRESS:
				if ( ROMSendByte ( gpioPtr, dallasAddress, stateMachine ) ) {					//	ROM address is 0x00
					debugIOLog (3,  "... failed at kSTATE_READ_MEMORY_ADDRESS %d, retryCount %d", (unsigned int)stateMachine, (unsigned int)retryCount );
					stateMachine = kSTATE_RESET_READ_MEMORY;
					if ( retryCount ) { retryCount--; }
				} else {
					stateMachine = kSTATE_READ_MEMORY_DATA;
					index = 0;
				}
				break;
			case kSTATE_READ_MEMORY_DATA:
				if ( ROMReadByte ( gpioPtr, &tempData ) ) {
					debugIOLog (3,  "... failed at kSTATE_READ_MEMORY_DATA %d, retryCount %d, byte %d", (unsigned int)stateMachine, (unsigned int)retryCount, (unsigned int)index );
					stateMachine = kSTATE_RESET_READ_MEMORY;
					if ( retryCount ) { retryCount--; }
				} else {
					index++;
					if ( index == size ) { stateMachine = kSTATE_RESET_READ_SCRATCHPAD; }
				}
				break;
			case kSTATE_RESET_READ_SCRATCHPAD:
				if ( ROMReset ( gpioPtr ) ) {
					debugIOLog (3,  "... failed at kSTATE_RESET_READ_SCRATCHPAD %d, retryCount %d", (unsigned int)stateMachine, (unsigned int)retryCount );
					stateMachine = kSCRATCHPAD_RETRY_STATE;
					if ( retryCount ) { retryCount--; }
				} else {
					stateMachine = kSTATE_CMD_SKIPROM_SCRATCHPAD;
				}
				break;
			case kSTATE_CMD_SKIPROM_SCRATCHPAD:
				if ( ROMSendByte ( gpioPtr, kROMSkipROM, stateMachine ) ) {
					debugIOLog (3,  "... failed at kSTATE_CMD_SKIPROM_SCRATCHPAD %d, retryCount %d", (unsigned int)stateMachine, (unsigned int)retryCount );
					stateMachine = kSCRATCHPAD_RETRY_STATE;
					if ( retryCount ) { retryCount--; }
				} else {
					stateMachine = kSTATE_CMD_SCRATCHPAD;
				}
				break;
			case kSTATE_CMD_SCRATCHPAD:
				if ( ROMSendByte ( gpioPtr, kROMReadScratch, stateMachine ) ) {
					debugIOLog (3,  "... failed at kSTATE_CMD_SCRATCHPAD %d, retryCount %d", (unsigned int)stateMachine, (unsigned int)retryCount );
					stateMachine = kSCRATCHPAD_RETRY_STATE;
					if ( retryCount ) { retryCount--; }
				} else {
					stateMachine = kSTATE_SCRATCHPAD_ADDRESS;
				}
				break;
			case kSTATE_SCRATCHPAD_ADDRESS:
				if ( ROMSendByte ( gpioPtr, dallasAddress, stateMachine ) ) {		//	ROM address is 0x00
					debugIOLog (3,  "... failed at kSTATE_SCRATCHPAD_ADDRESS %d, retryCount %d", (unsigned int)stateMachine, (unsigned int)retryCount );
					stateMachine = kSCRATCHPAD_RETRY_STATE;
					if ( retryCount ) { retryCount--; }
				} else {
					stateMachine = kSTATE_READ_SCRATCHPAD;
					index = 0;
				}
				break;
			case kSTATE_READ_SCRATCHPAD:
				if ( ROMReadByte ( gpioPtr, &bEEPROM[index] ) ) {
					debugIOLog (3,  "... failed at kSTATE_READ_SCRATCHPAD %d, retryCount %d, byte %d", (unsigned int)stateMachine, (unsigned int)retryCount, (unsigned int)index );
					stateMachine = kSCRATCHPAD_RETRY_STATE;							//	timing failed so retry at scratchpad transaction
					if ( retryCount ) { retryCount--; }
				} else {
					switch ( index ) {
						case 0:
							if ( kDeviceFamilySpeaker == ((DallasIDPtr)bEEPROM)->deviceFamily ) {
								index++;
								if ( index >= size ) { stateMachine = kSTATE_COMPLETED; }
							} else {
								debugIOLog (3,  "... failed with bad deviceFamily data %X, resetting state machine", ((DallasIDPtr)bEEPROM)->deviceFamily );
								stateMachine = kSTATE_RESET_READ_MEMORY;			//	bad data then retry at copy of EEPROM to scratchpad
							}
							break;
						case 1:
							if ( 0xFF != ((DallasIDPtr)bEEPROM)->deviceSubType ) {
								index++;
								if ( index >= size ) { stateMachine = kSTATE_COMPLETED; }
							} else {
								debugIOLog (3,  "... failed with bad deviceSubType data %X, resetting state machine", ((DallasIDPtr)bEEPROM)->deviceSubType );
								stateMachine = kSTATE_RESET_READ_MEMORY;			//	bad data then retry at copy of EEPROM to scratchpad
							}
							break;
						default:
							index++;
							if ( index >= size ) { stateMachine = kSTATE_COMPLETED; }
							break;
					}
				}
				break;
			case kSTATE_COMPLETED:
				break;
		}
		if ( retryCount && ( kSTATE_RESET_READ_MEMORY == stateMachine ) ) {
			IOSleep ( 1 );
		}
	}
	if ( kSTATE_COMPLETED == stateMachine ) { resultSuccess = TRUE; }
	
exit:
	debugIOLog (3,  "- AppleDallasDriver::readDataROM ( %X, %X, %X ) returns %X", (unsigned int)bEEPROM, (unsigned int)dallasAddress, (unsigned int)size, (unsigned int)resultSuccess );
    debugIOLog (3, "[DALLAS] readDataROM returns %dx", (unsigned int)resultSuccess);
    return resultSuccess;
}

//================================================================================
bool AppleDallasDriver::readSerialNumberROM (UInt8 *bROM)
{
    IOMemoryMap     *map = NULL;
    UInt8           *gpioPtr = NULL;
    int              index;
    bool             failure;
    bool             resultSuccess;
	int				retryCount;
    
    resultSuccess = FALSE;
    map = gpioRegMem->map (0);
    FailIf (!map, exit);
    gpioPtr = (UInt8*)map->getVirtualAddress ();
    debugIOLog (3, "[DALLAS] GPIO16 Register Value = 0x%02x", *gpioPtr);
    FailIf (!gpioPtr, exit);
    
    // Look for ROM present
    FailWithAction(ROMReset(gpioPtr), debugIOLog (3, "[DALLAS] No speaker ROM detected"), exit);
    
    // Read 64b ROM
    failure = TRUE;
    debugIOLog (3, "[DALLAS] Reading 64b ROM");
	retryCount = kRetryCountSeed;
    while (failure && retryCount) {
        while (failure && retryCount) {
            ROMReset(gpioPtr);
            failure = ROMSendByte( gpioPtr, kROMReadROM, 0 );
			retryCount--;
        }

        for ( index = 0; index < 8 && !failure; index++ ) {
            failure |= ROMReadByte ( gpioPtr, &bROM[index] );
		}
    }
    if ( !failure ) {
		resultSuccess = TRUE;
	}
exit:
    return resultSuccess;
}

//================================================================================
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
    int				 index;

    debugIOLog (3, "AppleDallasDriver Starting");
    
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

	for (index = 0; index < 8; index++)
		bROM[index] = 0;
		
	for (index = 0; index < 32; index++)
		bEEPROM[index] = 0;

	for (index = 0; index < 8; index++)
		bAppReg[index] = 0;

   // (void)readROM (bROM, bEEPROM, bAppReg);
   // ROMCheckCRC (bROM);

	registerService ();
	result = TRUE;

exit:
    return result;
}

//================================================================================
void AppleDallasDriver::stop (IOService *provider)
{
    debugIOLog (3, "AppleDallasDriver Stopping");
    super::stop (provider);
}

//================================================================================
//	The parameters are of the form:
//		UInt8				bROM[8];
//		UInt8				bEEPROM[32];  <--- use this for speaker id
//		UInt8				bAppReg[8];
//	NOTE:	There is no CRC within the EEPROM.  Only the lasered ROM has CRC!!!
//			This function only accesses the EEPROM.
bool AppleDallasDriver::getSpeakerID (UInt8 *bEEPROM)
{
	bool				resultSuccess;
    IOMemoryMap *		map = NULL;
    UInt8 *				gpioPtr = NULL;
	UInt8				savedGPIO;
	
	debugIOLog (3,  "+ AppleDallasDriver::getSpeakerID ( %X )", (unsigned int)bEEPROM );

	resultSuccess = FALSE;
	
    map = gpioRegMem->map (0);
    FailIf (!map, exit);
    gpioPtr = (UInt8*)map->getVirtualAddress ();

    debugIOLog (3, "... GPIO16 Register Value = 0x%02x", *gpioPtr);
    FailIf (!gpioPtr, exit);

	savedGPIO = *gpioPtr;

	if ( NULL != bEEPROM ) {
		debugIOLog (3,  "... About to readDataROM ( %X, %X, %X )", (unsigned int)bEEPROM, (unsigned int)kDallasIDAddress, (unsigned int)sizeof ( SpkrID ) );
 		resultSuccess = readDataROM (bEEPROM, kDallasIDAddress, sizeof ( SpkrID ) );
	}

	*gpioPtr = savedGPIO;
	OSSynchronizeIO();
	
exit:
	debugIOLog (3,  "- AppleDallasDriver::getSpeakerID ( %X ) returns %X", (unsigned int)bEEPROM, (unsigned int)resultSuccess );

	return resultSuccess;
}
