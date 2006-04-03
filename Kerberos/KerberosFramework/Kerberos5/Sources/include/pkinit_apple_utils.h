/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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

/*
 * pkinit_apple_utils.h - PKINIT utilities, Mac OS X version
 *
 * Created 19 May 2004 by Doug Mitchell.
 */
 
#ifndef	_PKINIT_APPLE_UTILS_H_
#define _PKINIT_APPLE_UTILS_H_

#include <Security/SecAsn1Coder.h>
#include "krb5.h"
#include <Security/cssmapple.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PKI_DEBUG
#define PKI_DEBUG   0
#endif
#if PKI_DEBUG

#include <stdio.h>

#define pkiDebug(args...)       printf(args)
#define pkiCssmErr(str, rtn)    cssmPerror(str, rtn)
#else
#define pkiDebug(args...)
#define pkiCssmErr(str, rtn)
#endif

/*
 * Macros used to initialize a declared CSSM_DATA and krb5_data to zero/NULL values.
 */
#define INIT_CDATA(cd)  cd = {0, NULL}
#define INIT_KDATA(kd)  kd = {0, 0, NULL}

/*
 * CSSM_DATA <--> krb5_ui_4
 *
 * dataToInt() returns nonzero on error
 */
int pkiDataToInt(
    const CSSM_DATA *cdata, 
    krb5_int32       *i);	/* RETURNED */

int pkiIntToData(
    krb5_int32	    num,
    CSSM_DATA       *cdata,     /* allocated in coder space and RETURNED */
    SecAsn1CoderRef coder);

/*
 * raw data --> krb5_data
 */
int pkiDataToKrb5Data(
    const void *data,
    unsigned dataLen,
    krb5_data *kd);		/* content mallocd and RETURNED */

/* 
 * CSSM_DATA <--> krb5_data
 *
 * CSSM_DATA data is managed by a SecAsn1CoderRef; krb5_data data is mallocd.
 *
 * Both return nonzero on error.
 */
int pkiCssmDataToKrb5Data(
    const CSSM_DATA *cd, 
    krb5_data *kd);		/* content mallocd and RETURNED */


int pkiKrb5DataToCssm(
    const krb5_data *kd,
    CSSM_DATA       *cdata,     /* allocated in coder space and RETURNED */
    SecAsn1CoderRef coder);

/*
 * Non-mallocing conversion between CSSM_DATA and krb5_data
 */
#define PKI_CSSM_TO_KRB_DATA(cd, kd)    \
    (kd)->data = (char *)(cd)->Data;	\
    (kd)->length = (cd)->Length;

#define PKI_KRB_TO_CSSM_DATA(kd, cd)    \
    (cd)->Data = (uint8 *)(kd)->data;	\
    (cd)->Length = (kd)->length;

/*
 * Compare to CSSM_DATAs. Return TRUE if they're the same else FALSE.
 */
krb5_boolean pkiCompareCssmData(
    const CSSM_DATA *d1,
    const CSSM_DATA *d2);

/* 
 * krb5_timestamp <--> a mallocd string in generalized format
 */
int pkiKrbTimestampToStr(
    krb5_timestamp      kts,
    char		**str);		// mallocd and RETURNED

int pkiTimeStrToKrbTimestamp(
    const char		*str,
    unsigned		len,
    krb5_timestamp      *kts);		// RETURNED

/*
 * Convert an OSStatus to a krb5_error_code
 */
krb5_error_code pkiOsStatusToKrbErr(
    OSStatus		ortn);
    
/*
 * Given a DER encoded certificate, obtain the associated IssuerAndSerialNumber.
 */
krb5_error_code pkiGetIssuerAndSerial(
    const krb5_data *cert,
    krb5_data       *issuer_and_serial);

/*
 * How many items in a NULL-terminated array of pointers?
 */
unsigned pkiNssArraySize(
    const void **array);

#ifdef __cplusplus
}
#endif

#endif  /* _PKINIT_APPLE_UTILS_H_ */

