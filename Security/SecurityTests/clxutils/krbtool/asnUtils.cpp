/*
 * Copyright (c) 2004,2006 Apple Computer, Inc. All Rights Reserved.
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
 * asnUtils.cpp - ASN.1-related utilities.
 *
 * Created 20 May 2004 by Doug Mitchell.
 */
#include "asnUtils.h"
#include <Security/nameTemplates.h>
#include <Security/SecAsn1Coder.h>
#include <string.h>
#include <stdio.h>
#include <Security/Security.h>
#include <Security/oidsattr.h>
#include <security_cdsa_utils/cuCdsaUtils.h>

static CSSM_CL_HANDLE gClHand = 0;

static CSSM_CL_HANDLE getClHand()
{
    if(gClHand) {
	return gClHand;
    }
    gClHand = cuClStartup();
    return gClHand;
}

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

bool compareCssmData(
    const CSSM_DATA *data1,
    const CSSM_DATA *data2)
{	
    if((data1 == NULL) || (data1->Data == NULL) || 
       (data2 == NULL) || (data2->Data == NULL) ||
       (data1->Length != data2->Length)) {
	return false;
    }
    if(data1->Length != data2->Length) {
	return false;
    }
    if(memcmp(data1->Data, data2->Data, data1->Length) == 0) {
	return true;
    }
    else {
	return false;
    }
}

void printString(
    const CSSM_DATA *str)
{
    unsigned i;
    char *cp = (char *)str->Data;
    for(i=0; i<str->Length; i++) {
	printf("%c", *cp++);
    }
    printf("\n");
}

void printData(
    const CSSM_DATA *cd)
{
    for(unsigned dex=0; dex<cd->Length; dex++) {
	printf("%02X", cd->Data[dex]);
	if((dex % 4) == 3) {
	    printf(" ");
	}
    }
    printf("\n");
}

/*
 * Print an NSS_ATV
 */
void printAtv(
    const NSS_ATV *atv)
{
    const CSSM_OID *oid = &atv->type;
    const char *fieldName = "Other";
    if(compareCssmData(oid, &CSSMOID_CountryName)) {
	fieldName = "Country       ";      
    }
    else if(compareCssmData(oid, &CSSMOID_OrganizationName)) {
	fieldName = "Org           ";      
    }
    else if(compareCssmData(oid, &CSSMOID_LocalityName)) {
	fieldName = "Locality      ";      
    }
    else if(compareCssmData(oid, &CSSMOID_OrganizationalUnitName)) {
	fieldName = "OrgUnit       ";      
    }
    else if(compareCssmData(oid, &CSSMOID_CommonName)) {
	fieldName = "Common Name   ";      
    }
    else if(compareCssmData(oid, &CSSMOID_Surname)) {
	fieldName = "Surname       ";      
    }
    else if(compareCssmData(oid, &CSSMOID_Title)) {
	fieldName = "Title         ";      
    }
    else if(compareCssmData(oid, &CSSMOID_Surname)) {
	fieldName = "Surname       ";      
    }
    else if(compareCssmData(oid, &CSSMOID_StateProvinceName)) {
	fieldName = "State         ";      
    }
    else if(compareCssmData(oid, &CSSMOID_CollectiveStateProvinceName)) {
	fieldName = "Coll. State   ";      
    }
    else if(compareCssmData(oid, &CSSMOID_EmailAddress)) {
	/* deprecated, used by Thawte */
	fieldName = "Email addrs   ";      
    }
    else {
	fieldName = "Other name    ";      
    }
    printf("      %s : ", fieldName);
    switch(atv->value.tag) {
	case SEC_ASN1_PRINTABLE_STRING:
	case SEC_ASN1_IA5_STRING:	
	case SEC_ASN1_T61_STRING:		// mostly printable....	
	case SEC_ASN1_UTF8_STRING:		// ditto
	    printString(&atv->value.item);
	    break;
	default:
	    printData(&atv->value.item);
	    break;
    }
}

/*
 * Print contents of an encoded Name (e.g. from an IssuerAndSerialNumber).
 */
void printName(
    const char *title,
    unsigned char *name,
    unsigned nameLen)
{
    SecAsn1CoderRef coder;
    if(SecAsn1CoderCreate(&coder)) {
	printf("*****Screwup in SecAsn1CoderCreate\n");
	return;
    }
    CSSM_DATA der = {nameLen, name};
    NSS_Name nssName;
    
    if(SecAsn1DecodeData(coder, &der, kSecAsn1NameTemplate, &nssName)) {
	printf("***Error decoding %s\n", title);
	return;
    }
    printf("   %s:\n", title);
    unsigned numRdns = pkiNssArraySize((const void **)nssName.rdns);
    for(unsigned rdnDex=0; rdnDex<numRdns; rdnDex++) {
	NSS_RDN *rdn = nssName.rdns[rdnDex];
	unsigned numAtvs = pkiNssArraySize((const void **)rdn->atvs);
	for(unsigned atvDex=0; atvDex<numAtvs; atvDex++) {
	    printAtv(rdn->atvs[atvDex]);
	}
    }
}

static void printOneCertName(
    CSSM_CL_HANDLE clHand,
    CSSM_HANDLE cacheHand,
    const char *title,
    const CSSM_OID *oid)
{
    CSSM_HANDLE resultHand = 0;
    CSSM_DATA_PTR field = NULL;
    uint32 numFields;
    CSSM_RETURN crtn;
    
    crtn = CSSM_CL_CertGetFirstCachedFieldValue(clHand, cacheHand,
	oid, &resultHand, &numFields, &field);
    if(crtn) {
	printf("***Error parsing cert\n");
	cssmPerror("CSSM_CL_CertGetFirstCachedFieldValue", crtn);
	return;
    }
    printName(title, field->Data, field->Length);
    CSSM_CL_FreeFieldValue(clHand, oid, field);
}

/*
 * Print subject and/or issuer of a cert.
 */
void printCertName(
    const unsigned char *cert,
    unsigned certLen,
    WhichName whichName)
{
    CSSM_CL_HANDLE clHand = getClHand();
    CSSM_HANDLE cacheHand;
    CSSM_DATA certData = {certLen, (uint8 *)cert};
    CSSM_RETURN crtn;
    bool printSubj = false;
    bool printIssuer = false;
    
    switch(whichName) {
	case NameBoth:
	    printSubj = true;
	    printIssuer = true;
	    break;
	case NameSubject:
	    printSubj = true;
	    break;
	case NameIssuer:
	    printIssuer = true;
	    break;
	default:
	    printf("***BRRZAP! Illegal whichName argument\n");
	    return;
    }
    
    crtn = CSSM_CL_CertCache(clHand, &certData, &cacheHand);
    if(crtn) {
	printf("***Error parsing cert\n");
	cssmPerror("CSSM_CL_CertCache", crtn);
	return;
    }
    
    if(printSubj) {
	printOneCertName(clHand, cacheHand, "Subject", &CSSMOID_X509V1SubjectNameStd);
    }
    if(printIssuer) {
	printOneCertName(clHand, cacheHand, "Issuer", &CSSMOID_X509V1IssuerNameStd);
    }
    CSSM_CL_CertAbortCache(clHand, cacheHand);
    return;
}
