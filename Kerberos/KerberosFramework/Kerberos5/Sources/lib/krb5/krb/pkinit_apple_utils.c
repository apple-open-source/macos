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
 * pkinit_apple_utils.c - PKINIT utilities, Mac OS X version
 *
 * Created 19 May 2004 by Doug Mitchell at Apple.
 */
 
#include "pkinit_apple_utils.h"
#include "pkinit_asn1.h"
#include <sys/errno.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <Security/Security.h>
/* 
 * Cruft needed to attach to a module
 */
static CSSM_VERSION vers = {2, 0};
static const CSSM_GUID testGuid = { 0xFADE, 0, 0, { 1,2,3,4,5,6,7,0 }};

/*
 * Standard app-level memory functions required by CDSA.
 */
static void * cuAppMalloc (uint32 size, void *allocRef) {
	return( malloc(size) );
}

static void cuAppFree (void *mem_ptr, void *allocRef) {
	free(mem_ptr);
 	return;
}

static void * cuAppRealloc (void *ptr, uint32 size, void *allocRef) {
	return( realloc( ptr, size ) );
}

static void * cuAppCalloc (uint32 num, uint32 size, void *allocRef) {
	return( calloc( num, size ) );
}

static CSSM_API_MEMORY_FUNCS memFuncs = {
	cuAppMalloc,
	cuAppFree,
	cuAppRealloc,
 	cuAppCalloc,
 	NULL
};

/*
 * Init CSSM; returns CSSM_FALSE on error. Reusable.
 */
static CSSM_BOOL cssmInitd = CSSM_FALSE;

static CSSM_BOOL cuCssmStartup()
{
    CSSM_RETURN  crtn;
    CSSM_PVC_MODE pvcPolicy = CSSM_PVC_NONE;
    
    if(cssmInitd) {
	return CSSM_TRUE;
    }  
    crtn = CSSM_Init (&vers, 
	CSSM_PRIVILEGE_SCOPE_NONE,
	&testGuid,
	CSSM_KEY_HIERARCHY_NONE,
	&pvcPolicy,
	NULL /* reserved */);
    if(crtn != CSSM_OK) 
    {
	return CSSM_FALSE;
    }
    else {
	cssmInitd = CSSM_TRUE;
	return CSSM_TRUE;
    }
}

static CSSM_CL_HANDLE cuClStartup()
{
    CSSM_CL_HANDLE clHand;
    CSSM_RETURN crtn;
    
    if(cuCssmStartup() == CSSM_FALSE) {
	return 0;
    }
    crtn = CSSM_ModuleLoad(&gGuidAppleX509CL,
	CSSM_KEY_HIERARCHY_NONE,
	NULL,			// eventHandler
	NULL);			// AppNotifyCallbackCtx
    if(crtn) {
	return 0;
    }
    crtn = CSSM_ModuleAttach (&gGuidAppleX509CL,
	&vers,
	&memFuncs,			// memFuncs
	0,				// SubserviceID
	CSSM_SERVICE_CL,		// SubserviceFlags - Where is this used?
	0,				// AttachFlags
	CSSM_KEY_HIERARCHY_NONE,
	NULL,				// FunctionTable
	0,				// NumFuncTable
	NULL,				// reserved
	&clHand);
    if(crtn) {
	return 0;
    }
    else {
	return clHand;
    }
}

static CSSM_RETURN cuClDetachUnload(
	CSSM_CL_HANDLE  clHand)
{
    CSSM_RETURN crtn = CSSM_ModuleDetach(clHand);
    if(crtn) {
	return crtn;
    }
    return CSSM_ModuleUnload(&gGuidAppleX509CL, NULL, NULL);
}

/*
 * CSSM_DATA <--> krb5_ui_4
 *
 * dataToInt() returns nonzero on error
 */
int pkiDataToInt(
    const CSSM_DATA *cdata, 
    krb5_int32       *i)	/* RETURNED */
{
    krb5_ui_4 len;
    krb5_int32 rtn = 0;
    krb5_ui_4 dex;
    
    if((cdata->Length == 0) || (cdata->Data == NULL)) {
	*i = 0;
	return 0;
    }
    len = cdata->Length;
    if(len > sizeof(krb5_int32)) {
	return -1;
    }
    
    uint8 *cp = cdata->Data;
    for(dex=0; dex<len; dex++) {
	rtn = (rtn << 8) | *cp++;
    }
    *i = rtn;
    return 0;
}

