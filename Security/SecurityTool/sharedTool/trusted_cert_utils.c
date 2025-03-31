/*
 * Copyright (c) 2003-2004,2006,2009-2019 Apple Inc. All Rights Reserved.
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
#include "trusted_cert_ssl.h"
#include "SecBase64.h"
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecBasePriv.h>
#include <Security/SecTrustSettings.h>
#if TARGET_OS_OSX
#include <Security/cssmapple.h>
#include <Security/oidsalg.h>
#endif
#include <utilities/fileIo.h>
#include <utilities/SecCFRelease.h>
#include <security_cdsa_utils/cuPem.h>

#define CFRELEASE(cf)    if(cf != NULL) { CFRelease(cf); }

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
	if(cfstr == NULL) {
		printf("<NULL>");
		return;
	}
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
		if (*cp == '\n' || *cp == '\r') {
			printf("\n"); /* handle line breaks */
			cp++;
		} else {
			putchar(*cp++);
		}
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
		printf("<<Malformed CFDateRef>>\n");
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
                        snprintf(bogus, sizeof(bogus), "Unknown SecTrustSettingsResult (%d)", (int)n);
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
	uint32_t n = (uint32_t)s;
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

int readCertFile(
	const char *fileName,
	SecCertificateRef *certRef)
{
    SecCertificateRef localCertRef = NULL;
    CFDataRef dataRef = NULL;
    unsigned char *buf = NULL;
    size_t numBytes;
    int rtn = 0;

    if (readFileSizet(fileName, &buf, &numBytes)) {
        rtn = -1;
        goto errOut;
    }
    dataRef = CFDataCreate(NULL, buf, numBytes);
    localCertRef = SecCertificateCreateWithData(NULL, dataRef);
    if (!localCertRef) {
        localCertRef = SecCertificateCreateWithPEM(NULL, dataRef);
        if (!localCertRef) {
            rtn = -1;
            goto errOut;
        }
    }

errOut:
    free(buf);
    CFRELEASE(dataRef);
    if (certRef) {
        *certRef = localCertRef;
    } else {
        CFRELEASE(localCertRef);
    }
    return rtn;
}


CFOptionFlags revCheckOptionStringToFlags(
	const char *revCheckOption)
{
	CFOptionFlags result = 0;
	if(revCheckOption == NULL) {
		return result;
	}
	else if(!strcmp(revCheckOption, "ocsp")) {
		result |= kSecRevocationOCSPMethod;
	}
	else if(!strcmp(revCheckOption, "crl")) {
		fprintf(stderr, "Warning: crl option is deprecated, use ocsp\n");
		result |= kSecRevocationCRLMethod;
	}
	else if(!strcmp(revCheckOption, "require")) {
		result |= kSecRevocationRequirePositiveResponse;
	}
	else if(!strcmp(revCheckOption, "offline")) {
		result |= kSecRevocationNetworkAccessDisabled;
	}
	else if(!strcmp(revCheckOption, "online")) {
		result |= kSecRevocationOnlineCheck;
	}
	return result;
}

static size_t print_buffer_pem(FILE *stream,
	const char *pem_name, size_t length, const uint8_t *bytes)
{
	size_t pem_name_len = strlen(pem_name);
	size_t b64_len = SecBase64Encode2(NULL, length, NULL, 0,
			kSecB64_F_LINE_LEN_USE_PARAM, 64, NULL);
        size_t buffer_len = 33 + 2 * pem_name_len + b64_len;
	char *buffer = malloc(buffer_len);
	char *p = buffer;
	p += snprintf(buffer, buffer_len, "-----BEGIN %s-----\n", pem_name);
	SecBase64Result result;
	p += SecBase64Encode2(bytes, length, p, b64_len,\
			kSecB64_F_LINE_LEN_USE_PARAM, 64, &result);
	if (result) {
		free(buffer);
		return result;
	}
	p += snprintf(p, buffer_len - (p - buffer), "\n-----END %s-----\n", pem_name);
	size_t res = fwrite(buffer, 1, p - buffer, stream);
	fflush(stream);
	bzero(buffer, p - buffer);
	free(buffer);
	return res;
}

void printCertLabel(SecCertificateRef certRef)
{
	CFStringRef label = SecCertificateCopySubjectSummary(certRef);
	printCfStr(label);
	CFReleaseSafe(label);
}

void printCertDescription(SecCertificateRef certRef)
{
	CFStringRef description = CFCopyDescription((CFTypeRef)certRef);
	printCfStr(description);
	CFReleaseSafe(description);
}

void printCertChain(SecTrustRef trustRef, bool printPem, bool printText)
{
    CFArrayRef chain = SecTrustCopyCertificateChain(trustRef);
    if (!chain) {
        return;
    }
	CFIndex idx, count = CFArrayGetCount(chain);
	for (idx = 0; idx < count; idx++) {
		SecCertificateRef cert = (SecCertificateRef)CFArrayGetValueAtIndex(chain, idx);
		fprintf(stdout, " %ld: ", idx);
		printCertLabel(cert);
		fprintf(stdout, "\n    ");
		if (!cert) { continue; }
		printCertDescription(cert);
		fprintf(stdout, "\n");
		if (printPem) {
			print_buffer_pem(stdout, "CERTIFICATE",
					SecCertificateGetLength(cert),
					SecCertificateGetBytePtr(cert));
		}
	}
    CFReleaseNull(chain);
}

