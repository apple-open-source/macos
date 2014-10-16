/*
 * rootUtils.cpp - utility routines for rootStoreTool
 */
 
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include "rootUtils.h"
#include <Security/SecCertificatePriv.h>
#include <Security/SecBasePriv.h>
#include <Security/SecTrustSettings.h>
#include <Security/TrustSettingsSchema.h>		/* private header */
#include <Security/SecAsn1Coder.h>
#include <Security/nameTemplates.h>				/* oh frabjous day */

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

static int indentSize = 0;
void indentIncr(void)	{ indentSize += 3; }
void indentDecr(void)	{ indentSize -= 3; }

void indent(void)
{
	if(indentSize < 0) {
		printf("***indent screwup\n");
		indentSize = 0;
	}
	for (int dex=0; dex<indentSize; dex++) {
		putchar(' ');
	}
}

void printAscii(
	const char *buf,
	unsigned len,
	unsigned maxLen)
{
	bool doEllipsis = false;
	if(len > maxLen) {
		len = maxLen;
		doEllipsis = true;
	}
	for(unsigned dex=0; dex<len; dex++) {
		char c = *buf++;
		if(isalnum(c) || (c == ' ')) {
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
	if(len > maxLen) {
		len = maxLen;
		doEllipsis = true;
	}
	for(unsigned dex=0; dex<len; dex++) {
		printf("%02X ", *buf++);
	}
	if(doEllipsis) {
		printf("...etc.");
	}
}

void printOid(
	const void *buf, 
	unsigned len, 
	OidParser &parser)
{
	char outstr[OID_PARSER_STRING_SIZE];
	parser.oidParse((const unsigned char *)buf, len, outstr);
	printf("%s", outstr);
}

void printData(
	const char *label,
	CFDataRef data,
	PrintDataType whichType,
	OidParser &parser)
{
	const unsigned char *buf = CFDataGetBytePtr(data);
	unsigned len = CFDataGetLength(data);
	
	printf("%s: ", label);
	switch(whichType) {
		case PD_Hex:
			printHex(buf, len, 16);
			break;
		case PD_ASCII:
			printAscii((const char *)buf, len, 50);
			break;
		case PD_OID:
			printOid(buf, len, parser);
	}
	putchar('\n');
}

/* print the contents of a CFString */
void printCfStr(
	CFStringRef cfstr)
{
	CFDataRef strData = CFStringCreateExternalRepresentation(NULL, cfstr,
		kCFStringEncodingUTF8, true);
	if(strData == NULL) {
		printf("<<string decode error>>");
		return;
	}
	const char *cp = (const char *)CFDataGetBytePtr(strData);
	CFIndex len = CFDataGetLength(strData);
	for(CFIndex dex=0; dex<len; dex++) {
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
	printf("%s %d, %ld %02d:%02d",
		month, gregDate.day, gregDate.year, gregDate.hour, gregDate.minute);
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
	printf("%ld", s);
}

/* print a CFNumber as a SecTrustSettingsResult */
void printResult(
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
		case kSecTrustSettingsResultDeny: s = "kSecTrustSettingsResultDeny"; break;
		case kSecTrustSettingsResultUnspecified:    s = "kSecTrustSettingsResultUnspecified"; break;
		default:
			sprintf(bogus, "Unknown SecTrustSettingsResult (%ld)", n);
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

/* print cert's label (the one SecCertificate infers) */
OSStatus printCertLabel(
	SecCertificateRef certRef)
{
	OSStatus ortn;
	CFStringRef label;
	
	ortn = SecCertificateInferLabel(certRef, &label);
	if(ortn) {
		cssmPerror("SecCertificateInferLabel", ortn);
		return ortn;
	}
	printCfStr(label);
	CFRelease(label);
	return noErr;
}

/*
 * How many items in a NULL-terminated array of pointers?
 */
static unsigned nssArraySize(
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

static int compareOids(
	const CSSM_OID *data1,
	const CSSM_OID *data2)
{	
	if((data1 == NULL) || (data1->Data == NULL) || 
	   (data2 == NULL) || (data2->Data == NULL) ||
	   (data1->Length != data2->Length)) {
		return 0;
	}
	if(data1->Length != data2->Length) {
		return 0;
	}
	return memcmp(data1->Data, data2->Data, data1->Length) == 0;
}

static void printRdn(const NSS_RDN *rdn, OidParser &parser)
{
	unsigned numAtvs = nssArraySize((const void **)rdn->atvs);
	char						*fieldName;

	for(unsigned dex=0; dex<numAtvs; dex++) {
		const NSS_ATV *atv = rdn->atvs[dex];
		if(compareOids(&atv->type, &CSSMOID_CountryName)) {
			fieldName = "Country       ";      
		}
		else if(compareOids(&atv->type, &CSSMOID_OrganizationName)) {
			fieldName = "Org           ";      
		}
		else if(compareOids(&atv->type, &CSSMOID_LocalityName)) {
			fieldName = "Locality      ";      
		}
		else if(compareOids(&atv->type, &CSSMOID_OrganizationalUnitName)) {
			fieldName = "OrgUnit       ";      
		}
		else if(compareOids(&atv->type, &CSSMOID_CommonName)) {
			fieldName = "Common Name   ";      
		}
		else if(compareOids(&atv->type, &CSSMOID_Surname)) {
			fieldName = "Surname       ";      
		}
		else if(compareOids(&atv->type, &CSSMOID_Title)) {
			fieldName = "Title         ";      
		}
		else if(compareOids(&atv->type, &CSSMOID_Surname)) {
			fieldName = "Surname       ";      
		}
		else if(compareOids(&atv->type, &CSSMOID_StateProvinceName)) {
			fieldName = "State         ";      
		}
		else if(compareOids(&atv->type, &CSSMOID_CollectiveStateProvinceName)) {
			fieldName = "Coll. State   ";      
		}
		else if(compareOids(&atv->type, &CSSMOID_EmailAddress)) {
			/* deprecated, used by Thawte */
			fieldName = "Email addrs   ";      
		}
		else {
			fieldName = "Other name    ";      
		}
		indent(); printf("%s      : ", fieldName);
		/* Not strictly true here, but we'll just assume we can print everything */
		printAscii((char *)atv->value.item.Data, atv->value.item.Length,
			atv->value.item.Length);
		putchar('\n');
	}
}

/* print a CFData as an X509 Name (i.e., subject or issuer) */
void printCfName(
	CFDataRef nameData,
	OidParser &parser)
{
	SecAsn1CoderRef coder = NULL;
	OSStatus ortn;

	ortn = SecAsn1CoderCreate(&coder);
	if(ortn) {
		cssmPerror("SecAsn1CoderCreate", ortn);
		return;
	}
	/* subsequent errors to errOut: */

	NSS_Name nssName = {NULL};
	unsigned numRdns;

	ortn = SecAsn1Decode(coder, 
		CFDataGetBytePtr(nameData), CFDataGetLength(nameData),
		kSecAsn1NameTemplate,
		&nssName);
	if(ortn) {
		printf("***Error decoding NSS_Name\n");
		goto errOut;
	}	
	numRdns = nssArraySize((const void **)nssName.rdns);
	for(unsigned dex=0; dex<numRdns; dex++) {
		printRdn(nssName.rdns[dex], parser);
	}

errOut:
	if(coder) {
		SecAsn1CoderRelease(coder);
	}
}

