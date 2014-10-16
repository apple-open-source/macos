/*
 * Copyright (c) 2000-2004,2013-2014 Apple Inc. All Rights Reserved.
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
 *
 * cssmapple.h -- CSSM features specific to Apple's Implementation
 */

#ifndef _CSSMAPPLE_H_
#define _CSSMAPPLE_H_  1


#ifdef __cplusplus
extern "C" {
#endif

/* First, an array of bits indicating various status of the cert. */
typedef uint32 CSSM_TP_APPLE_CERT_STATUS;
enum 
{
    CSSM_CERT_STATUS_EXPIRED            = 0x00000001,
    CSSM_CERT_STATUS_NOT_VALID_YET      = 0x00000002,
    CSSM_CERT_STATUS_IS_IN_INPUT_CERTS  = 0x00000004,
    CSSM_CERT_STATUS_IS_IN_ANCHORS      = 0x00000008,
    CSSM_CERT_STATUS_IS_ROOT            = 0x00000010,
    CSSM_CERT_STATUS_IS_FROM_NET        = 0x00000020,
    /* settings found in per-user Trust Settings */
    CSSM_CERT_STATUS_TRUST_SETTINGS_FOUND_USER      = 0x00000040,
    /* settings found in Admin Trust Settings */
    CSSM_CERT_STATUS_TRUST_SETTINGS_FOUND_ADMIN     = 0x00000080,
    /* settings found in System Trust Settings */
    CSSM_CERT_STATUS_TRUST_SETTINGS_FOUND_SYSTEM    = 0x00000100,
    /* Trust Settings result = Trust */
    CSSM_CERT_STATUS_TRUST_SETTINGS_TRUST           = 0x00000200,
    /* Trust Settings result = Deny */
    CSSM_CERT_STATUS_TRUST_SETTINGS_DENY            = 0x00000400,
    /* Per-cert error ignored due to Trust Settings */
    CSSM_CERT_STATUS_TRUST_SETTINGS_IGNORED_ERROR   = 0x00000800
};

typedef struct {
    CSSM_TP_APPLE_CERT_STATUS   StatusBits;
    uint32                      NumStatusCodes;
    CSSM_RETURN                 *StatusCodes;
    
    /* index into raw cert group or AnchorCerts depending on IS_IN_ANCHORS */
    uint32                      Index;   
    
    /* nonzero if cert came from a DLDB */
    CSSM_DL_DB_HANDLE           DlDbHandle;
    CSSM_DB_UNIQUE_RECORD_PTR   UniqueRecord;
} CSSM_TP_APPLE_EVIDENCE_INFO;


#ifdef	__cplusplus
}
#endif	// __cplusplus

#endif /* _CSSMAPPLE_H_ */
