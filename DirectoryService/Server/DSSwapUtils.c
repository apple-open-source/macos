/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

/*!
 * @header DSSwapUtils
 * Provides routines to byte swap DSProxy buffers.
 */
 
#ifndef __BIG_ENDIAN__

#include "DSSwapUtils.h"
#include <string.h>
#include <errno.h>			// system call error numbers
#include <unistd.h>			// for select call 
#include <stdlib.h>			// for calloc()
#include <stdio.h>
#include <ctype.h>
#include <syslog.h>
#include <Security/Authorization.h>
#include "CSharedData.h"
#include "SharedConsts.h"

UInt32 DSGetLong(void* ptr, dsBool inToBig)
{
	UInt32 returnVal = *(UInt32*)(ptr);
	if (!inToBig)
		returnVal = NXSwapBigIntToHost(returnVal);
		
	return returnVal;
}

unsigned short DSGetShort(void* ptr, dsBool inToBig)
{
	unsigned short returnVal = *(unsigned short*)(ptr);
	if (!inToBig)
		returnVal = NXSwapBigShortToHost(returnVal);
		
	return returnVal;
}

UInt32 DSGetAndSwapLong(void* ptr, dsBool inToBig)
{
	UInt32 returnVal = *(UInt32*)(ptr);
	if (inToBig)
		*(UInt32*)(ptr) = NXSwapHostIntToBig(returnVal);
	else
		returnVal = *(UInt32*)(ptr) = NXSwapBigIntToHost(returnVal);
		
	return returnVal;
}

unsigned short DSGetAndSwapShort(void* ptr, dsBool inToBig)
{
	unsigned short returnVal = *(unsigned short*)(ptr);
	if (inToBig)
		*(unsigned short*)(ptr) = NXSwapHostShortToBig(returnVal);
	else
		returnVal = *(unsigned short*)(ptr) = NXSwapBigShortToHost(returnVal);
		
	return returnVal;
}

void DSSwapLong(void* ptr, dsBool inToBig) { DSGetAndSwapLong(ptr, inToBig); }
void DSSwapShort(void* ptr, dsBool inToBig) { DSGetAndSwapShort(ptr, inToBig); }

void DSSwapRecordEntry(char* data, UInt32 type, dsBool inToBig)
{
	short i = 0;
	short j = 0;
	
    // if at any point we see data that doesn't make sense, we just bail
    
    // start with initial length, then type and name strings (each with two byte length)
    UInt32 entryLen = DSGetAndSwapLong(data, inToBig);
    char* dataEnd = data + 4 + entryLen;
    data += 4;
    unsigned short tempLen = DSGetAndSwapShort(data, inToBig);
    data += 2 + tempLen;
    if (data > dataEnd) return;
    tempLen = DSGetAndSwapShort(data, inToBig);
    data += 2 + tempLen;
    if (data > dataEnd) return;
    
    // next is the attribute count (2 bytes)
    unsigned short attrCount = DSGetAndSwapShort(data, inToBig);
    data += 2;
    
    // go through each attribute entry
    for (i = 0; i < attrCount; i++)
    {
        UInt32 attrLen;
        if ( (type != 'StdB') || (type != 'DbgB') )
        {
            // 4 byte attribute length
            attrLen = DSGetAndSwapLong(data, inToBig);
            data += 4;
        }
        else
        {
            // 2 byte attribute length
            attrLen = DSGetAndSwapShort(data, inToBig);
            data += 2;
        }
        
        char* attrEnd = data + attrLen;
        if (attrEnd > dataEnd) return;
        
        // next is attr type
        tempLen = DSGetAndSwapShort(data, inToBig);
        data += 2 + tempLen;
        if (data > attrEnd) return;
        
        // next is attr value count
        unsigned short attrValCount = DSGetAndSwapShort(data, inToBig);
        data += 2;

        // go through each attribute value
        for (j = 0; j < attrValCount; j++)
        {
            UInt32 attrValueLen;
            if ( (type != 'StdB') || (type != 'DbgB') )
            {
                // 4 byte attribute length
                attrValueLen = DSGetAndSwapLong(data, inToBig);
                data += 4;
            }
            else
            {
                // 2 byte attribute length
                attrValueLen = DSGetAndSwapShort(data, inToBig);
                data += 2;
            }
            
            data += attrValueLen;
        }
    }
}

