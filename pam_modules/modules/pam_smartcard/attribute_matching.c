/******************************************************************
 * The purpose of this module is to implement
 * attribute based matching for smartcard and user account
 ******************************************************************/

#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/CFURLAccess.h>
#include <CoreFoundation/CFPropertyList.h>
#include <Security/Security.h>
#include <Security/SecCertificatePriv.h>
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include "Common.h"
#include "scmatch_evaluation.h"

// These are keys into the dictionary read from the config file
#define kCACUserIDKeyFields				CFSTR("fields")
#define kCACUserIDKeyFormatString		CFSTR("formatString")
#define kCACUserIDDSAttributeString		CFSTR("dsAttributeString")		// e.g. dsAttrTypeStandard:RealName (kDS1AttrDistinguishedName)

// This is a key into the dictionary returned by examining the certificate, along with kCACUserIDDSAttributeString
#define kCACUserIDTargetSearchString	CFSTR("targetSearchString")	// i.e. the value in the directory to search for, e.g. "John Smith"

// The names in the brackets match the values in SecurityInterface/lib/CertificateStrings.h, e.g. [GNT_NT_PRINCIPAL_NAME_STR]
// Fields in the subjectAltName
#define kCUIKeyRFC822Name				CFSTR("RFC 822 Name")			// e.g. smith@navy.mil			[GNT_RFC_822_NAME_STR]
#define kCUIKeyNTPrincipalName			CFSTR("NT Principal Name")		// e.g. 0123456789@mil			[GNT_NT_PRINCIPAL_NAME_STR]

// Fields in the Subject Name
#define kCUIKeyCommonName				CFSTR("Common Name")			// e.g. SMITH.JOHN.Q.0123456789	[COMMON_NAME_STR]
#define kCUIKeyOrgUnit					CFSTR("OrganizationalUnit:1")	// e.g. USN						[ORG_UNIT_NAME_STR]
#define kCUIKeyOrgUnit2					CFSTR("OrganizationalUnit:2")	// e.g. PKI
#define kCUIKeyOrgUnit3					CFSTR("OrganizationalUnit:3")	// e.g. DoD
#define kCUIKeyOrganization				CFSTR("Organization")			// e.g. U.S. Government			[ORGANIZATION_NAME_STR]
#define kCUIKeyCountry					CFSTR("Country")				// e.g. US						[COUNTRY_NAME_STR]

#define CAC_CONFIG_FILE CFSTR("/etc/cacloginconfig.plist")
#define NT_PRICIPAL_OID  CFSTR("1.3.6.1.4.1.311.20.2.3")
#define GNT_RFC822_LABEL CFSTR("Email Address")

CFPropertyListRef copyConfigFileContent()
{
    CFPropertyListRef propertyList = NULL;
    CFReadStreamRef stream;
    CFErrorRef error = NULL;
    
    CFURLRef fileURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, CAC_CONFIG_FILE, kCFURLPOSIXPathStyle, false);
    if (!fileURL)
        return NULL;
    
    stream = CFReadStreamCreateWithFile(kCFAllocatorDefault, fileURL);
    if (!stream)
        goto cleanup;
    
    if (CFReadStreamOpen(stream))
    {
        propertyList = CFPropertyListCreateWithStream(kCFAllocatorDefault, stream, 0, kCFPropertyListImmutable, NULL, &error);
        CFReadStreamClose(stream);
    }
    CFRelease(stream);
    
    if (error)
    {
        CFRelease(error);
        goto cleanup;
    }

	if (propertyList == NULL)
	{
		goto cleanup;
	}

    // now check if config file is valid
    bool configValid;
    
    if (CFDictionaryGetCount(propertyList) == 0)
        configValid = false;
    else if (CFDictionaryContainsKey(propertyList, kCACUserIDKeyFields) == false)
        configValid = false;
    else if (CFDictionaryContainsKey(propertyList, kCACUserIDKeyFormatString) == false)
        configValid = false;
    else if (CFDictionaryContainsKey(propertyList, kCACUserIDDSAttributeString) == false)
        configValid = false;
    else
        configValid = true;
    
    if (!configValid)
    {
        
        if (propertyList)
        {
            CFRelease(propertyList);
            propertyList = NULL;
        }
    }
    
cleanup:
    CFReleaseSafe(fileURL);    
    return propertyList;
}

