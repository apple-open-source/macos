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
 * @header DSTCPEndian
 * Provides routines to byte swap DSProxy buffers.
 */
 
#include "DSTCPEndian.h"
#include <string.h>
#include <errno.h>			// system call error numbers
#include <unistd.h>			// for select call 
#include <stdlib.h>			// for calloc()
#include <stdio.h>
#include <ctype.h>

#include "CLog.h"

#define DUMP_BUFFER 0
#if DUMP_BUFFER

char* objectTypes[] =
{
	"kResult",
	"ktDirRef",
	"ktNodeRef",
	"ktRecRef",
	"ktAttrListRef",
	"ktAttrValueListRef",
	"ktDataBuff",
	"ktDataList",
	"ktDirPattMatch",
	"kAttrPattMatch",
	"kAttrMatch",
	"kMatchRecCount",
	"kNodeNamePatt",
	"ktAccessControlEntry",
	"ktAttrEntry",
	"ktAttrValueEntry",
	"kOpenRecBool",
	"kAttrInfoOnly",
	"kRecFlags",
	"kAttrFlags",
	"kRecEntryIndex",
	"kAttrInfoIndex",
	"kAttrValueIndex",
	"kAttrValueID",
	"kOutBuffLen",
	"kAuthStepDataLen",
	"kAuthOnlyBool",
	"kDirNodeName",
	"kAuthMethod",
	"kNodeInfoTypeList",
	"kRecNameList",
	"kRecTypeList",
	"kAttrTypeList",
	"kRecTypeBuff",
	"kRecNameBuff",
	"kAttrType",
	"kAttrTypeBuff",
	"kAttrValueBuff",
	"kNewAttrBuff",
	"kFirstAttrBuff",
    "unknown",
	"kAttrBuff",
	"kAuthStepBuff",
	"kAuthResponseBuff",
	"kAttrTypeRequestList",
	"kCustomRequestCode",
	"kPluginName",

	"kNodeCount",
	"kNodeIndex",
	"kAttrInfoCount",
	"kAttrRecEntryCount",
	"ktRecordEntry",
	"kAuthStepDataResponse",

	"kContextData",
	"ktPidRef",
	"ktGenericRef",
	"kNodeChangeToken",
	"ktEffectiveUID",
	"ktUID"
};

FILE* gDumpFile = NULL;

void DumpBuf(char* buf, uInt32 len)
{
    char acsiiBuf[17];
    
    for (uInt32 i = 0; i < len; i++)
    {
        if ((i % 16) == 0) ::memset(acsiiBuf, 0, 17);
        fprintf(gDumpFile, "%02X ", buf[i]);
        if (::isprint(buf[i]))
            acsiiBuf[i % 16] = buf[i];
        else
            acsiiBuf[i % 16] = '.';
        if ((i % 16) == 15) fprintf(gDumpFile, "   %s\n", acsiiBuf);
    }
}
#endif


DSTCPEndian::DSTCPEndian(sComData* fMessage, int direction) : fMessage(fMessage)
{
    toBig = (direction == kSwapToBig);
}