void DSSwapStandardBuf(char* data, UInt32 size, dsBool inToBig)
{
    // check if this buffer is in one of the known formats
    // Must be at least 12 bytes big
    if (size < 12)
        return;
        
    UInt32 type = DSGetLong(data, inToBig);
    if ((type == 'StdA') || (type == 'DbgA') || (type == 'StdB') || (type == 'DbgB') || (type == 'Gdni'))
    {
        // these buffers contain an array of offsets at the beginning, with the record data
        // packed in reverse order at the end of the buffer        
        
        // swap the type
        DSSwapLong(data, inToBig);
        UInt32 recordCount = DSGetAndSwapLong(data + 4, inToBig);
        
        // now swap record entries
		UInt32 j = 0;
        for (j = 0; j < recordCount; j++)
        {
            UInt32 offset = DSGetAndSwapLong(data + (j * 4) + 8, inToBig);
            if (offset > size)	return; // bad buff, so bail
            DSSwapRecordEntry(data + offset, type, inToBig);
        }
        
        // swap the end tag
        DSSwapLong(data + (recordCount * 4) + 8, inToBig);
    }
    else if (type == 'npss')
    {
        // this is similar to the buffer format above, although for some reason the offsets are
        // in reverse order at the end, and the data is at the beginning
        
        // swap the type
        DSSwapLong(data, inToBig);
        UInt32 nodeCount = DSGetAndSwapLong(data + 4, inToBig);
        
		UInt32 i = 0;
        for (i = 0; i < nodeCount; i++)
        {
            UInt32 offset = DSGetAndSwapLong(data + size - (4 * i) - 4, inToBig);
            if (offset > size) return;
            char* tempPtr = data + offset;
            unsigned short numSegments = DSGetAndSwapShort(tempPtr, inToBig);
            tempPtr += 2;
            
			short j = 0;
            for (j = 0; j < numSegments; j++)
            {
                unsigned short segmentLen = DSGetAndSwapShort(tempPtr, inToBig);
                tempPtr += 2 + segmentLen;
                if (tempPtr - data > (SInt32)size) return;
            }
        }
    }
}