CFTypeRef getSectionData(CFArrayRef values, CFStringRef label)
{
    if (!values || CFGetTypeID(values) != CFArrayGetTypeID())
        return NULL;
    
    for (CFIndex i = 0; i < CFArrayGetCount(values); ++i)
    {
        CFDictionaryRef item = CFArrayGetValueAtIndex(values, i);
        if (CFGetTypeID(item) != CFDictionaryGetTypeID())
            continue;
        
        CFStringRef itemLabel = CFDictionaryGetValue(item, kSecPropertyKeyLabel);
        if (itemLabel && CFStringCompare(itemLabel, label, 0) == kCFCompareEqualTo)
            return CFDictionaryGetValue(item, kSecPropertyKeyValue);
    }
    return NULL;
}

CFDictionaryRef copyCertificateDetails(SecCertificateRef cert)
{
    CFDictionaryRef certDetails = SecCertificateCopyValues(cert, NULL, NULL);
    if (!certDetails)
        return NULL;
    
    CFTypeRef subjectName = CFDictionaryGetValue(certDetails, kSecOIDX509V1SubjectName);
    if (subjectName)
        subjectName = CFDictionaryGetValue(subjectName, kSecPropertyKeyValue);
    
    CFTypeRef altName = CFDictionaryGetValue(certDetails, kSecOIDSubjectAltName);
    if (altName)
        altName = CFDictionaryGetValue(altName, kSecPropertyKeyValue);
    
    CFMutableDictionaryRef result = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!result)
        goto cleanup;
    
    // process supported subjectName fields
    if (subjectName)
    {
        CFTypeRef value;
        
        value = getSectionData(subjectName, kSecOIDCountryName);
        if (value)
            CFDictionarySetValue(result, kCUIKeyCountry, value);
        
        value = getSectionData(subjectName, kSecOIDOrganizationName);
        if (value)
            CFDictionarySetValue(result, kCUIKeyOrganization, value);
        
        value = getSectionData(subjectName, kSecOIDCommonName);
        if (value)
            CFDictionarySetValue(result, kCUIKeyCommonName, value);
        
        value = getSectionData(subjectName, kSecOIDOrganizationalUnitName);
        if (value)
        {
            if (CFGetTypeID(value) == CFStringGetTypeID())
                CFDictionarySetValue(result, kCUIKeyOrgUnit, value);
            else if (CFGetTypeID(value) == CFArrayGetTypeID())
            {
                CFIndex len = CFArrayGetCount(value);
                if (len > 0)
                    CFDictionarySetValue(result, kCUIKeyOrgUnit, CFArrayGetValueAtIndex(value, 0));
                if (len > 1)
                    CFDictionarySetValue(result, kCUIKeyOrgUnit2, CFArrayGetValueAtIndex(value, 1));
                if (len > 2)
                    CFDictionarySetValue(result, kCUIKeyOrgUnit3, CFArrayGetValueAtIndex(value, 2));
            }
        }
    }
    
    // process supported altName fields
    if (altName)
    {
        CFTypeRef value;
        
        value = getSectionData(altName, NT_PRICIPAL_OID);
        if (value)
            CFDictionarySetValue(result, kCUIKeyNTPrincipalName, value);
        
        value = getSectionData(altName, GNT_RFC822_LABEL);
        if (value)
            CFDictionarySetValue(result, kCUIKeyRFC822Name, value);
    }
    
cleanup:
    CFReleaseSafe(certDetails);
    return result;
}



CFDictionaryRef createUserSearchKey(SecCertificateRef certificate)
{
    // Returns a dictionary with 2 elements:
    //		Search string: 0123456789@navy.mil
    //		user lookup string: dsAttrTypeNative:MyUserIdentifier
    
    CFStringRef valueString;
    CFStringRef tagString;
    CFMutableDictionaryRef result = NULL;
    
    CFDictionaryRef configFile = copyConfigFileContent();
    if (!configFile)
        return NULL;
    
    CFDictionaryRef values = copyCertificateDetails(certificate);
    
    CFArrayRef userSearchValues = CFDictionaryGetValue(configFile, kCACUserIDKeyFields);
    CFMutableStringRef formatString = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFDictionaryGetValue(configFile, kCACUserIDKeyFormatString));
    if (!formatString)
        goto cleanup;
    
    CFStringRef userLookupString = CFDictionaryGetValue(configFile, kCACUserIDDSAttributeString);
    
    for (CFIndex i = 0; i < CFArrayGetCount(userSearchValues); ++i)
    {
        tagString = CFArrayGetValueAtIndex(userSearchValues, i);
        valueString = CFDictionaryGetValue(values, tagString);
        if (valueString)
        {
            CFStringRef replaceString = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("$%d"), (int)(i + 1));
            CFStringFindAndReplace(formatString, replaceString, valueString, CFRangeMake(0, CFStringGetLength(formatString)), 0);
            CFRelease(replaceString);
        }
    }
    
    result = CFDictionaryCreateMutable(kCFAllocatorDefault, 2, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (result)
    {
        CFDictionarySetValue(result, kCACUserIDTargetSearchString, formatString);
        CFDictionarySetValue(result, kCACUserIDDSAttributeString, userLookupString);
    }
cleanup:
    CFReleaseSafe(values);
    CFReleaseSafe(formatString);
    CFRelease(configFile);
    return result;
}

