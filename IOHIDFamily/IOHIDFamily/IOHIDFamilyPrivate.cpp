/*
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

#include "IOHIDFamilyPrivate.h"

#if !TARGET_OS_EMBEDDED
#include "IOHIDSystem.h"
#endif


#define kHIDTransport1ScoreIncrement    1000
#define kHIDTransport2ScoreIncrement    2000
#define kHIDDeviceUsageScoreBase        1100
#define kHIDDeviceUsageScoreIncrement   100
#define kHIDVendor1ScoreIncrement       5000
#define kHIDVendor2ScoreIncrement       1000
#define kHIDVendor3ScoreIncrement       100

//---------------------------------------------------------------------------
// Compare the properties in the supplied table to this object's properties.
bool CompareProperty( IOService * owner, OSDictionary * matching, const char * key, SInt32 * score, SInt32 increment)
{
    // We return success if we match the key in the dictionary with the key in
    // the property table, or if the prop isn't present
    //
    OSObject 	* value;
    OSObject    * property;
    bool        matches = true;
    
    value = matching->getObject( key );

    if( value)
    {
        property = owner->copyProperty( key );
        
        if ( property )
        {
            matches = value->isEqualTo( property );
            
            if (matches && score) 
                *score += increment;
            
            property->release();
        }
        else
            matches = false;
    }

    return matches;
}

bool CompareDeviceUsage( IOService * owner, OSDictionary * matching, SInt32 * score, SInt32 increment)
{
    // We return success if we match the key in the dictionary with the key in
    // the property table, or if the prop isn't present
    //
    OSObject * 		usage;
    OSObject *		usagePage;
    OSArray *		functions;
    OSDictionary * 	pair;
    bool		matches = true;
    int			count;
    
    usage = matching->getObject( kIOHIDDeviceUsageKey );
    usagePage = matching->getObject( kIOHIDDeviceUsagePageKey );
    functions = OSDynamicCast(OSArray, owner->copyProperty( kIOHIDDeviceUsagePairsKey ));
    
    if ( functions )
    {
        if ( usagePage || usage )
        {
            count = functions->getCount();
            
            for (int i=0; i<count; i++)
            {
                if ( !(pair = (OSDictionary *)functions->getObject(i)) )
                    continue;
            
                if ( !usagePage || 
                    !(matches = usagePage->isEqualTo(pair->getObject(kIOHIDDeviceUsagePageKey))) )
                    continue;

                if ( score && !usage ) 
                {
                    *score += increment / 2;
                    break;
                }
                    
                if ( !usage || 
                    !(matches = usage->isEqualTo(pair->getObject(kIOHIDDeviceUsageKey))) )            
                    continue;
        
                if ( score ) 
                    *score += increment;
                
                break;
            }
        }
        
        functions->release();
    } else {
		matches = false;
	}
    
    return matches;
}

bool CompareDeviceUsagePairs( IOService * owner, OSDictionary * matching, SInt32 * score, SInt32 increment)
{
    // We return success if we match the key in the dictionary with the key in
    // the property table, or if the prop isn't present
    //
    OSArray *		pairArray;
    OSDictionary * 	pair;
    bool		matches = true;
    int			count;
    
    pairArray = OSDynamicCast(OSArray, matching->getObject( kIOHIDDeviceUsagePairsKey ));
    
    if (pairArray)
    {
        count = pairArray->getCount();
        
        for (int i=0; i<count; i++)
        {
            if ( !(pair = OSDynamicCast(OSDictionary,pairArray->getObject(i))) )
                continue;
        
            if ( !(matches = CompareDeviceUsage(owner, pair, score, increment)) )
                continue;

            break;
        }
    }
    
    return matches;
}

bool MatchPropertyTable(IOService * owner, OSDictionary * table, SInt32 * score)
{
    bool    match       = true;
    SInt32  pUScore     = 0;
    SInt32  pUPScore    = 0;
    SInt32  useScore    = 0;
    SInt32  trans1Score = 0;
    SInt32  trans2Score = 0;
    SInt32  ven1Score   = 0;
    SInt32  ven2Score   = 0;
    SInt32  ven3Score   = 0;

    // Compare properties.        
    if (!CompareProperty(owner, table, kIOHIDTransportKey, &trans1Score, kHIDTransport1ScoreIncrement)   ||
        !CompareProperty(owner, table, kIOHIDLocationIDKey, &trans2Score, kHIDTransport2ScoreIncrement)  ||
        !CompareProperty(owner, table, kIOHIDVendorIDKey, &ven1Score, kHIDVendor1ScoreIncrement)         ||
        !CompareProperty(owner, table, kIOHIDProductIDKey, &ven2Score, kHIDVendor2ScoreIncrement)        ||
        !CompareProperty(owner, table, kIOHIDVersionNumberKey, &ven3Score, kHIDVendor3ScoreIncrement)    ||
        !CompareProperty(owner, table, kIOHIDManufacturerKey, &ven3Score, kHIDVendor3ScoreIncrement)     ||
        !CompareProperty(owner, table, kIOHIDSerialNumberKey, &ven3Score, kHIDVendor3ScoreIncrement)     ||
        !CompareProperty(owner, table, kIOHIDPrimaryUsagePageKey, &pUPScore, kHIDDeviceUsageScoreBase)   ||
        !CompareProperty(owner, table, kIOHIDPrimaryUsageKey, &pUScore, kHIDDeviceUsageScoreIncrement)   ||
        !CompareDeviceUsagePairs(owner, table, &useScore, kHIDDeviceUsageScoreIncrement)                 ||
        !CompareDeviceUsage(owner, table, &useScore, kHIDDeviceUsageScoreIncrement)                      ||
        !CompareProperty(owner, table, "BootProtocol", score)                                            ||
		(table->getObject("HIDDefaultBehavior") && !owner->getProperty("HIDDefaultBehavior")))
    {
        if (score) 
            *score = 0;
        match = false;
    }
    else if ( score )
    {
        if ( trans1Score > 0 )
            *score += trans1Score + trans2Score;

        if ( ven1Score > 0 )
            *score += ven1Score + ven2Score + ven3Score;
            
        if ( useScore > 0 )
            *score += useScore + kHIDDeviceUsageScoreBase;
        else if ( pUPScore > 0 )
            *score += pUPScore + pUScore;
    }

    return match;
}

void IOHIDSystemActivityTickle(SInt32 nxEventType, IOService *sender)
{
#if !TARGET_OS_EMBEDDED
    IOHIDSystem *ioSys = IOHIDSystem::instance();
    if (ioSys) {
        intptr_t event = nxEventType;
        ioSys->message(kIOHIDSystemActivityTickle, sender, (void*)event);
    }
#endif
}

