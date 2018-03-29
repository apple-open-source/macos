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
#include "OSStackRetain.h"
#include "IOHIDPrivateKeys.h"
#include "IOHIDDebug.h"

#define kHIDTransport1ScoreIncrement        1000
#define kHIDTransport2ScoreIncrement        2000
#define kHIDDeviceUsageScoreBase            1100
#define kHIDDeviceUsageScoreIncrement       100
#define kHIDVendor1ScoreIncrement           5000
#define kHIDVendor2ScoreIncrement           1000
#define kHIDVendor2ArrayScoreIncrement      975
#define kHIDVendor2MaskScoreIncrement       950
#define kHIDVendor2ArrayMaskScoreIncrement  925
#define kHIDVendor3ScoreIncrement           100

//---------------------------------------------------------------------------
// Compare the properties in the supplied table to this object's properties.
bool CompareProperty( IOService * owner, OSDictionary * matching, const char * key, SInt32 * score, SInt32 increment)
{
    // We return success if we match the key in the dictionary with the key in
    // the property table, or if the prop isn't present
    //
    OSObject *  value;
    OSObject *  property;
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
    OSNumber *      usage;
    OSNumber *      usagePage;
    OSArray *       functions;
    OSObject *      obj;
    OSDictionary *  pair;
    bool            matches = true;
    int             count;
    
    usage = OSDynamicCast(OSNumber, matching->getObject( kIOHIDDeviceUsageKey ));
    usagePage = OSDynamicCast(OSNumber, matching->getObject( kIOHIDDeviceUsagePageKey ));
    obj = owner->copyProperty( kIOHIDDeviceUsagePairsKey );
    functions = OSDynamicCast(OSArray, obj);
    
    if ( functions )
    {
        if ( usagePage || usage )
        {
            count = functions->getCount();
            
            for (int i=0; i<count; i++)
            {
                if ( !(pair = OSDynamicCast(OSDictionary, functions->getObject(i))) )
                    continue;
                
                OSNumber *pairUsage;
                OSNumber *pairUsagePage;
                
                pairUsage = OSDynamicCast(OSNumber, pair->getObject(kIOHIDDeviceUsageKey));
                pairUsagePage = OSDynamicCast(OSNumber, pair->getObject(kIOHIDDeviceUsagePageKey));
            
                if ( !usagePage || !pairUsagePage ||
                    !(matches = usagePage->isEqualTo(pairUsagePage)) )
                    continue;

                if ( score && !usage ) 
                {
                    *score += increment / 2;
                    break;
                }
                    
                if ( !usage || ! pairUsage ||
                    !(matches = usage->isEqualTo(pairUsage)) )
                    continue;
        
                if ( score ) 
                    *score += increment;
                
                break;
            }
        }
        
    } else {
        matches = false;
    }
    
    OSSafeReleaseNULL(obj);
    return matches;
}

