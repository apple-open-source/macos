/*
	File:		SLPComm.h

	Contains:	IPC calling conventions for communication with slpd

	Written by:	Kevin Arnold

	Copyright:	© 2000 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):


*/

/*
      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |            Code               |            Length             |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |           Status              |                Data           /
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


*/

#ifndef _SLPComm_
#define _SLPComm_
#pragma once

#if __cplusplus
//extern "C" {
#endif

#include <Carbon/Carbon.h>

#define kSLPLoadToolPath	"/System/Library/Frameworks/CoreServices.framework/Frameworks/NSLCore.framework/Resources/NSLPlugins/slpdLoad"
#define kSLPLoadToolCmd		"slpdLoad"
#define kTCPBufferSize		1400
#define kRAdminIPCPath		"/tmp/slpRAdmin_ipc"

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
#if __cplusplus
//}
#endif

#endif