void DSTCPEndian::SwapMessage()
{
    short i;
    sObject* object;
    bool isTwoWay = false;
    
    if (fMessage == nil) return;
    
#if DUMP_BUFFER
    uInt32 bufSize = 0;
    if (gDumpFile == NULL)
        gDumpFile = fopen("/Library/Logs/DirectoryService/PacketDump", "w");
        
    if (toBig)
        fprintf(gDumpFile, "\n\n\nBuffer in host order (preSwap)\n");
    else
        fprintf(gDumpFile, "\n\n\nBuffer in network order (preSwap)\n");
    for (i=0; i< 10; i++)
    {
        object = &fMessage->obj[i];
        uInt32 objType = GetLong(&object->type);
        uInt32 offset = GetLong(&object->offset);
        uInt32 length = GetLong(&object->length);
        char* type = "unknown";
        if (objType >= 4460 && objType <= 4519)
            type = objectTypes[objType - 4460];
        if (objType != 0)
        {
            if (length > 0)
                    fprintf(gDumpFile, "Object %d, type %s, offset %ld, size %ld\n", i, type, GetLong(&object->offset), GetLong(&object->length));
            else
                    fprintf(gDumpFile, "Object %d, type %s, value %ld\n", i, type, GetLong(&object->count));
        }
        if (length > 0)
        {
            uInt32 size = offset + length - sizeof(sComData) + 4;
            if (size > bufSize) bufSize = size;
        }
    }
    DumpBuf(fMessage->data, bufSize);
#endif
    
    SwapLong(&fMessage->fDataSize);
    SwapLong(&fMessage->fDataLength);
    SwapLong(&fMessage->fMsgID);
    SwapLong(&fMessage->fPID);
    
    // if this is the auth case, we need to do some checks
    for (i=0; i< 10; i++)
    {
        object = &fMessage->obj[i];
        uInt32 objType = GetLong(&object->type);

        // check for two-way randnum special case before we start swapping stuff
        if (objType == kAuthMethod)
        {
            char* method = (char *)fMessage + GetLong(&object->offset);
            if ( ::strcmp( method, kDSStdAuth2WayRandom ) == 0 )
                isTwoWay = true;
        }
    }

    // swap objects
    for (i=0; i< 10; i++)
    {
        object = &fMessage->obj[i];

        if (object->type == 0)
            continue;
            
        uInt32 objType = GetAndSwapLong(&object->type);
        SwapLong(&object->count);
        uInt32 objOffset = GetAndSwapLong(&object->offset);
        SwapLong(&object->used);
        uInt32 objLength = GetAndSwapLong(&object->length);
            
        SwapObjectData(objType, (char *)fMessage + objOffset, objLength, (!isTwoWay));
    }
    
#if DUMP_BUFFER
    if (toBig)
        fprintf(gDumpFile, "\n\nBuffer in network order (post Swap)\n");
    else
        fprintf(gDumpFile, "\n\nBuffer in host order (post Swap)\n");
    DumpBuf(fMessage->data, bufSize);
    fflush(stdout);
#endif
}