int pkiIntToData(
    krb5_int32	    num,
    CSSM_DATA       *cdata,
    SecAsn1CoderRef coder)
{
    krb5_ui_4 unum = (krb5_ui_4)num;
    uint32 len = 0;
    uint8 *cp = NULL;
    unsigned i;
    
    if(unum < 0x100) {
	len = 1;
    }
    else if(unum < 0x10000) {
	len = 2;
    }
    else if(unum < 0x1000000) {
	len = 3;
    }
    else {
	len = 4;
    }
    if(SecAsn1AllocItem(coder, cdata, len)) {
	return ENOMEM;
    }
    cp = &cdata->Data[len - 1];
    for(i=0; i<len; i++) {
	*cp-- = unum & 0xff;
	unum >>= 8;
    }
    return 0;
}

/*
 * raw data --> krb5_data
 */
int pkiDataToKrb5Data(
    const void *data,
    unsigned dataLen,
    krb5_data *kd)
{
    assert(data != NULL);
    assert(kd != NULL);
    kd->data = (char *)malloc(dataLen);
    if(kd->data == NULL) {
	return ENOMEM;
    }
    kd->length = dataLen;
    memmove(kd->data, data, dataLen);
    return 0;
}

/* 
 * CSSM_DATA <--> krb5_data
 *
 * CSSM_DATA data is managed by a SecAsn1CoderRef; krb5_data data is mallocd.
 *
 * Both return nonzero on error.
 */
int pkiCssmDataToKrb5Data(
    const CSSM_DATA *cd, 
    krb5_data *kd)
{
    assert(cd != NULL);
    return pkiDataToKrb5Data(cd->Data, cd->Length, kd);
}

int pkiKrb5DataToCssm(
    const krb5_data *kd,
    CSSM_DATA       *cd,
    SecAsn1CoderRef coder)
{
    assert((cd != NULL) && (kd != NULL));
    return SecAsn1AllocCopy(coder, kd->data, kd->length, cd);
}

krb5_boolean pkiCompareCssmData(
    const CSSM_DATA *d1,
    const CSSM_DATA *d2)
{
    if((d1 == NULL) || (d2 == NULL)) {
	return FALSE;
    }
    if(d1->Length != d2->Length) {
	return FALSE;
    }
    if(memcmp(d1->Data, d2->Data, d1->Length)) {
	return FALSE;
    }
    else {
	return TRUE;
    }
}

/* 
 * krb5_timestamp --> a mallocd string in generalized format
 */
int pkiKrbTimestampToStr(
    krb5_timestamp kts,
    char **str)		    // mallocd and RETURNED
{
    time_t gmt_time = kts;
    struct tm *utc = gmtime(&gmt_time);
    if (utc == NULL ||
	utc->tm_year > 8099 || utc->tm_mon > 11 ||
	utc->tm_mday > 31 || utc->tm_hour > 23 ||
	utc->tm_min > 59 || utc->tm_sec > 59) {
	return ASN1_BAD_GMTIME;
    }
    char *outStr = (char *)malloc(16);
    sprintf(outStr, "%04d%02d%02d%02d%02d%02dZ",
	utc->tm_year + 1900, utc->tm_mon + 1,
	utc->tm_mday, utc->tm_hour, utc->tm_min, utc->tm_sec);
    *str = outStr;
    return 0;
}

int pkiTimeStrToKrbTimestamp(
    const char		*str,
    unsigned		len,
    krb5_timestamp      *kts)       // RETURNED
{
    char 	szTemp[5];
    unsigned 	x;
    unsigned 	i;
    char 	*cp;
    struct tm	tmp;
    time_t      t;
    
    if(len != 15) {
	return ASN1_BAD_LENGTH;
    }

    if((str == NULL) || (kts == NULL)) {
    	return -1;
    }
  	
    cp = (char *)str;
    memset(&tmp, 0, sizeof(tmp));
    
    /* check that all characters except last are digits */
    for(i=0; i<(len - 1); i++) {
	if ( !(isdigit(cp[i])) ) {
	    return ASN1_BAD_TIMEFORMAT;
	}
    }

    /* check last character is a 'Z' */
    if(cp[len - 1] != 'Z' )	{
	return ASN1_BAD_TIMEFORMAT;
    }
    
    /* YEAR */
    szTemp[0] = *cp++;
    szTemp[1] = *cp++;
    szTemp[2] = *cp++;
    szTemp[3] = *cp++;
    szTemp[4] = '\0';
    x = atoi( szTemp );
    /* by definition - tm_year is year - 1900 */
    tmp.tm_year = x - 1900;

    /* MONTH */
    szTemp[0] = *cp++;
    szTemp[1] = *cp++;
    szTemp[2] = '\0';
    x = atoi( szTemp );
    /* in the string, months are from 1 to 12 */
    if((x > 12) || (x <= 0)) {
	return ASN1_BAD_TIMEFORMAT;
    }
    /* in a tm, 0 to 11 */
    tmp.tm_mon = x - 1;

    /* DAY */
    szTemp[0] = *cp++;
    szTemp[1] = *cp++;
    szTemp[2] = '\0';
    x = atoi( szTemp );
    /* 1..31 */
    if((x > 31) || (x <= 0)) {
	return ASN1_BAD_TIMEFORMAT;
    }
    tmp.tm_mday = x;

    /* HOUR */
    szTemp[0] = *cp++;
    szTemp[1] = *cp++;
    szTemp[2] = '\0';
    x = atoi( szTemp );
    if((x > 23) || (x < 0)) {
	return ASN1_BAD_TIMEFORMAT;
    }
    tmp.tm_hour = x;

    /* MINUTE */
    szTemp[0] = *cp++;
    szTemp[1] = *cp++;
    szTemp[2] = '\0';
    x = atoi( szTemp );
    if((x > 59) || (x < 0)) {
	return ASN1_BAD_TIMEFORMAT;
    }
    tmp.tm_min = x;

    /* SECOND */
    szTemp[0] = *cp++;
    szTemp[1] = *cp++;
    szTemp[2] = '\0';
    x = atoi( szTemp );
    if((x > 59) || (x < 0)) {
	return ASN1_BAD_TIMEFORMAT;
    }
    tmp.tm_sec = x;
    t = timegm(&tmp);
    if(t == -1) {
	return ASN1_BAD_TIMEFORMAT;
    }
    *kts = t;
    return 0;
}

