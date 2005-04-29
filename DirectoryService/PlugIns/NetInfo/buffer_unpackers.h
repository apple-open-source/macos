/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#ifndef __BUFFER_UNPACKERS_H__
#define __BUFFER_UNPACKERS_H__

#include "CNiPlugIn.h"

sInt32 Get2FromBuffer					(	tDataBufferPtr inAuthData,
											tDataList **inOutDataList,
											char **inOutItemOne,
											char **inOutItemTwo,
											unsigned int *outItemCount );

sInt32 UnpackSambaBufferFirstThreeItems	(	tDataBufferPtr inAuthData,
											tDataListPtr *outDataList,
											char **outUserName, 
											unsigned char *outChallenge,
											unsigned long *outChallengeLen,
											unsigned char **outResponse,
											unsigned long *outResponseLen );

sInt32 UnpackSambaBuffer				(	tDataBufferPtr inAuthData,
											char **outUserName, 
											unsigned char *outC8,
											unsigned char *outP24 );

sInt32 UnpackNTLMv2Buffer				(	tDataBufferPtr inAuthData,
											char **outNIName,
											unsigned char *outChal,
											unsigned char **outDigest,
											unsigned long *outDigestLen,
											char **outSambaName,
											char **outDomain );

sInt32 UnpackMSCHAPv2Buffer				(	tDataBufferPtr inAuthData,
											char **outNIName,
											unsigned char *outChal,
											unsigned char **outPeerChal,
											unsigned char **outDigest,
											unsigned long *outDigestLen,
											char **outSambaName);

sInt32 UnpackMPPEKeyBuffer				(	tDataBufferPtr inAuthData,
											char **outUserName,
											unsigned char *outP24,
											int *outKeySize );

sInt32 UnpackDigestBuffer				(	tDataBufferPtr inAuthData,
											char **outUserName,
											digest_context_t *digestContext );

sInt32 UnpackCramBuffer					(	tDataBufferPtr inAuthData,
											char **outUserName,
											char **outChal,
											unsigned char **outResponse,
											unsigned long *outResponseLen );

sInt32 UnpackAPOPBuffer					(	tDataBufferPtr inAuthData,
											char **outUserName,
											char **outChal,
											char **outResponse );

sInt32 RepackBufferForPWServer			(	tDataBufferPtr inBuff,
											const char *inUserID,
											unsigned long inUserIDNodeNum,
											tDataBufferPtr *outBuff );
											
sInt32 GetUserNameFromAuthBuffer		(	tDataBufferPtr inAuthData,
											unsigned long inUserNameIndex, 
											char **outUserName );

#endif
