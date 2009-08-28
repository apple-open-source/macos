/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#ifndef __SMBAUTH_H__
#define	__SMBAUTH_H__		1

#include <openssl/des.h>

/* utility functions prototypes */
#ifdef __cplusplus
extern "C" {
#endif

	void pwsf_CalculateSMBNTHash(const char *utf8Password, unsigned char outHash[16]);
	void pwsf_CalculateSMBLANManagerHash(const char *password, unsigned char outHash[16]);
	void pwsf_CalculateWorkstationCredentialSessKey( const unsigned char inNTHash[16], const char serverChallenge[8], const char clientChallenge[8], unsigned char outWCSK[8] );
	void pwsf_CalculateWorkstationCredentialStrongSessKey( const unsigned char inNTHash[16], const char serverChallenge[8], const char clientChallenge[8], unsigned char outWCSK[16] );
	void pwsf_CalculatePPTPSessionKeys( const unsigned char inNTHash[16], const unsigned char inNTResponse[24], int inSessionKeyLen, unsigned char *outSendKey, unsigned char *outReceiveKey );
	void pwsf_GetMasterKey( const unsigned char inNTHashHash[16], const unsigned char inNTResponse[24], unsigned char outMasterKey[16] );
	void pwsf_GetAsymetricStartKey( unsigned char inMasterKey[16], unsigned char *outSessionKey, int inSessionKeyLen, bool inIsSendKey, bool inIsServer );
	void pwsf_CalculateP24(unsigned char *P21, unsigned char *C8, unsigned char *P24);
	void pwsf_DESEncodeV1(const void *str, void *data);
	int32_t pwsf_LittleEndianCharsToInt32( const char *inCharPtr );
	void pwsf_LittleEndianUnicodeToUnicode(const u_int16_t *unistr, int unistrLen, u_int16_t *unicode);
	void pwsf_MD4Encode(unsigned char *output, const unsigned char *input, unsigned int len);
	void pwsf_strnupper(char *str, int maxlen);
	void pwsf_CStringToUnicode(char *cstr, u_int16_t *unicode);

#ifdef __cplusplus
};
#endif

#endif