bool CompareDeviceUsagePairs( IOService * owner, OSDictionary * matching, SInt32 * score, SInt32 increment)
{
    // We return success if we match the key in the dictionary with the key in
    // the property table, or if the prop isn't present
    //
    OSArray *       pairArray;
    OSDictionary *  pair;
    bool            matches = true;
    int             count;
    
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

bool CompareProductID( IOService * owner, OSDictionary * matching, SInt32 * score)
{
    bool pidMatch = false;
    bool arrayMatch = false;
    bool maskMatch = false;
    bool maskArrayMatch = false;
    
    SInt32 pidScore = 0;
    SInt32 arrayScore = 0;
    SInt32 maskScore = 0;
    SInt32 maskArrayScore = 0;
    
    // Compare each of the types of productID matching. Then in order of highest score to least
    // see if we have any hits. Once we find one hit that matches properly then we can return
    // true after incrementing the score.
    pidMatch = CompareProperty(owner, matching, kIOHIDProductIDKey, &pidScore, kHIDVendor2ScoreIncrement);
    arrayMatch = CompareNumberPropertyArray(owner, matching, kIOHIDProductIDArrayKey, kIOHIDProductIDKey, &arrayScore, kHIDVendor2ArrayScoreIncrement);
    maskMatch = CompareNumberPropertyMask(owner, matching, kIOHIDProductIDKey, kIOHIDProductIDMaskKey, &maskScore, kHIDVendor2MaskScoreIncrement);
    maskArrayMatch = CompareNumberPropertyArrayWithMask(owner, matching, kIOHIDProductIDArrayKey, kIOHIDProductIDKey, kIOHIDProductIDMaskKey, &maskArrayScore, kHIDVendor2ArrayMaskScoreIncrement);

    if ( pidMatch && pidScore != 0 )
    {
        *score += pidScore;
        return true;
    }
    else if ( arrayMatch && arrayScore != 0 )
    {
        *score += arrayScore;
        return true;
    }
    else if ( maskMatch && maskScore != 0 )
    {
        *score += maskScore;
        return true;
    }
    else if ( maskArrayMatch && maskArrayScore != 0 )
    {
        *score += maskArrayScore;
        return true;
    }
    else
    {
        // If any of the matches explicitly failed (the property was present
        // but none of our values matched the service object, then we should
        // explicitly fail the matching. This will only return true if the
        // personality did not define any productID related keys. 
        return pidMatch && arrayMatch && maskMatch && maskArrayMatch;
    }
}


bool CompareNumberPropertyMask( IOService *owner, OSDictionary *matching, const char *key, const char *maskKey, SInt32 *score, SInt32 increment)
{
    OSNumber *    registryProperty = (OSNumber *)owner->copyProperty(key);
    OSNumber *    dictionaryProperty;
    OSNumber *    valueMask;
    CONVERT_TO_STACK_RETAIN(registryProperty);
    
    dictionaryProperty = OSDynamicCast(OSNumber, matching->getObject(key));
    valueMask = OSDynamicCast(OSNumber, matching->getObject(maskKey));
    
    // If the dicitonary or value mask doesn't exist then return true
    if ( dictionaryProperty && valueMask )
    {
        if ( OSDynamicCast(OSNumber, registryProperty) )
        {
            // If all our values are OSNumbers, then get their actual value and do the masking
            // to see if they are equal
            //
            UInt32  registryValue = registryProperty->unsigned32BitValue();
            UInt32  dictionaryValue = dictionaryProperty->unsigned32BitValue();
            UInt32  mask = valueMask->unsigned32BitValue();
            
            if ( (registryValue & mask) == (dictionaryValue & mask) )
            {
                if ( score )
                    *score += increment;
                return true;
            }
        }
    }
    else
        return true;
    
    return false;
}

bool CompareNumberPropertyArray( IOService * owner, OSDictionary * matching, const char * arrayName, const char * key, SInt32 * score, SInt32 increment)
{
    OSNumber    *registryProperty = (OSNumber *)owner->copyProperty(key);
    OSArray     *propertyArray = (OSArray *)matching->getObject(arrayName);
    CONVERT_TO_STACK_RETAIN(registryProperty);
    
    // If the property in the matching doesn't exist return true
    if ( OSDynamicCast(OSArray, propertyArray) )
    {
        if ( OSDynamicCast(OSNumber, registryProperty ) )
        {
            OSNumber *propertyFromArray;
            unsigned int i = 0;
            
            for (i = 0; i < propertyArray->getCount(); i ++ )
            {
                propertyFromArray = OSDynamicCast(OSNumber, propertyArray->getObject(i));
                if ( propertyFromArray && propertyFromArray->isEqualTo(registryProperty) )
                {
                    if ( score )
                        *score += increment;
                    return true;
                }
            }
        }
    }
    else
        return true;
    
    return false;
}

bool CompareNumberPropertyArrayWithMask( IOService * owner, OSDictionary * matching, const char * arrayName, const char * key, const char * maskKey, SInt32 * score, SInt32 increment)
{
    OSNumber    *registryProperty = (OSNumber *)owner->copyProperty(key);
    OSArray     *propertyArray = (OSArray *)matching->getObject(arrayName);
    OSNumber    *valueMask = (OSNumber *)matching->getObject(maskKey);
    CONVERT_TO_STACK_RETAIN(registryProperty);

    // If the property array or the value mask doesn't exist then return true
    if( OSDynamicCast(OSArray, propertyArray) && OSDynamicCast(OSNumber, valueMask) )
    {
        if ( OSDynamicCast(OSNumber, registryProperty) )
        {
            OSNumber *propertyFromArray;
            UInt32  registryValue = registryProperty->unsigned32BitValue();
            UInt32  mask = valueMask->unsigned32BitValue();
            
            unsigned int i = 0;
            
            for ( i = 0; i < propertyArray->getCount(); i ++ )
            {
                propertyFromArray = OSDynamicCast(OSNumber, propertyArray->getObject(i));
                if ( propertyFromArray )
                {
                    UInt32 propertyFromArrayValue = propertyFromArray->unsigned32BitValue();
                    if( (registryValue & mask) == (propertyFromArrayValue & mask ) )
                    {
                        if ( score )
                            *score += increment;
                        return true;
                        
                    }
                }
            }
        }
    }
    else
        return true;
    
    return false;
}

bool MatchPropertyTable(IOService * owner, OSDictionary * table, SInt32 * score)
{
    bool    match           = true;
    SInt32  pUScore         = 0;
    SInt32  pUPScore        = 0;
    SInt32  useScore        = 0;
    SInt32  trans1Score     = 0;
    SInt32  trans2Score     = 0;
    SInt32  ven1Score       = 0;
    SInt32  ven2Score       = 0;
    SInt32  ven3Score       = 0;
    bool    pUPMatch        = CompareProperty(owner, table, kIOHIDPrimaryUsagePageKey, &pUPScore, kHIDDeviceUsageScoreBase);
    bool    pUMatch         = CompareProperty(owner, table, kIOHIDPrimaryUsageKey, &pUScore, kHIDDeviceUsageScoreIncrement);
    bool    useMatch        = CompareDeviceUsagePairs(owner, table, &useScore, kHIDDeviceUsageScoreIncrement);
    bool    use2Match       = CompareDeviceUsage(owner, table, &useScore, kHIDDeviceUsageScoreIncrement);
    bool    trans1Match     = CompareProperty(owner, table, kIOHIDTransportKey, &trans1Score, kHIDTransport1ScoreIncrement);
    bool    trans2Match     = CompareProperty(owner, table, kIOHIDLocationIDKey, &trans2Score, kHIDTransport2ScoreIncrement);
    bool    venIDMatch      = CompareProperty(owner, table, kIOHIDVendorIDKey, &ven1Score, kHIDVendor1ScoreIncrement);
    bool    prodIDMatch     = CompareProductID(owner, table, &ven2Score);
    bool    versNumMatch    = CompareProperty(owner, table, kIOHIDVersionNumberKey, &ven3Score, kHIDVendor3ScoreIncrement);
    bool    manMatch        = CompareProperty(owner, table, kIOHIDManufacturerKey, &ven3Score, kHIDVendor3ScoreIncrement);
    bool    serialMatch     = CompareProperty(owner, table, kIOHIDSerialNumberKey, &ven3Score, kHIDVendor3ScoreIncrement);
    bool    phisicalDeviceUniqueID = CompareProperty(owner, table, kIOHIDPhysicalDeviceUniqueIDKey, &ven3Score, kHIDVendor3ScoreIncrement);
    bool    bootPMatch      = CompareProperty(owner, table, "BootProtocol", score);
    // Compare properties.
    if (!pUPMatch ||
        !pUMatch ||
        !useMatch ||
        !use2Match ||
        !trans1Match ||
        !trans2Match ||
        !venIDMatch ||
        !prodIDMatch ||
        !versNumMatch ||
        !manMatch ||
        !serialMatch ||
        !bootPMatch ||
        !phisicalDeviceUniqueID ||
        (table->getObject("HIDDefaultBehavior") && !owner->getProperty("HIDDefaultBehavior")) ||
        (table->getObject(kIOHIDCompatibilityInterface) && !owner->getProperty(kIOHIDCompatibilityInterface))
        )
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
    HIDLogInfo("HID Activity Tickle (type:%d sender:%llx)", nxEventType, sender ? sender->getRegistryEntryID() : 0);
    IOHIDSystem *ioSys = IOHIDSystem::instance();
    if (ioSys) {
        intptr_t event = nxEventType;
        ioSys->message(kIOHIDSystemActivityTickle, sender, (void*)event);
    }
#else
    (void)nxEventType;
    (void)sender;
#endif
}

extern "C" int  kern_stack_snapshot_with_reason(char *reason);
extern "C" kern_return_t sysdiagnose_notify_user(uint32_t keycode);

void handle_stackshot_keychord(uint32_t keycode)
{
    kern_stack_snapshot_with_reason((char *)"Stackshot triggered using keycombo");
    sysdiagnose_notify_user(keycode);
    HIDLog("IOHIDSystem posted stackshot event 0x%08x", keycode);
}