void DSTCPEndian::SwapObjectData(uInt32 type, char* data, uInt32 size, bool swapAuth)
{
    // first swap the contents of certain object types
    switch (type)
    {
        // all of these types are really just data bufs
        case ktDataBuff:
        {
            SwapStandardBuf(data, size);
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
        {
            uInt32 totalLen = 0;
            while (totalLen < size)
            {
                uInt32 length = GetAndSwapLong(data);
                totalLen += length + 4;
                data += length + 4;
            }
        
            break;
        }
        case ktAttrEntry:
        {
            tAttributeEntry* entry = (tAttributeEntry*)data;
            SwapLong(&entry->fAttributeValueCount);
            SwapLong(&entry->fAttributeDataSize);
            SwapLong(&entry->fAttributeValueMaxSize);
            SwapLong(&entry->fAttributeSignature.fBufferSize);
            SwapLong(&entry->fAttributeSignature.fBufferLength);
            break;
        }
        case ktAttrValueEntry:
        {
            tAttributeValueEntry* entry = (tAttributeValueEntry*)data;
            SwapLong(&entry->fAttributeValueID);
            SwapLong(&entry->fAttributeValueData.fBufferSize);
            SwapLong(&entry->fAttributeValueData.fBufferLength);
            break;
        }
        case ktRecordEntry:
        {
            short tempLen;
            tRecordEntry* entry = (tRecordEntry*)data;
            SwapLong(&entry->fRecordAttributeCount);
            SwapLong(&entry->fRecordNameAndType.fBufferSize);
            SwapLong(&entry->fRecordNameAndType.fBufferLength);

            // fRecordNameAndType has some embedded lengths
            char* ptr = (char*)&entry->fRecordNameAndType.fBufferData[0];
            tempLen = GetAndSwapShort(ptr);
            ptr += tempLen + 2;
            SwapShort(ptr);
            break;
        }
        
        default:
            // everything else should be a simple value with a 0 length
            if (size != 0)
            {
#ifdef DSSERVERTCP
                DBGLOG3( kLogTCPEndpoint, "*** DS Error in: %s at: %d: Unexpected pObj of type %d\n", __FILE__, __LINE__, type );
#else
                LOG3( kStdErr, "*** DS Error in: %s at: %d: Unexpected pObj\n of type %d", __FILE__, __LINE__, type );
#endif
            }
            break;
    }
}

void DSTCPEndian::SwapStandardBuf(char* data, uInt32 size)
{
    // check if this buffer is in one of the known formats
    // Must be at least 12 bytes big
    if (size < 12)
        return;
        
    uInt32 type = GetLong(data);
    if ((type == 'StdA') || (type == 'StdB') || (type == 'Gdni'))
    {
        // these buffers contain an array of offsets at the beginning, with the record data
        // packed in reverse order at the end of the buffer        
        
        // swap the type
        SwapLong(data);
        uInt32 recordCount = GetAndSwapLong(data + 4);
        
        // now swap record entries
        for (uInt32 j = 0; j < recordCount; j++)
        {
            uInt32 offset = GetAndSwapLong(data + (j * 4) + 8);
            if (offset > size)	return; // bad buff, so bail
            SwapRecordEntry(data + offset, type);
        }
        
        // swap the end tag
        SwapLong(data + (recordCount * 4) + 8);
    }
    else if (type == 'npss')
    {
        // this is similar to the buffer format above, although for some reason the offsets are
        // in reverse order at the end, and the data is at the beginning
        
        // swap the type
        SwapLong(data);
        uInt32 nodeCount = GetAndSwapLong(data + 4);
        
        for (uInt32 i = 0; i < nodeCount; i++)
        {
            uInt32 offset = GetAndSwapLong(data + size - (4 * i) - 4);
            if (offset > size) return;
            char* tempPtr = data + offset;
            uInt16 numSegments = GetAndSwapShort(tempPtr);
            tempPtr += 2;
            
            for (short j = 0; j < numSegments; j++)
            {
                uInt16 segmentLen = GetAndSwapShort(tempPtr);
                tempPtr += 2 + segmentLen;
                if (tempPtr - data > (long)size) return;
            }
        }
    }
}


void DSTCPEndian::SwapRecordEntry(char* data, uInt32 type)
{
    // if at any point we see data that doesn't make sense, we just bail
    
    // start with initial length, then type and name strings (each with two byte length)
    uInt32 entryLen = GetAndSwapLong(data);
    char* dataEnd = data + entryLen;
    data += 4;
    uInt16 tempLen = GetAndSwapShort(data);
    data += 2 + tempLen;
    if (data > dataEnd) return;
    tempLen = GetAndSwapShort(data);
    data += 2 + tempLen;
    if (data > dataEnd) return;
    
    // next is the attribute count (2 bytes)
    uInt16 attrCount = GetAndSwapShort(data);
    data += 2;
    
    // go through each attribute entry
    for (short i = 0; i < attrCount; i++)
    {
        uInt32 attrLen;
        char* attrEnd = data;
        if (type != 'stdB')
        {
            // 4 byte attribute length
            attrLen = GetAndSwapLong(data);
            data += 4;
        }
        else
        {
            // 2 byte attribute length
            attrLen = GetAndSwapShort(data);
            data += 2;
        }
        
        attrEnd += attrLen;
        if (attrEnd > dataEnd) return;
        
        // next is attr type
        tempLen = GetAndSwapShort(data);
        data += 2 + tempLen;
        if (data > attrEnd) return;
        
        // next is attr value count
        uInt16 attrValCount = GetAndSwapShort(data);
        data += 2;

        // go through each attribute value
        for (short j = 0; j < attrValCount; j++)
        {
            uInt32 attrValueLen;
            if (type != 'stdB')
            {
                // 4 byte attribute length
                attrValueLen = GetAndSwapLong(data);
                data += 4;
            }
            else
            {
                // 2 byte attribute length
                attrValueLen = GetAndSwapShort(data);
                data += 2;
            }
            
            data += attrValueLen;
        }
    }
}