bool isNonRepudiated(SecCertificateRef cert)
{
    bool result = false;
    
    CFDictionaryRef certDetails = SecCertificateCopyValues(cert, NULL, NULL);
    if (!certDetails)
        return result;

    // the most correct way to do this is to get key ACL and look if it needs PIN, but since getACL for smartcards record is hardcoded,
    // we are unable to distinguish between records on ACL basis. Using key name is also not a proper way because it is based on tokend
    // implementation
    // please note that the fact certificate itself has no kSecKeyUsageNonRepudiation bit set does not imply anything because "PIN every time" policy
    // is enforced by smartcard and not by the certificate. So let's hope the cards are made the correct way and certificates have
    // kSecKeyUsageNonRepudiation set, like DHS cards do.

    CFDictionaryRef usageDict = CFDictionaryGetValue(certDetails, kSecOIDKeyUsage);
    if (usageDict)
    {
        CFNumberRef usage = CFDictionaryGetValue(usageDict, kSecPropertyKeyValue);
        if (usage)
        {
            uint32_t usageBits;
            CFNumberGetValue(usage, kCFNumberSInt32Type, &usageBits);
            if (usageBits & kSecKeyUsageNonRepudiation)
            {
                result = true;
            }
        }
    }
    CFRelease(certDetails);
    return result;
}

SecKeychainRef copyAttributeMatchedKeychain(ODRecordRef odRecord, CFArrayRef identities, SecIdentityRef* returnedIdentity)
{
    CFDictionaryRef dict;
    
    if (identities == NULL)
        return NULL;

    SecKeychainRef keychainCandidate = NULL;
    SecIdentityRef identityCandidate = NULL;
    bool isNonRepu;
    SecKeychainRef keychain = NULL;
    for (CFIndex i = 0; i < CFArrayGetCount(identities); ++i)
    {
        SecIdentityRef identity = (SecIdentityRef)CFArrayGetValueAtIndex(identities, i);
        SecCertificateRef candidate;
        OSStatus status = SecIdentityCopyCertificate(identity, &candidate);
        if (status != errSecSuccess)
            continue;

        dict = createUserSearchKey(candidate);
        isNonRepu = isNonRepudiated(candidate);
		status = SecKeychainItemCopyKeychain((SecKeychainItemRef)candidate, &keychain);
		CFReleaseNull(candidate);

        if (status == errSecSuccess && dict)
        {
            // find user for this pair
            CFStringRef expectedValue = (CFStringRef) CFDictionaryGetValue(dict, kCACUserIDTargetSearchString);
            CFStringRef attributeName = (CFStringRef) CFDictionaryGetValue(dict, kCACUserIDDSAttributeString);
            CFStringRef value = NULL;
            int res = od_record_attribute_create_cfstring(odRecord, attributeName, &value);
            bool match = (res == 0) && (value) && (CFStringCompare(expectedValue, value, 0) == kCFCompareEqualTo);
            CFRelease(dict);

            if (match == true)
            {
                // check if this certificate is non-repudiated
                // if so, keep it as a candidate for the case we won't find better one
                if (!isNonRepu)
                {
                    CFReleaseSafe(keychainCandidate); // no need to retain candidate
                    if (returnedIdentity)
                        *returnedIdentity = identity;
                    return keychain;
                }
                else
                {
                    if (keychainCandidate == NULL)
                    {
                        CFRetain(keychain);
                        keychainCandidate = keychain;
                        identityCandidate = identity;
                    }
                    else
                    {
                        // not interested in additional candidates
                        CFReleaseSafe(keychainCandidate);
                    }
                }
            }
        }
		CFReleaseNull(keychain);
    }

    // if we got here, all available certificates were either non-repu or non-matching
    if (keychainCandidate)
    {
        if (returnedIdentity)
            *returnedIdentity = identityCandidate;
        return keychainCandidate;
    }

    return NULL;
}
