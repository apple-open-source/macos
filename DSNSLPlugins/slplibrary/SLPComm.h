/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
 
/*!
 *  @header SLPComm
 *  IPC calling conventions for communication with slpd
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |            Code               |            Length             |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |           Status              |                Data           /
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

#ifndef _SLPComm_
#define _SLPComm_
#pragma once

#define kTCPBufferSize		1400
#define kRAdminIPCPath		"/tmp/slpRAdmin_ipc"

const CFStringRef	kSLPRAdminNotificationSAFE_CFSTR = CFSTR("SLP RAdmin Notification");

enum {
	kSLPWrongMessageTypeErr		= -27,		// we want to make this append to the SLPInternalError types
	kSLPNullBufferErr			= -28
};

enum SLPdMessageType {
    kSLPInvalidMessageType			= -1,
    kSLPFirstMessageType			= 1,
    kSLPRegisterURL					= kSLPFirstMessageType,
    kSLPDeregisterURL				= 2,
    kSLPDAStatusQuery				= 3,
    kSLPDAStatusReply				= 4,
    kSLPTurnOnDA					= 5,
    kSLPTurnOffDA					= 6,
    kSLPGetScopeList				= 7,
    kSLPSetScopeList				= 8,
    kSLPAddScope					= 9,
    kSLPDeleteScope					= 10,
    kSLPScopeList					= 11,
    kSLPGetLoggingLevel				= 12,
    kSLPSetLoggingLevel				= 13,
    kSLPGetRegisteredSvcs			= 14,
    kSLPTurnOnRAdminNotifications	= 15,
    kSLPTurnOffRAdminNotifications	= 16,
    kSLPScopeDeleted				= 17,
    kSLPStartUp						= 18,
    kSLPShutDown					= 19,

    kSLPLastMessageType
};

const char* PrintableHeaderType( void* message );

typedef struct SLPdMessageHeader {
	UInt16			messageType;
	UInt16			messageLength;
	SInt16			messageStatus;
} SLPdMessageHeader;

#define SLPDAStatus UInt16
enum {
	kSLPDARunning		= 0x0003,	// match up with our RAdmin values for ease of debugging
	kSLPDANotRunning	= 0x0001
};

OSStatus SendDataToSLPd(	char* data,
							UInt32 dataLen, 
							char** returnData, 
							UInt32* returnDataLen );

OSStatus SendDataToSLPRAdmin(	char* data,
                                UInt32 dataLen,
                                char** returnData,
                                UInt32* returnDataLen );

OSStatus SendDataViaIPC(	char* ipc_path,
                            char* data,
                            UInt32 dataLen,
                            char** returnData,
                            UInt32* returnDataLen );

OSStatus RunSLPLoad( void );

/* Creation Routines */
char* MakeSLPRegistrationDataBuffer(	char* scopeListPtr, 
										UInt32 scopeListLen, 
										char* urlPtr, 					
										UInt32 urlLen, 					
										char* attributeList, 
										UInt32 attributeListLen, 
										UInt32* dataBufferLen );

char* MakeSLPDeregistrationDataBuffer(	char* scopeListPtr, 
                                        UInt32 scopeListLen, 
                                        char* urlPtr, 
                                        UInt32 urlLen, 
                                        UInt32* dataBufferLen );

char* MakeSLPRegDeregDataBuffer(	SLPdMessageType messageType,
									char* scopeListPtr, 
									UInt32 scopeListLen, 
									char* urlPtr, 
									UInt32 urlLen, 
									char* attributeList, 
									UInt32 attributeListLen, 
									UInt32* dataBufferLen );

char* MakeSLPScopeListDataBuffer( 	char* scopeListPtr, 
									UInt32 scopeListLen, 
                                    UInt32* dataBufferLen );

char* MakeSLPSetScopeListDataBuffer( 	char* scopeListPtr, 
                                        UInt32 scopeListLen, 
                                        UInt32* dataBufferLen );

char* _MakeSLPScopeListDataBuffer( 	SLPdMessageType messageType,
									char* scopeListPtr, 
									UInt32 scopeListLen, 
                                    UInt32* dataBufferLen );

char* MakeSLPIOCallBuffer		(	Boolean	turnOnSLPd,
									UInt32* dataBufferLen );
                                    
OSStatus MakeSLPSimpleRequestDataBuffer( SLPdMessageType messageType, UInt32* dataBufferLen, char** dataBuffer );

OSStatus MakeSLPGetDAStatusDataBuffer( UInt32* dataBufferLen, char** dataBuffer );
OSStatus MakeSLPDAStatus( SLPDAStatus daStatus, UInt32* dataBufferLen, char** dataBuffer );
OSStatus MakeSLPTurnOnDADataBuffer( UInt32* dataBufferLen, char** dataBuffer );
OSStatus MakeSLPTurnOffDADataBuffer( UInt32* dataBufferLen, char** dataBuffer );
OSStatus MakeSLPGetDAScopeListDataBuffer( UInt32* dataBufferLen, char** dataBuffer );
OSStatus MakeSLPGetLoggingOptionsDataBuffer( UInt32* dataBufferLen, char** dataBuffer );
OSStatus MakeSLPSetLoggingOptionsDataBuffer( UInt32 logLevel, UInt32* dataBufferLen, char** dataBuffer );
OSStatus MakeSLPGetRegisteredSvcsDataBuffer( UInt32* dataBufferLen, char** dataBuffer );

char* MakeSLPScopeDeletedDataBuffer(	char* scopePtr,
                                        UInt32 scopeLen,
                                        UInt32* dataBufferLen );


/* Parsing Routines */
SLPdMessageType GetSLPMessageType( void* dataBuffer );

OSStatus GetSLPRegistrationDataFromBuffer(	char* dataBuffer, 
											char** scopeListPtr, 		// sets ptr to data in dataBuffer
											UInt32* scopeListLen, 
											char** urlPtr, 				// sets ptr to data in dataBuffer
											UInt32* urlLen, 
											char** attributeListPtr, 
											UInt32* attributeListLen );
											
OSStatus GetSLPDeregistrationDataFromBuffer(	char* dataBuffer, 
												char** scopeListPtr, 	// sets ptr to data in dataBuffer
												UInt32* scopeListLen, 
												char** urlPtr, 			// sets ptr to data in dataBuffer
												UInt32* urlLen );
											

OSStatus GetSLPRegDeregDataFromBuffer(	SLPdMessageType messageType,
										char* dataBuffer, 
										char** scopeList, 
										UInt32* scopeListLen, 
										char** url, 
										UInt32* urlLen, 
										char** attributeListPtr, 
										UInt32* attributeListLen );

OSStatus GetSLPScopeListFromBuffer( 	char* dataBuffer,
                                        char** scopeList,
                                        UInt32* scopeListLen );

OSStatus GetSLPDALoggingLevelFromBuffer( 	char* dataBuffer,
                                            UInt32* loggingLevel );

OSStatus GetSLPDAStatusFromBuffer( char* dataBufer, SLPDAStatus* daStatus );

#endif
