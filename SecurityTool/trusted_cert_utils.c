/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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
 * trusted_cert_utils.c
 */

#include "trusted_cert_utils.h"
#include <Security/SecPolicyPriv.h>
#include <Security/SecBasePriv.h>
#include <Security/SecTrust.h>
#include <Security/SecTrustSettings.h>
#include <Security/cssmapple.h>
#include <Security/oidsalg.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <security_cdsa_utils/cuPem.h>

static int indentSize = 0;
void indentIncr(void)	{ indentSize += 3; }
void indentDecr(void)	{ indentSize -= 3; }

void indent(void)
{
	int dex;
	if(indentSize < 0) {
		/* bug */
		indentSize = 0;
	}
	for (dex=0; dex<indentSize; dex++) {
		putchar(' ');
	}
}

void printAscii(
	const char *buf,
	unsigned len,
	unsigned maxLen)
{
	bool doEllipsis = false;
	unsigned dex;
	if(len > maxLen) {
		len = maxLen;
		doEllipsis = true;
	}
	for(dex=0; dex<len; dex++) {
		char c = *buf++;
		if(isalnum(c)) {
			putchar(c);
		}
		else {
			putchar('.');
		}
		fflush(stdout);
	}
	if(doEllipsis) {
		printf("...etc.");
	}
}

void printHex(
	const unsigned char *buf,
	unsigned len,
	unsigned maxLen)
{
	bool doEllipsis = false;
	unsigned dex;
	if(len > maxLen) {
		len = maxLen;
		doEllipsis = true;
	}
	for(dex=0; dex<len; dex++) {
		printf("%02X ", *buf++);
	}
	if(doEllipsis) {
		printf("...etc.");
	}
}

/* print the contents of a CFString */
void printCfStr(
	CFStringRef cfstr)
{
	CFDataRef strData = CFStringCreateExternalRepresentation(NULL, cfstr,
		kCFStringEncodingUTF8, true);
	CFIndex dex;

	if(strData == NULL) {
		printf("<<string decode error>>");
		return;
	}
	const char *cp = (const char *)CFDataGetBytePtr(strData);
	CFIndex len = CFDataGetLength(strData);
	for(dex=0; dex<len; dex++) {
		putchar(*cp++);
	}
	CFRelease(strData);
}

/* print a CFDateRef */
static const char *months[12] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

void printCFDate(
	CFDateRef dateRef)
{
	CFAbsoluteTime absTime = CFDateGetAbsoluteTime(dateRef);
	if(absTime == 0.0) {
		printf("<<Malformed CFDateeRef>>\n");
		return;
	}
	CFGregorianDate gregDate = CFAbsoluteTimeGetGregorianDate(absTime, NULL);
	const char *month = "Unknown";
	if((gregDate.month > 12) || (gregDate.month <= 0)) {
		printf("Huh? GregDate.month > 11. These amps only GO to 11.\n");
	}
	else {
		month = months[gregDate.month - 1];
	}
	printf("%s %d, %d %02d:%02d",
		month, gregDate.day, (int)gregDate.year, gregDate.hour, gregDate.minute);
}

/* print a CFNumber */
void printCfNumber(
	CFNumberRef cfNum)
{
	SInt32 s;
	if(!CFNumberGetValue(cfNum, kCFNumberSInt32Type, &s)) {
		printf("***CFNumber overflow***");
		return;
	}
	printf("%d", (int)s);
}

/* print a CFNumber as a SecTrustSettingsResult */
void printResultType(
	CFNumberRef cfNum)
{
	SInt32 n;
	if(!CFNumberGetValue(cfNum, kCFNumberSInt32Type, &n)) {
		printf("***CFNumber overflow***");
		return;
	}
	const char *s;
	char bogus[100];
	switch(n) {
		case kSecTrustSettingsResultInvalid: s = "kSecTrustSettingsResultInvalid"; break;
		case kSecTrustSettingsResultTrustRoot: s = "kSecTrustSettingsResultTrustRoot"; break;
		case kSecTrustSettingsResultTrustAsRoot: s = "kSecTrustSettingsResultTrustAsRoot"; break;
		case kSecTrustSettingsResultDeny:    s = "kSecTrustSettingsResultDeny"; break;
		case kSecTrustSettingsResultUnspecified: s = "kSecTrustSettingsResultUnspecified"; break;
		default:
			sprintf(bogus, "Unknown SecTrustSettingsResult (%d)", (int)n);
			s = bogus;
			break;
	}	
	printf("%s", s);
}

