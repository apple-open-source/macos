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
 * @header SharedConsts
 */

#ifndef __SharedConsts_h__
#define	__SharedConsts_h__	1

#include <mach/message.h>

#include "PrivateTypes.h"

typedef struct {
	unsigned int	msgt_name : 8,
					msgt_size : 8,
					msgt_number : 12,
					msgt_inline : 1,
					msgt_longform : 1,
					msgt_deallocate : 1,
					msgt_unused : 1;
} mach_msg_type_t;

typedef struct sObject
{
	uInt32		type;
	uInt32		count;
	uInt32		offset;
	uInt32		used;
	uInt32		length;
} sObject;

typedef struct sComData
{
	mach_msg_header_t	head;
	mach_msg_type_t		type;
	uInt32				fDataSize;
	uInt32				fDataLength;
	uInt32				fMsgID;
	uInt32				fPID;
	uInt32				fPort;
	uInt32				fIPAddress;
	sObject				obj[ 10 ];
	char				data[ 1 ];
} sComData;

const uInt32 kMsgBlockSize	= 1024 * 4;					// Set to average of 4k
const uInt32 kObjSize		= sizeof( sObject ) * 10;	// size of object struct
//const uInt32 kIPCMsgLen		= kMsgBlockSize + kObjSize;	// IPC message block size
const uInt32 kIPCMsgLen		= kMsgBlockSize;	// IPC message block size

typedef struct sIPCMsg
{
	mach_msg_header_t	fHeader;
	uInt32				fMsgType;
	uInt32				fCount;
	uInt32				fOf;
	uInt32				fMsgID;
	uInt32				fPID;
	uInt32				fPort;
	sObject				obj[ 10 ];
	char				fData[ kIPCMsgLen ];
	mach_msg_security_trailer_t	fTail;	//this is the largest trailer struct
										//we never set this and we never send it
										//but we have the bucket large enough to receive it
} sIPCMsg;



typedef enum {
	kResult					= 4460,
	ktDirRef				= 4461,
	ktNodeRef				= 4462,
	ktRecRef				= 4463,
	ktAttrListRef			= 4464,
	ktAttrValueListRef		= 4465,
	ktDataBuff				= 4466,
	ktDataList				= 4467,
	ktDirPattMatch			= 4468,
	kAttrPattMatch			= 4469,
	kAttrMatch				= 4470,
	kMatchRecCount			= 4471,
	kNodeNamePatt			= 4472,
	ktAccessControlEntry	= 4473,
	ktAttrEntry				= 4474,
	ktAttrValueEntry		= 4475,
	kOpenRecBool			= 4476,
	kAttrInfoOnly			= 4477,
	kRecFlags				= 4478,
	kAttrFlags				= 4479,
	kRecEntryIndex			= 4480,
	kAttrInfoIndex			= 4481,
	kAttrValueIndex			= 4482,
	kAttrValueID			= 4483,
	kOutBuffLen				= 4484,
	kAuthStepDataLen		= 4485,
	kAuthOnlyBool			= 4486,
	kDirNodeName			= 4487,
	kAuthMethod				= 4488,
	kNodeInfoTypeList		= 4489,
	kRecNameList			= 4490,
	kRecTypeList			= 4491,
	kAttrTypeList			= 4492,
	kRecTypeBuff			= 4493,
	kRecNameBuff			= 4494,
	kAttrType				= 4495,
	kAttrTypeBuff			= 4496,
	kAttrValueBuff			= 4497,
	kNewAttrBuff			= 4498,
	kFirstAttrBuff			= 4499,
	kAttrBuff				= 4501,
	kAuthStepBuff			= 4502,
	kAuthResponseBuff		= 4503,
	kAttrTypeRequestList	= 4504,
	kCustomRequestCode		= 4505,
	kPluginName				= 4506,

	kNodeCount				= 4507,
	kNodeIndex				= 4508,
	kAttrInfoCount			= 4509,
	kAttrRecEntryCount		= 4510,
	ktRecordEntry			= 4511,
	kAuthStepDataResponse	= 4512,

	kContextData			= 4513,
	ktPidRef				= 4514,
	ktGenericRef			= 4515,
	kNodeChangeToken		= 4516,
	ktEffectiveUID			= 4517,
	ktUID					= 4518,
	kEnd					= 0xFFFFFFFF
} eValueType;

//Note any time an entry point reference is added it must be ensured that
//the DSPlugInStub, and DSPlugInStubC plugins all have the corresponding changes made.
//Also note that the enums combined for Server and PlugIn calls cannot exceed 255 since
//unsigned int	msgt_name : 8 --> is the used field in the mach_msg_type_t used in sComData to send the type of call
//Allow server calls to be 1 to 127 ****Only add numbers on to the end of this list - DON'T change any for backward compatibility****
enum eDSServerCalls {
	/*  1 */ kOpenDirService			= 1,
	/*    */ kCloseDirService,
	/*    */ kGetDirNodeName,
	/*    */ kGetDirNodeCount,
	/*  5 */ kGetDirNodeChangeToken,
	/*    */ kGetDirNodeList,
	/*    */ kFindDirNodes,
	/*    */ kVerifyDirRefNum,
	/*    */ kCheckUserNameAndPassword,
	/* 10 */ kAddChildPIDToReference,
	/*    */ kOpenDirServiceProxy,
	/* 12 */ kDSServerCallsEnd
};

//Note any time an entry point reference is added it must be ensured that
//the DSPlugInStub, and DSPlugInStubC plugins all have the corresponding changes made.
//Also note that the enums combined for Server and PlugIn calls cannot exceed 255 since
//unsigned int	msgt_name : 8 --> is the used field in the mach_msg_type_t used in sComData to send the type of call
//allow plugin calls to be 128 to 255 ****Only add numbers on to the end of this list - DON'T change any for backward compatibility****
enum eDSPluginCalls {
	/* 128 */ kDSPlugInCallsBegin	=  128,
	/*     */ kReleaseContinueData,
	/* 130 */ kOpenDirNode,
	/*     */ kCloseDirNode,
	/*     */ kGetDirNodeInfo,
	/*     */ kGetRecordList,
	/*     */ kGetRecordEntry,
	/* 135 */ kGetAttributeEntry,
	/*     */ kGetAttributeValue,
	/*     */ kOpenRecord,
	/*     */ kGetRecordReferenceInfo,
	/*     */ kGetRecordAttributeInfo,
	/* 140 */ kGetRecordAttributeValueByID,
	/*     */ kGetRecordAttributeValueByIndex,
	/*     */ kFlushRecord,
	/*     */ kCloseRecord,
	/*     */ kSetRecordName,
	/* 145 */ kSetRecordType,
	/*     */ kDeleteRecord,
	/*     */ kCreateRecord,
	/*     */ kCreateRecordAndOpen,
	/*     */ kAddAttribute,
	/* 150 */ kRemoveAttribute,
	/*     */ kAddAttributeValue,
	/*     */ kRemoveAttributeValue,
	/*     */ kSetAttributeValue,
	/*     */ kDoDirNodeAuth,
	/* 155 */ kDoAttributeValueSearch,
	/*     */ kDoAttributeValueSearchWithData,
	/*     */ kDoPlugInCustomCall,
	/*     */ kCloseAttributeList,
	/*     */ kCloseAttributeValueList,
	/*     */ kHandleNetworkTransition,
	/*     */ kServerRunLoop,
	/*     */ kDoDirNodeAuthOnRecordType,
	/*     */ kCheckNIAutoSwitch,
	/* 164 */ kDSPlugInCallsEnd
};



#endif // __SharedConsts_h__