void DSSwapObjectData(	UInt32 type,
						char* data,
						UInt32 size,
						dsBool swapAuth,
						dsBool isCustomCall,
						UInt32 inCustomRequestNum,
						const char* inPluginName,
						dsBool isAPICallResponse,
						dsBool inToBig)
{
    // first swap the contents of certain object types
    switch (type)
    {
        // all of these types are really just data buffers
        case ktDataBuff:
        {
			if (isCustomCall && (inPluginName != nil))
			{
				if (strcmp(inPluginName,"Configure") == 0)
				{
					if (inCustomRequestNum == eDSCustomCallConfigureGetAuthRef)
					{
						if (!isAPICallResponse)
						{
							UInt32 totalLen = 0;
							while (totalLen < size)
							{
								UInt32 length = DSGetAndSwapLong(data, inToBig);
								totalLen += length + 4;
								data += length + 4;
							}
							break;
						}
					}
					else if ( inCustomRequestNum == eDSCustomCallConfigureCheckVersion)
					{
						if (isAPICallResponse)
						{
							UInt32 totalLen = 0;
							while (totalLen < size)
							{
								UInt32 length = DSGetAndSwapShort(data, inToBig);
								totalLen += length + 2;
								data += length + 2;
							}
							break;
						}
					}
					else if ( inCustomRequestNum == eDSCustomCallConfigureSCGetKeyPathValueSize )
					{
						if (isAPICallResponse)
						{
							DSSwapLong(data, inToBig);
						}
					}
					else if ( inCustomRequestNum == eDSCustomCallConfigureSCGetKeyValueSize )
					{
						if (isAPICallResponse)
						{
							DSSwapLong(data, inToBig);
						}
					}
				}
				else if (strcmp(inPluginName,"Search") == 0)
				{
					if ( inCustomRequestNum == eDSCustomCallSearchReadDHCPLDAPSize )
					{
						if (isAPICallResponse)
						{
							DSSwapLong(data, inToBig);
						}
					}
				}
				else if (strcmp(inPluginName,"LDAPv3") == 0)
				{
					if (inCustomRequestNum == eDSCustomCallLDAPv3WriteServerMappings)
					{
						if (!isAPICallResponse)
						{
							UInt32 totalLen = 0;
							while (totalLen < size)
							{
								UInt32 length = DSGetAndSwapLong(data, inToBig);
								totalLen += length + 4;
								data += length + 4;
							}
							break;
						}
					}
					else if ( inCustomRequestNum == eDSCustomCallLDAPv3ReadConfigSize )
					{
						if (isAPICallResponse)
						{
							DSSwapLong(data, inToBig);
						}
					}
					else if ( inCustomRequestNum == 1003 ) // eRecordDeleteAndCredentials
					{
						if (!isAPICallResponse)
						{
							DSSwapLong(data, inToBig);
						}
					}
				}
				else if (strcmp(inPluginName,"SMB") == 0)
				{
					if ( inCustomRequestNum == 'xmls' )
					{
						if (isAPICallResponse)
						{
							DSSwapLong(data, inToBig);
						}
					}
					else if (inCustomRequestNum == 'read' )
					{
						if (isAPICallResponse)
						{
							UInt32 totalLen = 0;
							while (totalLen < size)
							{
								UInt32 length = DSGetAndSwapLong(data, inToBig);
								totalLen += length + 4;
								data += length + 4;
							}
							break;
						}
					}
					else if (inCustomRequestNum == 'writ' )
					{
						if (isAPICallResponse)
						{
							data += sizeof( AuthorizationExternalForm );
							UInt32 totalLen = 0;
							while (totalLen < size)
							{
								UInt32 length = DSGetAndSwapLong(data, inToBig);
								totalLen += length + 4;
								data += length + 4;
							}
							break;
						}
					}
				}
			}
			else
			{
				DSSwapStandardBuf(data, size, inToBig);
			}
        }
        case kAttrMatch:
        case kAttrType:
        case kAuthResponseBuff:
        case kAuthMethod:
        case kAttrTypeBuff:
        case kAttrValueBuff:
        case kFirstAttrBuff:
        case kNewAttrBuff:
        case kRecNameBuff:
        case kRecTypeBuff:
            break;
        case kAuthStepBuff:
		case kAuthStepDataResponse:
        {
            if (!swapAuth)
            {
                // two way random calls just have a blob of data,
                // no swapping required
                break;
            }

            // every call of every other type of auth has its buffer packed just like
            // a tDataList, so just fall through to the case below (no break).
        }
        // all of these types are data lists
        case ktDataList:
        case kDirNodeName:
        case kAttrTypeRequestList:
        case kRecTypeList:
        case kAttrTypeList:
        case kRecNameList:
        case kNodeInfoTypeList:
        case kNodeNamePatt:
        case kAttrMatches:
        case kAttrValueList:
        {
            UInt32 totalLen = 0;
            while (totalLen < size)
            {
                UInt32 length = DSGetAndSwapLong(data, inToBig);
                totalLen += length + 4;
                data += length + 4;
            }
        
            break;
        }
        case ktAttrEntry:
        {
            tAttributeEntry* entry = (tAttributeEntry*)data;
            DSSwapLong(&entry->fAttributeValueCount, inToBig);
            DSSwapLong(&entry->fAttributeDataSize, inToBig);
            DSSwapLong(&entry->fAttributeValueMaxSize, inToBig);
            DSSwapLong(&entry->fAttributeSignature.fBufferSize, inToBig);
            DSSwapLong(&entry->fAttributeSignature.fBufferLength, inToBig);
            break;
        }
        case ktAttrValueEntry:
        {
            tAttributeValueEntry* entry = (tAttributeValueEntry*)data;
            DSSwapLong(&entry->fAttributeValueID, inToBig);
            DSSwapLong(&entry->fAttributeValueData.fBufferSize, inToBig);
            DSSwapLong(&entry->fAttributeValueData.fBufferLength, inToBig);
            break;
        }
        case ktRecordEntry:
        {
            short tempLen;
            tRecordEntry* entry = (tRecordEntry*)data;
            DSSwapLong(&entry->fRecordAttributeCount, inToBig);
            DSSwapLong(&entry->fRecordNameAndType.fBufferSize, inToBig);
            DSSwapLong(&entry->fRecordNameAndType.fBufferLength, inToBig);

            // fRecordNameAndType has some embedded lengths
            char* ptr = (char*)&entry->fRecordNameAndType.fBufferData[0];
            tempLen = DSGetAndSwapShort(ptr, inToBig);
            ptr += tempLen + 2;
            DSSwapShort(ptr, inToBig);
            break;
        }
        
        default:
            // everything else should be a simple value with a 0 length
            if (size != 0)
            {
                syslog( LOG_INFO, "*** DS Error in: %s at: %d: Unexpected pObj\n of type %d", __FILE__, __LINE__, type );
            }
            break;
    }
}

#endif
