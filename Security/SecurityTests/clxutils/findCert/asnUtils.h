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
 * asnUtils.h 
 *
 * Created 20 May 2004 by Doug Mitchell.
 */
#ifndef _ASN_UTILS_H_
#define _ASN_UTILS_H_

#include <Security/cssmtype.h>
#include <Security/nameTemplates.h>

#ifdef  __cplusplus
extern "C" {
#endif

unsigned nssArraySize(
    const void **array);

bool compareCssmData(
    const CSSM_DATA *data1,
    const CSSM_DATA *data2);

void printData(
    const CSSM_DATA *cd);

void printString(
    const CSSM_DATA *str);

void printAtv(
    const NSS_ATV *atv);

void printName(
    const char *title,
    unsigned char *name,
    unsigned nameLen);

/*
 * Print subject and/or issuer of a cert.
 */
typedef enum {
    NameBoth = 0,
    NameSubject,
    NameIssuer
} WhichName;

void printCertName(
    const unsigned char *cert,
    unsigned certLen,
    WhichName whichName);

#ifdef __cplusplus
}
#endif

#endif  /* _ASN_UTILS_H_ */
