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
 
#ifndef __BIG_ENDIAN__

#include "DSTCPEndian.h"
#include <string.h>
#include <errno.h>			// system call error numbers
#include <unistd.h>			// for select call 
#include <stdlib.h>			// for calloc()
#include <stdio.h>
#include <ctype.h>
#include <syslog.h>
#include <Security/Authorization.h>

#ifdef DSSERVERTCP

#include "CServerPlugin.h"
#include "CLog.h"
#include "CRefTable.h"

#else

#include "CDSRefMap.h"
#include "DirServicesPriv.h"

#endif

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

void DumpBuf(char* buf, UInt32 len)
{
    char acsiiBuf[17];
    
    for (UInt32 i = 0; i < len; i++)
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


DSTCPEndian::DSTCPEndian(sComProxyData* message, int direction) : fMessage(message)
{
    toBig = (direction == kDSSwapToBig);
	fIPAddress = 0;
	fPort = 0;
}

void DSTCPEndian::SwapMessage()
{
    short i;
    sObject* object;
    bool isTwoWay = false;
	UInt32 ourMsgID = 0;
    
    if (fMessage == nil) return;
    
#if DUMP_BUFFER
    UInt32 bufSize = 0;
    if (gDumpFile == NULL)
        gDumpFile = fopen("/Library/Logs/DirectoryService/PacketDump", "w");
        
    if (toBig)
        fprintf(gDumpFile, "\n\n\nBuffer in host order (preSwap)\n");
    else
        fprintf(gDumpFile, "\n\n\nBuffer in network order (preSwap)\n");
    for (i=0; i< 10; i++)
    {
        object = &fMessage->obj[i];
        UInt32 objType = DSGetLong(&object->type, toBig);
        UInt32 offset = DSGetLong(&object->offset, toBig);
        UInt32 length = DSGetLong(&object->length, toBig);
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
            UInt32 size = offset + length - sizeof(sComProxyData) + 4;
            if (size > bufSize) bufSize = size;
        }
    }
    DumpBuf(fMessage->data, bufSize);
#endif
    
	ourMsgID = fMessage->fMsgID; //want this to be consistent below as host order

    DSSwapLong(&fMessage->fDataSize, toBig);
    DSSwapLong(&fMessage->fDataLength, toBig);
    DSSwapLong(&fMessage->fMsgID, toBig);
    DSSwapLong(&fMessage->fPID, toBig); //FW side uses PID BUT the server receiving side uses the TCP port instead
    //DSSwapLong(&fMessage->fIPAddress, toBig); //obtained directly from the endpoint and not present yet in this value
    
	UInt32 aNodeRef = 0;
#ifndef DSSERVERTCP
	UInt32 aNodeRefMap = 0; //used with ktNodeRefMap only for FW
#endif
	//handle CustomCall endian issues - need to determine which plugin is being used
	bool bCustomCall = false;
	UInt32 aCustomRequestNum = 0;
	bool bIsAPICallResponse = false;
	const char* aPluginName = nil;
    // if this is the auth case, we need to do some checks
    for (i=0; i< 10; i++)
    {
        object = &fMessage->obj[i];
        UInt32 objType = DSGetLong(&object->type, toBig);

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
			aCustomRequestNum = (UInt32)DSGetLong(&object->count, toBig);
#ifdef DSSERVERTCP
DbgLog(kLogTCPEndpoint, "DSSERVERTCP>DSTCPEndian::SwapMessage(): kCustomRequestCode with code %u", aCustomRequestNum );
#else
LOG1(kStdErr, "FW-DSTCPEndian::SwapMessage(): kCustomRequestCode with code %u", aCustomRequestNum );
#endif
		}
		
		if (objType == ktNodeRef)
		{
			//need to determine the nodename for discrimination of duplicate custom call codes - server and FW sends
			aNodeRef = (UInt32)DSGetLong(&object->count, toBig);
		}
#ifndef DSSERVERTCP
		if (objType == ktNodeRefMap)
		{
			//need to determine the nodename for discrimination of duplicate custom call codes - FW only
			aNodeRefMap = (UInt32)DSGetLong(&object->count, toBig);
			//need to keep this since it should come back as well from the server
		}
#endif

		if (objType == kResult)
		{
			bIsAPICallResponse = true;
		}
    } // for (i=0; i< 10; i++)
	
#ifndef DSSERVERTCP
	if (bIsAPICallResponse)
	{
		DSSwapLong(&ourMsgID, toBig);
		if (aCustomRequestNum == 0)
		{
			aCustomRequestNum = CDSRefMap::GetCustomCodeFromMsgIDMap( ourMsgID );
			if (aCustomRequestNum != 0)
			{
				CDSRefMap::RemoveMsgIDToCustomCodeMap( ourMsgID );
				bCustomCall = true;
			}
		}
	}
#endif

	if ( bCustomCall && (aCustomRequestNum != 0) )
	{
#ifdef DSSERVERTCP
		if (aNodeRef != 0)
		{
			CServerPlugin *aPluginPtr = nil;
			SInt32 myResult = CRefTable::VerifyNodeRef( aNodeRef, &aPluginPtr, fPort, fIPAddress );
			if (myResult == eDSNoErr)
			{
				aPluginName = aPluginPtr->GetPluginName();
			}
		}
#else
		if (aNodeRefMap != 0)
		{
			CDSRefMap::MapMsgIDToServerRef( ourMsgID, aNodeRef );
			CDSRefMap::MapMsgIDToCustomCode( ourMsgID, aCustomRequestNum );
			aPluginName = dsGetPluginNamePriv( aNodeRefMap, getpid() ); //FW side this PID should work but on server side we use the IP Port instead
		}
		else
		{
			if (bIsAPICallResponse)
			{
				UInt32 ourServerRef = 0;
				ourServerRef = CDSRefMap::GetServerRefFromMsgIDMap( ourMsgID );
				CDSRefMap::RemoveMsgIDToServerRefMap( ourMsgID );
				aNodeRefMap = CDSRefMap::GetLocalRefFromServerMap( ourServerRef );
				aPluginName = dsGetPluginNamePriv( aNodeRefMap, getpid() ); //FW side this PID should work but on server side we use the IP Port instead
			}
		}
#endif
	}

    // swap objects
    for (i=0; i< 10; i++)
    {
        object = &fMessage->obj[i];

        if (object->type == 0)
            continue;
            
        UInt32 objType = DSGetAndSwapLong(&object->type, toBig);
        DSSwapLong(&object->count, toBig);
        UInt32 objOffset = DSGetAndSwapLong(&object->offset, toBig);
        DSSwapLong(&object->used, toBig);
        UInt32 objLength = DSGetAndSwapLong(&object->length, toBig);
            
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

void DSTCPEndian::AddIPAndPort( UInt32 inIPAddress, UInt32 inPort)
{
	fIPAddress = inIPAddress;
	fPort = inPort;
}


#endif
