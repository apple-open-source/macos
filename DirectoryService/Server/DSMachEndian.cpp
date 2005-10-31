/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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
 * @header DSMachEndian
 * Provides routines to byte swap Mach buffers.
 */
 
#ifndef __BIG_ENDIAN__

#include "DSMachEndian.h"
#include <string.h>
#include <errno.h>			// system call error numbers
#include <unistd.h>			// for select call 
#include <stdlib.h>			// for calloc()
#include <stdio.h>
#include <ctype.h>
#include <syslog.h>
#include <Security/Authorization.h>

#include "CServerPlugin.h"
#include "CLog.h"
#include "CReftable.h"
#include "CSharedData.h"

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
	"ktUID",
	
	"kAttrMatches",
	"kAttrValueList"
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


DSMachEndian::DSMachEndian(sComData* fMessage, int direction) : fMessage(fMessage)
{
    toBig = (direction == kDSSwapToBig);
}

void DSMachEndian::SwapMessage()
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
        uInt32 objType = DSGetLong(&object->type, toBig);
        uInt32 offset = DSGetLong(&object->offset, toBig);
        uInt32 length = DSGetLong(&object->length, toBig);
        char* type = "unknown";
        if (objType >= kResult && objType <= kAttrValueList)
            type = objectTypes[objType - kResult];
        if (objType != 0)
        {
            if (length > 0)
                    fprintf(gDumpFile, "Object %d, type %s, offset %ld, size %ld\n", i, type, DSGetLong(&object->offset, toBig), DSGetLong(&object->length, toBig));
            else
                    fprintf(gDumpFile, "Object %d, type %s, value %ld\n", i, type, DSGetLong(&object->count, toBig));
        }
        if (length > 0)
        {
            uInt32 size = offset + length - sizeof(sComData) + 4;
            if (size > bufSize) bufSize = size;
        }
    }
    DumpBuf(fMessage->data, bufSize);
#endif
    
    DSSwapLong(&fMessage->fDataSize, toBig);
    DSSwapLong(&fMessage->fDataLength, toBig);
    DSSwapLong(&fMessage->fMsgID, toBig);
    DSSwapLong(&fMessage->fPID, toBig);
    //DSSwapLong(&fMessage->fIPAddress, toBig); //zero is used for the mach ipc case
    
	uInt32 aNodeRef = 0;

	//handle CustomCall endian issues - need to determine which plugin is being used
	bool bCustomCall = false;
	uInt32 aCustomRequestNum = 0;
	bool bIsAPICallResponse = false;
	const char* aPluginName = nil;
    // if this is the auth case or custom call case, we need to do some checks
    for (i=0; i< 10; i++)
    {
        object = &fMessage->obj[i];
        uInt32 objType = DSGetLong(&object->type, toBig);

        // check for two-way random special case before we start swapping stuff
        if (objType == kAuthMethod)
        {
            char* method = (char *)fMessage + DSGetLong(&object->offset, toBig);
            if ( ::strcmp( method, kDSStdAuth2WayRandom ) == 0 )
                isTwoWay = true;
        }

		//check for Custom Call special casing
		if (objType == kCustomRequestCode)
		{
			bCustomCall = true;
			aCustomRequestNum = (uInt32)DSGetLong(&object->count, toBig);
DBGLOG1(kLogTCPEndpoint, "DSMachEndian::SwapMessage(): kCustomRequestCode with code %u", aCustomRequestNum );
		}
		
		if (objType == ktNodeRef)
		{
			//need to determine the nodename for discrimination of duplicate custom call codes - server and FW sends
			aNodeRef = (uInt32)DSGetLong(&object->count, toBig);
		}

		if (objType == kResult)
		{
			bIsAPICallResponse = true;
		}
    } // for (i=0; i< 10; i++)
	
	if ( bCustomCall && (aCustomRequestNum != 0) )
	{
		if (aNodeRef != 0)
		{
			CServerPlugin *aPluginPtr = nil;
			sInt32 myResult = CRefTable::VerifyNodeRef( aNodeRef, &aPluginPtr, fMessage->fPID, 0 );
			if (myResult == eDSNoErr)
			{
				aPluginName = aPluginPtr->GetPluginName();
				if (aPluginName != nil)
				{
				}
			}
		}
	}

    // swap objects
    for (i=0; i< 10; i++)
    {
        object = &fMessage->obj[i];

        if (object->type == 0)
            continue;
            
        uInt32 objType = DSGetAndSwapLong(&object->type, toBig);
        DSSwapLong(&object->count, toBig);
        uInt32 objOffset = DSGetAndSwapLong(&object->offset, toBig);
        DSSwapLong(&object->used, toBig);
        uInt32 objLength = DSGetAndSwapLong(&object->length, toBig);
            
        DSSwapObjectData(objType, (char *)fMessage + objOffset, objLength, (!isTwoWay), bCustomCall, aCustomRequestNum, (const char*)aPluginName, bIsAPICallResponse, toBig);
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


#endif