/* print a CFNumber as SecTrustSettingsKeyUsage */
void printKeyUsage(
	CFNumberRef cfNum)
{
	SInt32 s;
	if(!CFNumberGetValue(cfNum, kCFNumberSInt32Type, &s)) {
		printf("***CFNumber overflow***");
		return;
	}
	uint32 n = (uint32)s;
	if(n == kSecTrustSettingsKeyUseAny) {
		printf("<any>");
		return;
	}
	else if(n == 0) {
		printf("<none>");
		return;
	}
	printf("< ");
	if(n & kSecTrustSettingsKeyUseSignature) {
		printf("Signature ");
	}
	if(n & kSecTrustSettingsKeyUseEnDecryptData) {
		printf("EnDecryptData ");
	}
	if(n & kSecTrustSettingsKeyUseEnDecryptKey) {
		printf("EnDecryptKey ");
	}
	if(n & kSecTrustSettingsKeyUseSignCert) {
		printf("SignCert ");
	}
	if(n & kSecTrustSettingsKeyUseSignRevocation) {
		printf("SignRevocation ");
	}
	if(n & kSecTrustSettingsKeyUseKeyExchange) {
		printf("KeyExchange ");
	}
	printf(" >");
}

/* print a CFNumber as CSSM_RETURN string */
void printCssmErr(
	CFNumberRef cfNum)
{
	SInt32 s;
	if(!CFNumberGetValue(cfNum, kCFNumberSInt32Type, &s)) {
		printf("***CFNumber overflow***");
		return;
	}
	printf("%s", cssmErrorString((CSSM_RETURN)s));
}

/* convert an OID to a SecPolicyRef */
SecPolicyRef oidToPolicy(
	const CSSM_OID *oid)
{
	OSStatus ortn;
	SecPolicyRef policyRef = NULL;
	
	ortn = SecPolicyCopy(CSSM_CERT_X_509v3, oid, &policyRef);
	if(ortn) {
		cssmPerror("SecPolicyCopy", ortn);
		return NULL;
	}
	return policyRef;
}

typedef struct {
	const CSSM_OID *oid;
	const char *oidStr;
} OidString;

static OidString oidStrings[] = 
{
	{ &CSSMOID_APPLE_ISIGN, "iSign" },
	{ &CSSMOID_APPLE_X509_BASIC, "Apple X509 Basic" },
	{ &CSSMOID_APPLE_TP_SSL, "SSL" },
	{ &CSSMOID_APPLE_TP_SMIME, "SMIME" },
	{ &CSSMOID_APPLE_TP_EAP, "EAP" },
	{ &CSSMOID_APPLE_TP_SW_UPDATE_SIGNING, "SW Update Signing" },
	{ &CSSMOID_APPLE_TP_IP_SEC, "IPSec" },
	{ &CSSMOID_APPLE_TP_ICHAT, "iChat" },
	{ &CSSMOID_APPLE_TP_RESOURCE_SIGN, "Resource Signing" },
	{ &CSSMOID_APPLE_TP_PKINIT_CLIENT, "PKINIT Client" },
	{ &CSSMOID_APPLE_TP_PKINIT_SERVER, "PKINIT Server" },
	{ &CSSMOID_APPLE_TP_CODE_SIGNING, "Code Signing" },
	{ &CSSMOID_APPLE_TP_PACKAGE_SIGNING, "Package Signing" }
};
#define NUM_OID_STRINGS		(sizeof(oidStrings) / sizeof(oidStrings[0]))

/* convert a policy string to a SecPolicyRef */
SecPolicyRef oidStringToPolicy(
	const char *oidStr)
{
	/* OID string to an OID pointer */
	const CSSM_OID *oid = NULL;
	unsigned dex;
	
	for(dex=0; dex<NUM_OID_STRINGS; dex++) {
		OidString *os = &oidStrings[dex];
		if(!strcmp(oidStr, os->oidStr)) {
			oid = os->oid;
			break;
		}
	}
	if(oid == NULL) {
		return NULL;
	}

	/* OID to SecPolicyRef */
	return oidToPolicy(oid);
}