/*
 * Convert an OSStatus to a krb5_error_code
 */
krb5_error_code pkiOsStatusToKrbErr(
    OSStatus ortn)
{
    /* FIXME */
    return (krb5_error_code)ortn;
}

/*
 * Given a DER encoded certificate, obtain the associated IssuerAndSerialNumber.
 */
krb5_error_code pkiGetIssuerAndSerial(
    const krb5_data *cert,
    krb5_data       *issuer_and_serial)
{
    CSSM_HANDLE cacheHand = 0;
    CSSM_RETURN crtn = CSSM_OK;
    CSSM_DATA certData = { cert->length, (uint8 *)cert->data };
    CSSM_HANDLE resultHand = 0;
    CSSM_DATA_PTR derIssuer = NULL;
    CSSM_DATA_PTR serial;
    krb5_data krb_serial;
    krb5_data krb_issuer;
    uint32 numFields;
    
    CSSM_CL_HANDLE clHand = cuClStartup();
    if(clHand == 0) {
	return CSSMERR_CSSM_ADDIN_LOAD_FAILED;
    }
    /* subsequent errors to errOut: */
    
    crtn = CSSM_CL_CertCache(clHand, &certData, &cacheHand);
    if(crtn) {
	pkiCssmErr("CSSM_CL_CertCache", crtn);
	goto errOut;
    }
    
    /* obtain the two fields; issuer is DER encoded */
    crtn = CSSM_CL_CertGetFirstCachedFieldValue(clHand, cacheHand,
	&CSSMOID_X509V1IssuerNameStd, &resultHand, &numFields, &derIssuer);
    if(crtn) {
	pkiCssmErr("CSSM_CL_CertGetFirstCachedFieldValue(issuer)", crtn);
	goto errOut;
    }
    crtn = CSSM_CL_CertGetFirstCachedFieldValue(clHand, cacheHand,
	&CSSMOID_X509V1SerialNumber, &resultHand, &numFields, &serial);
    if(crtn) {
	pkiCssmErr("CSSM_CL_CertGetFirstCachedFieldValue(serial)", crtn);
	goto errOut;
    }
    PKI_CSSM_TO_KRB_DATA(derIssuer, &krb_issuer);
    PKI_CSSM_TO_KRB_DATA(serial, &krb_serial);
    crtn = pkinit_issuer_serial_encode(&krb_issuer, &krb_serial, issuer_and_serial);
    
errOut:
    if(derIssuer) {
	CSSM_CL_FreeFieldValue(clHand, &CSSMOID_X509V1IssuerNameStd, derIssuer);
    }
    if(serial) {
	CSSM_CL_FreeFieldValue(clHand, &CSSMOID_X509V1SerialNumber, serial);
    }
    if(cacheHand) {
	CSSM_CL_CertAbortCache(clHand, cacheHand);
    }
    if(clHand) {
	cuClDetachUnload(clHand);
    }
    return crtn;
}

/*
 * How many items in a NULL-terminated array of pointers?
 */
unsigned pkiNssArraySize(
    const void **array)
{
    unsigned count = 0;
    if (array) {
	while (*array++) {
	    count++;
	}
    }
    return count;
}

