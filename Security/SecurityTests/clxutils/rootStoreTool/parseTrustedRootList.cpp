/*
 * parseTrustedRootList.cpp - parse the contents of a TrustedRootList record.
 *
 * Created May 26 2005 by dmitch. 
 */
 
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include "parseTrustedRootList.h"
#include "rootUtils.h"

#include <Security/TrustSettingsSchema.h>		/* private header */
#include <Security/SecTrustSettings.h>			
#include <CoreFoundation/CoreFoundation.h>
#include <security_utilities/cfutilities.h>

/*
 * Data is obtained from a SecKeychainItemRef; it's expected to be the XML encoding
 * of a CFPropertyList (specifically of a CFDictionaryRef).
 */
int parseTrustedRootList(
	CFDataRef plistData)
{
	/* First decode the XML */
	CFStringRef errStr = NULL;
	CFRef<CFPropertyListRef> rawPropList;
	int ourRtn = 0;
	OidParser parser;
	
	rawPropList.take(CFPropertyListCreateFromXMLData(
		NULL,
		plistData,
		kCFPropertyListImmutable,
		&errStr));
	CFPropertyListRef cfRawPropList = rawPropList;
	if(cfRawPropList == NULL) {
		printf("***parseTrustedRootList: Error decoding TrustedRootList XML data\n");
		if(errStr != NULL) {
			printf("Error string: "); CFShow(errStr);
			CFRelease(errStr);
		}
		return -1;
	}
	if(errStr != NULL) {
		CFRelease(errStr);
	}
	
	CFDictionaryRef topDict = (CFDictionaryRef)cfRawPropList;
	if(CFGetTypeID(topDict) != CFDictionaryGetTypeID()) {
		printf("***parseTrustedRootList: malformed propList");
		return -1;
	}

	printf("=== Parsed User Trust Record ===\n");
	
	/* that dictionary has two entries */
	CFNumberRef cfVers = (CFNumberRef)CFDictionaryGetValue(topDict, kTrustRecordVersion);
	if((cfVers == NULL) || (CFGetTypeID(cfVers) != CFNumberGetTypeID())) {
		printf("***parseTrustedRootList: malformed version");
	}
	else {
		SInt32 vers;
		if(!CFNumberGetValue(cfVers, kCFNumberSInt32Type, &vers)) {
			printf("***parseTrustedRootList: malformed version");
		}
		else {
			printf("Version = %ld\n", vers);
		}
	}
	
	CFDictionaryRef certsDict = (CFDictionaryRef)CFDictionaryGetValue(topDict, 
		kTrustRecordTrustList);
	if((certsDict == NULL) || (CFGetTypeID(certsDict) != CFDictionaryGetTypeID())) {
		printf("***parseTrustedRootList: malformed mTrustArray");
		return -1;
	}
	
	CFIndex numCerts = CFDictionaryGetCount(certsDict);
	const void *dictKeys[numCerts];
	const void *dictValues[numCerts];
	CFDictionaryGetKeysAndValues(certsDict, dictKeys, dictValues);

	CFDataRef certApp;
	CFDataRef certPolicy;
	CFDictionaryRef ucDict;
	CFArrayRef usageConstraints;
	CFDataRef cfd;
	CFIndex numUsageConstraints;
	CFStringRef policyStr;
	CFNumberRef cfNum;
	CFDateRef modDate;
	
	printf("Number of cert entries: %ld\n", numCerts);
	
	for(CFIndex dex=0; dex<numCerts; dex++) {
		printf("Cert %ld:\n", dex);
		indentIncr();
		
		/* per-cert key is ASCII representation of SHA1(cert) */
		CFStringRef certHashStr = (CFStringRef)dictKeys[dex];
		if(CFGetTypeID(certHashStr) != CFStringGetTypeID()) {
			printf("***parseTrustedRootList: malformed certsDict key");
			ourRtn = -1;
			goto nextCert;
		}
		indent(); printf("Cert Hash              : ");
		printCfStr(certHashStr);
		printf("\n");
	
		/* get per-cert dictionary */
		CFDictionaryRef certDict = (CFDictionaryRef)dictValues[dex];
		if(CFGetTypeID(certDict) != CFDictionaryGetTypeID()) {
			printf("***parseTrustedRootList: malformed certDict");
			ourRtn = -1;
			goto nextCert;
		}
		
		/* 
		 * That dictionary has exactly four entries...but the first
 		 * 
		 * First, the issuer. This is in non-normalized form.
		 */
		cfd = (CFDataRef)CFDictionaryGetValue(certDict, kTrustRecordIssuer);
		if(cfd == NULL) {
			printf("***parseTrustedRootList: missing issuer");
			ourRtn = -1;
			goto nextCert;
		}
		if(CFGetTypeID(cfd) != CFDataGetTypeID()) {
			printf("***parseTrustedRootList: malformed issuer");
			ourRtn = -1;
			goto nextCert;
		}
		indent(); 
		if(CFDataGetLength(cfd) == 0) {
			/* that's for a default setting */
			printf("Issuer                 : <none>\n");
		}
		else {
			printf("Issuer                 : \n");
			indentIncr(); printCfName(cfd, parser);
			indentDecr();
		}

		/* Serial number */
		cfd = (CFDataRef)CFDictionaryGetValue(certDict, kTrustRecordSerialNumber);
		if(cfd == NULL) {
			printf("***parseTrustedRootList: missing serial number");
			ourRtn = -1;
			goto nextCert;
		}
		if(CFGetTypeID(cfd) != CFDataGetTypeID()) {
			printf("***parseTrustedRootList: malformed serial number");
			ourRtn = -1;
			goto nextCert;
		}
		indent(); printData("Serial Number          ", cfd, PD_Hex, parser);

		/* modification date */
		modDate = (CFDateRef)CFDictionaryGetValue(certDict, kTrustRecordModDate);
		if(modDate == NULL) {
			printf("***parseTrustedRootList: missing modification date");
			ourRtn = -1;
			goto nextCert;
		}
		if(CFGetTypeID(modDate) != CFDateGetTypeID()) {
			printf("***parseTrustedRootList: malformed modification date");
			ourRtn = -1;
			goto nextCert;
		}
		indent(); 
		printf("Modification Date      : ");
		printCFDate(modDate);
		printf("\n");
		
		/* 
		 * Array of usageConstraint dictionaries - the array itself must be there,
		 * though it might be empty. 
		 */
		usageConstraints = (CFArrayRef)CFDictionaryGetValue(certDict,
				kTrustRecordTrustSettings);
		numUsageConstraints = 0;
		if(usageConstraints != NULL) {
			if(CFGetTypeID(usageConstraints) != CFArrayGetTypeID()) {
				printf("***parseTrustedRootList: malformed Usage Constraints array");
				ourRtn = -1;
				goto nextCert;
			}
		
			numUsageConstraints = CFArrayGetCount(usageConstraints);
		}
		indent(); printf("Num usage constraints  : ");
		if(usageConstraints) {
			printf("%ld\n", numUsageConstraints);
		}
		else {
			printf("<not present>\n");
		}		

		/* grind thru the usageConstraint dictionaries */
		for(CFIndex apDex=0; apDex<numUsageConstraints; apDex++) {
			indent(); printf("Usage constraint %ld:\n", apDex);
			indentIncr();
			
			ucDict = (CFDictionaryRef)CFArrayGetValueAtIndex(usageConstraints, apDex);
			if(CFGetTypeID(ucDict) != CFDictionaryGetTypeID()) {
				printf("***parseTrustedRootList: malformed usageConstraint dictionary");
				ourRtn = -1;
				goto nextAp;
			}
			
			/* policy - optional - an OID */
			certPolicy = (CFDataRef)CFDictionaryGetValue(ucDict, kSecTrustSettingsPolicy);
			if(certPolicy != NULL) {
				if(CFGetTypeID(certPolicy) != CFDataGetTypeID()) {
					printf("***parseTrustedRootList: malformed certPolicy");
					ourRtn = -1;
					goto nextAp;
				}
				indent(); printData("Policy OID          ", certPolicy, PD_OID, parser);
			}
			
			/* app - optional - data - opaque */
			certApp = (CFDataRef)CFDictionaryGetValue(ucDict, kSecTrustSettingsApplication);
			if(certApp != NULL) {
				if(CFGetTypeID(certApp) != CFDataGetTypeID()) {
					printf("***parseTrustedRootList: malformed certApp");
					ourRtn = -1;
					goto nextAp;
				}
				indent(); printData("Application         ", certApp, PD_Hex, parser);
			}
			
			/* policy string */
			policyStr = (CFStringRef)CFDictionaryGetValue(ucDict, kSecTrustSettingsPolicyString);
			if(policyStr != NULL) {
				if(CFGetTypeID(policyStr) != CFStringGetTypeID()) {
					printf("***parseTrustedRootList: malformed policyStr");
					ourRtn = -1;
					goto nextAp;
				}
				indent(); printf("Policy String       : ");
				printCfStr(policyStr); printf("\n");
			}

			/* Allowed error */
			cfNum = (CFNumberRef)CFDictionaryGetValue(ucDict, kSecTrustSettingsAllowedError);
			if(cfNum != NULL) {
				if(CFGetTypeID(cfNum) != CFNumberGetTypeID()) {
					printf("***parseTrustedRootList: malformed allowedError");
					ourRtn = -1;
					goto nextAp;
				}
				indent(); printf("Allowed Error       : ");
				printCssmErr(cfNum); printf("\n");
			}

			/* ResultType */
			cfNum = (CFNumberRef)CFDictionaryGetValue(ucDict, kSecTrustSettingsResult);
			if(cfNum != NULL) {
				if(CFGetTypeID(cfNum) != CFNumberGetTypeID()) {
					printf("***parseTrustedRootList: malformed Result");
					ourRtn = -1;
					goto nextAp;
				}
				indent(); printf("Result Type         : ");
				printResult(cfNum); printf("\n");
			}

			/* key usage */
			cfNum = (CFNumberRef)CFDictionaryGetValue(ucDict, kSecTrustSettingsKeyUsage);
			if(cfNum != NULL) {
				if(CFGetTypeID(cfNum) != CFNumberGetTypeID()) {
					printf("***parseTrustedRootList: malformed keyUsage");
					ourRtn = -1;
					goto nextAp;
				}
				indent(); printf("Key Usage           : ");
				printKeyUsage(cfNum); printf("\n");
			}

		nextAp:
			indentDecr();
		}
		
	nextCert:
		indentDecr();
	} /* for each cert dictionary  in top-level array */
	
	printf("=== End of Parsed User Trust Record ===\n");
	return ourRtn;

}