/* CSSM_OID --> OID string */
const char *oidToOidString(
	const CSSM_OID *oid)
{
	unsigned dex;
	static char unknownOidString[200];
	
	for(dex=0; dex<NUM_OID_STRINGS; dex++) {
		OidString *os = &oidStrings[dex];
		if(compareOids(oid, os->oid)) {
			return os->oidStr;
		}
	}
	sprintf(unknownOidString, "Unknown OID length %ld, value { ", oid->Length);
	for(dex=0; dex<oid->Length; dex++) {
		char tmp[6];
		sprintf(tmp, "%02X ", oid->Data[dex]);
		strcat(unknownOidString, tmp);
	}
	strcat(unknownOidString, " }"); 
	return unknownOidString;
}

/* compare OIDs; returns 1 if identical, else returns 0 */
int compareOids(
	const CSSM_OID *oid1,
	const CSSM_OID *oid2)
{
	if((oid1 == NULL) || (oid2 == NULL)) {
		return 0;
	}
	if(oid1->Length != oid2->Length) {
		return 0;
	}
	if(memcmp(oid1->Data, oid2->Data, oid1->Length)) {
		return 0;
	}
	return 1;
}

/* app path string to SecTrustedApplicationRef */
SecTrustedApplicationRef appPathToAppRef(
	const char *appPath)
{
	SecTrustedApplicationRef appRef = NULL;
	OSStatus ortn;
	
	if(appPath == NULL) {
		return NULL;
	}
	ortn = SecTrustedApplicationCreateFromPath(appPath, &appRef);
	if(ortn) {
		cssmPerror("SecTrustedApplicationCreateFromPath", ortn);
		return NULL;
	}
	return appRef;
}

int readCertFile(
	const char *fileName,
	SecCertificateRef *certRef)
{
	unsigned char *cp = NULL;
	unsigned len = 0;
	CSSM_DATA certData;
	OSStatus ortn;
	unsigned char *decoded = NULL;
	unsigned decodedLen = 0;

	if(readFile(fileName, &cp, &len)) {
		printf("***Error reading file %s\n", fileName);
		return -1;
	}
	if(isPem(cp, len)) {
		if(pemDecode(cp, len, &decoded, &decodedLen)) {
			fprintf(stderr, "Error decoding cert file %s\n", fileName);
			return -1;
		}
		certData.Length = decodedLen;
		certData.Data = decoded;
	}
	else {
		certData.Length = len;
		certData.Data = cp;
	}
	ortn = SecCertificateCreateFromData(&certData, 
			CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_DER, certRef);
	free(cp);
	if(decoded) {
		free(decoded);
	}
	if(ortn) {
		cssmPerror("SecCertificateCreateFromData", ortn);
		return -1;
	}
	return 0;
}

/* policy string --> CSSM_OID */
const CSSM_OID *policyStringToOid(
	const char *policy)
{
	if(policy == NULL) {
		return NULL;
	}
	if(!strcmp(policy, "ssl")) {
		return &CSSMOID_APPLE_TP_SSL;
	}
	else if(!strcmp(policy, "smime")) {
		return &CSSMOID_APPLE_TP_SMIME;
	}
	else if(!strcmp(policy, "codeSign")) {
		return &CSSMOID_APPLE_TP_CODE_SIGNING;
	}
	else if(!strcmp(policy, "IPSec")) {
		return &CSSMOID_APPLE_TP_IP_SEC;
	}
	else if(!strcmp(policy, "iChat")) {
		return &CSSMOID_APPLE_TP_ICHAT;
	}
	else if(!strcmp(policy, "basic")) {
		return &CSSMOID_APPLE_X509_BASIC;
	}
	else if(!strcmp(policy, "swUpdate")) {
		return &CSSMOID_APPLE_TP_SW_UPDATE_SIGNING;
	}
	else if(!strcmp(policy, "pkgSign")) {
		return &CSSMOID_APPLE_TP_PACKAGE_SIGNING;
	}
	else if(!strcmp(policy, "pkinitClient")) {
		return &CSSMOID_APPLE_TP_PKINIT_CLIENT;
	}
	else if(!strcmp(policy, "pkinitServer")) {
		return &CSSMOID_APPLE_TP_PKINIT_SERVER;
	}
	else if(!strcmp(policy, "eap")) {
		return &CSSMOID_APPLE_TP_EAP;
	}
	else {
		fprintf(stderr, "***unknown policy spec (%s)\n", policy);
		return NULL;
	}
}
