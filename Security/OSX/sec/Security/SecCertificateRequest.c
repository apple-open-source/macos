/*
 * Copyright (c) 2008-2009,2012-2020 Apple Inc. All Rights Reserved.
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
 */

#include <libDER/oids.h>
#include <libDER/DER_Encode.h>

#include <security_asn1/SecAsn1Types.h>
#include <security_asn1/csrTemplates.h>
#include <security_asn1/certExtensionTemplates.h>
#include <security_asn1/secasn1.h>
#include <security_asn1/SecAsn1Types.h>
#include <security_asn1/oidsalg.h>
#include <security_asn1/nameTemplates.h>
#include <security_asn1/SecAsn1TimeUtils.h>

#include <TargetConditionals.h>
#if TARGET_OS_OSX
// ENABLE_CMS 0
OSStatus SecCmsArraySortByDER(void **objs, const SecAsn1Template *objtemplate, void **objs2);
#else
// ENABLE_CMS 1
#include <security_smime/cmspriv.h>
#endif

#include <Security/SecInternal.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecIdentity.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecItem.h>
#include <Security/SecKey.h>
#include <Security/SecRSAKey.h>
#include <Security/SecKeyPriv.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonDigestSPI.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>
#include <Security/SecCMS.h>
#include <utilities/debugging.h>

#if TARGET_OS_IPHONE
#include <Security/SecECKey.h>
#endif

#include <utilities/SecCFWrappers.h>

#include <AssertMacros.h>

#include <Security/SecCertificateRequest.h>
#include <Security/SecCertificateOIDS.h>

/* Subject Name Attribute OID constants */
const CFStringRef kSecOidCommonName = CFSTR("CN");
const CFStringRef kSecOidCountryName = CFSTR("C");
const CFStringRef kSecOidStateProvinceName = CFSTR("ST");
const CFStringRef kSecOidLocalityName = CFSTR("L");
const CFStringRef kSecOidOrganization = CFSTR("O");
const CFStringRef kSecOidOrganizationalUnit = CFSTR("OU");
const CFStringRef kSecOidEmailAddress = CFSTR("EMAIL");
/*
 Note: SecCertificateOIDS.h declares kSecOIDEmailAddress (OID vs. Oid),
 where the OID value is defined in CertificateValues.cpp as follows:
 const CFStringRef kSecOIDEmailAddress = CFSTR("1.2.840.113549.1.9.1");
*/
// keep natural order: C > ST > L > O > OU > CN > EMAIL

/* Type constants */
const unsigned char SecASN1PrintableString = SEC_ASN1_PRINTABLE_STRING;
const unsigned char SecASN1UTF8String = SEC_ASN1_UTF8_STRING;

/* Parameter dictionary keys */
const CFStringRef kSecCSRChallengePassword = CFSTR("csrChallengePassword");
const CFStringRef kSecSubjectAltName = CFSTR("subjectAltName");
const CFStringRef kSecCertificateKeyUsage = CFSTR("keyUsage");
const CFStringRef kSecCSRBasicConstraintsCA = CFSTR("basicConstraintsCA");
const CFStringRef kSecCSRBasicContraintsPathLen = CFSTR("basicConstraints");
const CFStringRef kSecCertificateExtendedKeyUsage = CFSTR("certificateEKUs");
const CFStringRef kSecCertificateExtensions = CFSTR("certificateExtensions");
const CFStringRef kSecCertificateExtensionsEncoded = CFSTR("certificateExtensionsEncoded");
const CFStringRef kSecCertificateLifetime = CFSTR("certificateLifetime");
const CFStringRef kSecCertificateSerialNumber = CFSTR("certificateSerial");

/* SubjectAltName dictionary keys */
const CFStringRef kSecSubjectAltNameDNSName = CFSTR("dNSName");
const CFStringRef kSecSubjectAltNameEmailAddress = CFSTR("rfc822Name");
const CFStringRef kSecSubjectAltNameURI = CFSTR("uniformResourceIdentifier");
const CFStringRef kSecSubjectAltNameNTPrincipalName = CFSTR("ntPrincipalName");

/* EKUs */
const CFStringRef kSecEKUServerAuth = CFSTR("1.3.6.1.5.5.7.3.1");
const CFStringRef kSecEKUClientAuth = CFSTR("1.3.6.1.5.5.7.3.2");
const CFStringRef kSecEKUCodesigning = CFSTR("1.3.6.1.5.5.7.3.3");
const CFStringRef kSecEKUEmailProtection = CFSTR("1.3.6.1.5.5.7.3.4");
const CFStringRef kSecEKUTimeStamping = CFSTR("1.3.6.1.5.5.7.3.8");
const CFStringRef kSecEKUOCSPSigning = CFSTR("1.3.6.1.5.5.7.3.9");

/* PKCS9 OIDs */
static const uint8_t pkcs9ExtensionsRequested[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 14 };
static const uint8_t pkcs9ChallengePassword[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 7 };

/* ASN1 BOOLEAN TRUE */
static const uint8_t encoded_asn1_true = 0xFF;
static const SecAsn1Item asn1_true =
{ sizeof(encoded_asn1_true), (uint8_t*)&encoded_asn1_true };

/* ASN1 BOOLEAN FALSE */
static const uint8_t encoded_asn1_false = 0x00;
static const SecAsn1Item asn1_false =
{ sizeof(encoded_asn1_false), (uint8_t*)&encoded_asn1_false };

/* ASN1 NULL */
static const uint8_t encoded_null[2] = { SEC_ASN1_NULL, 0 };
static const SecAsn1Item asn1_null = { sizeof(encoded_null), (uint8_t*)encoded_null };

static uint8_t * mod128_oid_encoding_ptr(uint8_t *ptr, uint32_t src, bool final)
{
    if (src > 128)
        ptr = mod128_oid_encoding_ptr(ptr, src / 128, false);

    unsigned char octet = src % 128;
    if (!final)
        octet |= 128;
    *ptr++ = octet;

    return ptr;
}

static uint8_t * oid_der_data(PRArenaPool *poolp, CFStringRef oid_string, size_t *oid_data_len)
{
    CFArrayRef oid = NULL;
    /* estimate encoded length from base 10 (4 bits) to base 128 (7 bits) */
    require(((size_t)CFStringGetLength(oid_string) < (SIZE_MAX/4)), out); // Guard against integer overflow on size_t
    size_t tmp_oid_length = ((((size_t)CFStringGetLength(oid_string)) * 4) / 7) + 1;
    uint8_t *tmp_oid_data = PORT_ArenaAlloc(poolp, tmp_oid_length);
    uint8_t *tmp_oid_data_ptr = tmp_oid_data;
    require(tmp_oid_data, out); // Allocation failure
    oid = CFStringCreateArrayBySeparatingStrings(kCFAllocatorDefault,
                                                            oid_string, CFSTR("."));
    CFIndex i = 0, count = CFArrayGetCount(oid);
    SInt32 first_digit = 0, digit;
    for (i = 0; i < count; i++) {
        CFStringRef oid_octet = CFArrayGetValueAtIndex(oid, i);
        SInt32 oid_octet_int_value = CFStringGetIntValue(oid_octet);
        require(abs(oid_octet_int_value) != INT32_MAX, out);
        if (i == 0)
            first_digit = oid_octet_int_value;
        else {
            if (i == 1)
                digit = 40 * first_digit + oid_octet_int_value;
            else
                digit = oid_octet_int_value;
            tmp_oid_data_ptr = mod128_oid_encoding_ptr(tmp_oid_data_ptr, digit, true);
        }
    }
    CFReleaseSafe(oid);

    *oid_data_len = tmp_oid_data_ptr - tmp_oid_data;
    return tmp_oid_data;
out:
    CFReleaseSafe(oid);
    return NULL;
}


/*
Get challenge password conversion and apply this:

ASCII ? => PrintableString subset: [A-Za-z0-9 '()+,-./:=?] ?

PrintableString > IA5String > UTF8String

Consider using IA5String for email address (now done in make_nss_atv)
*/

static inline bool printable_string(CFStringRef string, bool permitAtSymbol)
{
    bool result = true;

    /* Note: the PrintableString character set does not contain the @ symbol.
       We include it here in printable_charset so that we can first eliminate
       strings containing characters outside [PrintableString+'@'], then
       selectively eliminate strings with '@' if permitAtSymbol is false. */
    CFCharacterSetRef printable_charset =
        CFCharacterSetCreateWithCharactersInString(kCFAllocatorDefault,
            CFSTR("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                    "abcdefghijklmnopqrstuvwxyz"
                    "0123456789 '()+,-./:=?@"));
    CFCharacterSetRef not_printable_charset =
        CFCharacterSetCreateInvertedSet(kCFAllocatorDefault, printable_charset);
    CFCharacterSetRef at_charset = CFCharacterSetCreateWithCharactersInString(kCFAllocatorDefault, CFSTR("@"));
    if (CFStringFindCharacterFromSet(string, not_printable_charset, CFRangeMake(0, CFStringGetLength(string)), 0, NULL)) {
        result = false;
    } else if (CFStringFindCharacterFromSet(string, at_charset, CFRangeMake(0, CFStringGetLength(string)), 0, NULL)) {
        result = permitAtSymbol;
    }
    CFReleaseSafe(printable_charset);
    CFReleaseSafe(not_printable_charset);
    CFReleaseSafe(at_charset);

    return result;
}

static bool isEmailAddressOid(CFTypeRef oid) {
    if (isString(oid)) {
        // dotted-decimal OID string or case-insensitive "EMAIL" string
        if (CFEqual(CFSTR("1.2.840.113549.1.9.1"), oid) ||
            kCFCompareEqualTo == CFStringCompare(kSecOidEmailAddress, oid, kCFCompareCaseInsensitive)) {
            return true;
        }
    } else if (isData(oid)) {
        // data representation of OID
        if (CFDataGetLength(oid) == (CFIndex)(oidEmailAddress.length) &&
            (!memcmp(CFDataGetBytePtr(oid), oidEmailAddress.data, oidEmailAddress.length))) {
            return true;
        }
    }
    return false;
}

static bool make_nss_atv(PRArenaPool *poolp,
    CFTypeRef oid, const void * value, const unsigned char type_in, NSS_ATV *nss_atv, CFErrorRef *error)
{
    size_t length = 0;
    char *buffer = NULL;
    unsigned char type = type_in;
    if (CFGetTypeID(value) == CFStringGetTypeID()) {
        length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value),
            kCFStringEncodingUTF8);
        buffer = PORT_ArenaAlloc(poolp, length);
        /* TODO: Switch to using CFStringGetBytes,since this code will do the wrong thing for embedded 0's */
        if (!CFStringGetCString(value, buffer, length, kCFStringEncodingASCII)) {
            if (!CFStringGetCString(value, buffer, length, kCFStringEncodingUTF8)) {
                SecError(errSecParam, error, CFSTR("ATV value is not a Printable or UTF8 string"));
                return false;
            }
            if (type && type != SecASN1UTF8String) {
                SecError(errSecParam, error, CFSTR("ATV input type does not match value encoding"));
                return false;
            }
            type = SecASN1UTF8String;
        }
        else {
            if (!type || type == SecASN1PrintableString) {
                /* According to RFC5280 (4.1.2.6. Subject), the attribute value for emailAddress
                   is of type IA5String to permit inclusion of the character '@', which is not
                   part of the PrintableString character set. */
                bool useIA5String = isEmailAddressOid(oid);
                if (!printable_string(value, useIA5String)) {
                    type = SEC_ASN1_UTF8_STRING; /* has characters outside of allowed printable string set */
                } else if (useIA5String) {
                    type = SEC_ASN1_IA5_STRING; /* passed printable string + '@' check for email address oid */
                } else {
                    type = SEC_ASN1_PRINTABLE_STRING; /* also a printable string, but not email address oid */
                }
            }
        }
        length = strlen(buffer);
    } else if (CFGetTypeID(value) == CFDataGetTypeID()) {
        /* will remain valid for the duration of the operation, still maybe copy into pool */
        length = CFDataGetLength(value);
        buffer = (char *)CFDataGetBytePtr(value);
    } else {
        SecError(errSecParam, error, CFSTR("ATV value of unsupported type"));
        return false;
    }
    size_t oid_length = 0;
    uint8_t *oid_data = NULL;
    if (CFGetTypeID(oid) == CFStringGetTypeID()) {
        if (CFEqual(kSecOidCommonName, oid)) {
            oid_length = oidCommonName.length; oid_data = oidCommonName.data;
        } else if (CFEqual(kSecOidCountryName, oid)) {
            oid_length = oidCountryName.length; oid_data = oidCountryName.data;
        } else if (CFEqual(kSecOidStateProvinceName, oid)) {
            oid_length = oidStateOrProvinceName.length; oid_data = oidStateOrProvinceName.data;
        } else if (CFEqual(kSecOidLocalityName, oid)) {
            oid_length = oidLocalityName.length; oid_data = oidLocalityName.data;
        } else if (CFEqual(kSecOidOrganization, oid)) {
            oid_length = oidOrganizationName.length; oid_data = oidOrganizationName.data;
        } else if (CFEqual(kSecOidOrganizationalUnit, oid)) {
            oid_length = oidOrganizationalUnitName.length; oid_data = oidOrganizationalUnitName.data;
        } else if (CFEqual(kSecOidEmailAddress, oid)) {
            oid_length = oidEmailAddress.length; oid_data = oidEmailAddress.data;
        } else {
            oid_data = oid_der_data(poolp, oid, &oid_length);
            require_action(oid_data, out, SecError(errSecInternal, error, CFSTR("ATV OID encode failed for %@"), oid));
        }
    } else if (CFGetTypeID(oid) == CFDataGetTypeID()) {
        /* will remain valid for the duration of the operation, still maybe copy into pool */
        oid_length = CFDataGetLength(oid);
        oid_data = (uint8_t *)CFDataGetBytePtr(oid);
    } else {
        SecError(errSecParam, error, CFSTR("ATV OID of unsupported type"));
        return false;
    }
    NSS_ATV stage_nss_atv = { { oid_length, oid_data },
        { { length, (uint8_t*)buffer }, type } };
    *nss_atv = stage_nss_atv;
    return true;
out:
    return false;
}

static NSS_RDN **make_subject(PRArenaPool *poolp, CFArrayRef subject, CFErrorRef *error)
{
    if (!subject) {
        return NULL;
    }
    CFIndex rdn_ix, rdn_count = CFArrayGetCount(subject);
    NSS_RDN **rdnps = PORT_ArenaZNewArray(poolp, NSS_RDN *, rdn_count + 1);
    NSS_RDN *rdns = PORT_ArenaZNewArray(poolp, NSS_RDN, rdn_count);
    for (rdn_ix = 0; rdn_ix < rdn_count; rdn_ix++) {
        rdnps[rdn_ix] = &rdns[rdn_ix];
        CFArrayRef rdn = CFArrayGetValueAtIndex(subject, rdn_ix);
        CFIndex atv_ix, atv_count = CFArrayGetCount(rdn);
        rdns[rdn_ix].atvs = PORT_ArenaZNewArray(poolp, NSS_ATV *, atv_count + 1);
        NSS_ATV *atvs = PORT_ArenaZNewArray(poolp, NSS_ATV, atv_count);
        for (atv_ix = 0; atv_ix < atv_count; atv_ix++) {
            rdns[rdn_ix].atvs[atv_ix] = &atvs[atv_ix];
            CFArrayRef atv = CFArrayGetValueAtIndex(rdn, atv_ix);
            if (CFArrayGetCount(atv) == 2) {
                if (!make_nss_atv(poolp, CFArrayGetValueAtIndex(atv, 0),
                                  CFArrayGetValueAtIndex(atv, 1), 0, &atvs[atv_ix], error)) {
                    return NULL;
                }
            } else {
                SecError(errSecParam, error, CFSTR("subject input incorrect: 2 values required in ATV array"));
                return NULL;
            }
        }
    }
    return rdnps;
}

typedef struct {
       SecAsn1Oid typeId;
       SecAsn1Item value;
} nt_principal_other_name;

const SecAsn1Template my_other_name_template[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(nt_principal_other_name) },
    { SEC_ASN1_OBJECT_ID,
      offsetof(nt_principal_other_name,typeId), },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | 0, offsetof(nt_principal_other_name,value), kSecAsn1UTF8StringTemplate, },
    { 0, }
};
const SecAsn1Template my_other_name_template_cons[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | NGT_OtherName,
    0, my_other_name_template, sizeof(nt_principal_other_name) }
};

struct make_general_names_context {
    PRArenaPool *poolp;
    SecAsn1Item *names;
    uint32_t count;
    uint32_t capacity;
    CFErrorRef *error;
};

static void make_general_names(const void *key, const void *value, void *context)
{
    struct make_general_names_context *gn = (struct make_general_names_context *)context;
    CFErrorRef *error = gn->error;

    require(value,out);
    CFArrayRef gn_values = NULL;
    CFStringRef gn_value = NULL;
    CFIndex entry_ix, entry_count = 0;
    if (CFGetTypeID(value) == CFArrayGetTypeID()) {
        gn_values = (CFArrayRef)value;
        entry_count = CFArrayGetCount(value);
    } else if (CFGetTypeID(value) == CFStringGetTypeID()) {
        gn_value = (CFStringRef)value;
        entry_count = 1;
    } else {
        SecError(errSecParam, error, CFSTR("unsupported subjectAltName value type"));
    }

    require(entry_count > 0, out);

    require(key,out);
    require_action(CFGetTypeID(key) == CFStringGetTypeID(), out,
                   SecError(errSecParam, error, CFSTR("unsupported subjectAltName key type")));

    if (!gn->names || (gn->count == gn->capacity)) {
        uint32_t capacity = gn->capacity;
        if (capacity) {
            capacity *= 2;
        } else {
            capacity = 10;
        }

        void * new_array = PORT_ArenaZNewArray(gn->poolp, SecAsn1Item, capacity);
        if (gn->names) {
            memcpy(new_array, gn->names, gn->capacity);
        }
        gn->names = new_array;
        gn->capacity = capacity;
    }

    NSS_GeneralName general_name_item = { { }, -1 };
    if (kCFCompareEqualTo == CFStringCompare(kSecSubjectAltNameDNSName, key, kCFCompareCaseInsensitive)) {
        general_name_item.tag = NGT_DNSName;
    } else if (kCFCompareEqualTo == CFStringCompare(kSecSubjectAltNameEmailAddress, key, kCFCompareCaseInsensitive)) {
        general_name_item.tag = NGT_RFC822Name;
    } else if (kCFCompareEqualTo == CFStringCompare(kSecSubjectAltNameURI, key, kCFCompareCaseInsensitive)) {
        general_name_item.tag = NGT_URI;
    } else if (kCFCompareEqualTo == CFStringCompare(kSecSubjectAltNameNTPrincipalName, key, kCFCompareCaseInsensitive)) {
        /*
            NT Principal in SubjectAltName is defined in the context of Smartcards:

            http://www.oid-info.com/get/1.3.6.1.4.1.311.20.2.3
            http://support.microsoft.com/default.aspx?scid=kb;en-us;281245

            Subject Alternative Name = Other Name: Principal Name= (UPN). For example:
            UPN = user1@name.com
            The UPN OtherName OID is : "1.3.6.1.4.1.311.20.2.3"
            The UPN OtherName value: Must be ASN1-encoded UTF8 string
            Subject = Distinguished name of user. This field is a mandatory extension, but the population of this field is optional.
        */

		/* OtherName ::= SEQUENCE {
							type-id OBJECT IDENTIFIER,
							value [0] EXPLICIT ANY DEFINED BY type-id
						} */
		uint8_t nt_principal_oid[] = { 0x2B, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x14, 0x02, 0x03 };
        nt_principal_other_name name = {};

        size_t length = 0;
        char *buffer = NULL;
		require_action(gn_value, out, SecError(errSecParam, error, CFSTR("NTPrincipalName types do not allow multiple values")));
        require(CFGetTypeID(gn_value) == CFStringGetTypeID(), out);
        length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value),
            kCFStringEncodingUTF8);
        buffer = PORT_ArenaAlloc(gn->poolp, length);
        if (!CFStringGetCString(value, buffer, length, kCFStringEncodingUTF8)) {
            SecError(errSecParam, error, CFSTR("failed to get NTPrincipalName value bytes"));
            goto out;
        }

        name.typeId.Length = sizeof(nt_principal_oid);
        name.typeId.Data = nt_principal_oid;
        name.value.Length = strlen(buffer);
        name.value.Data = (uint8_t*)buffer;
        if (!SEC_ASN1EncodeItem(gn->poolp, &gn->names[gn->count], &name, my_other_name_template_cons)) {
            SecError(errSecInternal, error, CFSTR("failed to encode NTPrincipalName"));
        }
        gn->count++;

        /* We already encoded the value for the general name */
		goto out;
    } else {
        secerror("csr: unsupported SubjectAltName type, skipping");
        goto out;
    }

    if (gn_values) {
        for (entry_ix = 0; entry_ix < entry_count; entry_ix++) {
            CFStringRef entry_value = (CFStringRef)CFArrayGetValueAtIndex(gn_values, entry_ix);
            require_action(CFGetTypeID(entry_value) == CFStringGetTypeID(), out,
                           SecError(errSecParam, error, CFSTR("subjectAltName value is not a string")));
            CFIndex buffer_size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(entry_value),
                kCFStringEncodingUTF8); /* we only allow ASCII => only expect IA5Strings */
            char *buffer = (char *)PORT_ArenaZNewArray(gn->poolp, uint8_t, buffer_size);
            require_action(CFStringGetCString(entry_value, buffer, buffer_size, kCFStringEncodingASCII), out,
                           SecError(errSecParam, error, CFSTR("failed to get subjectAltName value bytes")));
            general_name_item.item.Data = (uint8_t*)buffer;
            general_name_item.item.Length = strlen(buffer);
            SEC_ASN1EncodeItem(gn->poolp, &gn->names[gn->count], &general_name_item, kSecAsn1GeneralNameTemplate);
            gn->count++;
        }
    } else if (gn_value) {
        CFIndex buffer_size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(gn_value),
            kCFStringEncodingUTF8);
        char *buffer = (char *)PORT_ArenaZNewArray(gn->poolp, uint8_t, buffer_size);
        require_action(CFStringGetCString(gn_value, buffer, buffer_size, kCFStringEncodingASCII), out,
                       SecError(errSecParam, error, CFSTR("failed to get subjectAltName value bytes")));
        general_name_item.item.Data = (uint8_t*)buffer;
        general_name_item.item.Length = strlen(buffer);
        SEC_ASN1EncodeItem(gn->poolp, &gn->names[gn->count], &general_name_item, kSecAsn1GeneralNameTemplate);
        gn->count++;
    }
out:
    return;
}

static SecAsn1Item make_subjectAltName_extension(PRArenaPool *poolp, CFDictionaryRef subjectAltNames, CFErrorRef *error)
{
    SecAsn1Item subjectAltExt = {};

    struct make_general_names_context context = { poolp, NULL, 0, 0, error };
    CFDictionaryApplyFunction(subjectAltNames, make_general_names, &context);

    // all general names in a sequence:
    uint32_t ix;
    SecAsn1Item **general_names = PORT_ArenaZNewArray(poolp, SecAsn1Item *, context.count + 1);
    for (ix = 0; ix < context.count; ix++) {
        general_names[ix] = &context.names[ix];
    }
    NSS_GeneralNames gnames = { general_names };
    if (!SEC_ASN1EncodeItem(poolp, &subjectAltExt, &gnames, kSecAsn1GeneralNamesTemplate)) {
        SecError(errSecInternal, error, CFSTR("failed to encode SubjectAltName"));
    }

    return subjectAltExt;
}

static bool make_EKU_extension(PRArenaPool *poolp, CFArrayRef extendedKeyUsages, SecAsn1Item *result, CFErrorRef *error)
{
    SecAsn1Item ekuExt = {};
    CFIndex count = CFArrayGetCount(extendedKeyUsages);
    uint32_t ix;
    SecAsn1Item **ext_key_usages = PORT_ArenaZNewArray(poolp, SecAsn1Item *, count + 1);
    for (ix = 0; ix < count; ix++) {
        CFStringRef ekuStr = CFArrayGetValueAtIndex(extendedKeyUsages, ix);
        if (!isString(ekuStr) || !SecCertificateIsOidString(ekuStr)) {
            SecError(errSecParam, error, CFSTR("eku is not an OID"));
            return false;
        }
        CFDataRef ekuData = SecCertificateCreateOidDataFromString(NULL, ekuStr);
        if (!ekuData) {
            SecError(errSecParam, error, CFSTR("failed to create EKU data"));
            return false;
        }
        ext_key_usages[ix] = (SecAsn1Item*)PORT_ArenaAlloc(poolp, sizeof(SecAsn1Item));
        ext_key_usages[ix]->Length = CFDataGetLength(ekuData);
        ext_key_usages[ix]->Data = (unsigned char*)PORT_ArenaAlloc(poolp, CFDataGetLength(ekuData));
        PORT_Memcpy(ext_key_usages[ix]->Data, CFDataGetBytePtr(ekuData), CFDataGetLength(ekuData));
        CFReleaseNull(ekuData);
    }
    NSS_ExtKeyUsage ekus = { ext_key_usages };
    if (!SEC_ASN1EncodeItem(poolp, &ekuExt, &ekus, kSecAsn1ExtKeyUsageTemplate)) {
        SecError(errSecInternal, error, CFSTR("failed to encode EKUs"));
        return false;
    }
    *result = ekuExt;
    return true;
}

struct add_custom_extension_args {
    PLArenaPool *poolp;
    NSS_CertExtension *csr_extension;
    uint32_t num_extensions;
    uint32_t max_extensions;
    bool encodeData;
    CFErrorRef *error;
};

static void add_custom_extension(const void *key, const void *value, void *context)
{
    struct add_custom_extension_args *args = (struct add_custom_extension_args *)context;
    size_t der_data_len;
    CFErrorRef *error = args->error;

    require_action(args->num_extensions < args->max_extensions, out, SecError(errSecParam, error, CFSTR("too many extensions")));

    uint8_t * der_data = oid_der_data(args->poolp, key, &der_data_len);
    SecAsn1Item encoded_value = {};

    if (CFGetTypeID(value) == CFStringGetTypeID()) {
        if (!args->encodeData) {
            SecError(errSecParam, error, CFSTR("encoded extensions must be a data type"));
            goto out;
        }
        CFIndex buffer_size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value), kCFStringEncodingUTF8);
        char *buffer = (char *)PORT_ArenaZNewArray(args->poolp, uint8_t, buffer_size);
        if (!CFStringGetCString(value, buffer, buffer_size, kCFStringEncodingUTF8)) {
            SecError(errSecParam, error, CFSTR("failed to get custom extension string bytes"));
            goto out;
        }

        SecAsn1Item buffer_item = { strlen(buffer), (uint8_t*)buffer };
        if (!SEC_ASN1EncodeItem(args->poolp, &encoded_value, &buffer_item, kSecAsn1UTF8StringTemplate)) {
            SecError(errSecInternal, error, CFSTR("failed to encode extension string"));
        }
    } else if (CFGetTypeID(value) == CFDataGetTypeID()) {
        if (args->encodeData) {
            SecAsn1Item data_item = { CFDataGetLength(value), (uint8_t*)CFDataGetBytePtr(value) };
            if (!SEC_ASN1EncodeItem(args->poolp, &encoded_value, &data_item, kSecAsn1OctetStringTemplate)) {
                SecError(errSecInternal, error, CFSTR("failed to encode extension data"));
            }
        }
        else {
            encoded_value.Length = CFDataGetLength(value);
            encoded_value.Data = (uint8_t*)CFDataGetBytePtr(value);
        }
    } else {
        SecError(errSecParam, error, CFSTR("unsupported custom extensions value type"));
        goto out;
    }


    if (der_data && encoded_value.Length) {
        args->csr_extension[args->num_extensions].value = encoded_value;
        args->csr_extension[args->num_extensions].extnId.Length = der_data_len;
        args->csr_extension[args->num_extensions].extnId.Data = der_data;
        args->num_extensions++;
    }
out:
    return;
}

static
bool
extensions_from_parameters(PRArenaPool *poolp, CFDictionaryRef parameters, NSS_CertExtension ***result, CFErrorRef *error)
{
    uint32_t num_extensions = 0, max_extensions = 10;
    NSS_CertExtension **csr_extensions = PORT_ArenaZNewArray(poolp, NSS_CertExtension *, max_extensions + 1); /* NULL terminated array */
    NSS_CertExtension *csr_extension = PORT_ArenaZNewArray(poolp, NSS_CertExtension, max_extensions);
    CFErrorRef localError = NULL;

    CFBooleanRef basic_contraints_ca = CFDictionaryGetValue(parameters, kSecCSRBasicConstraintsCA);
    CFNumberRef basic_contraints_num = CFDictionaryGetValue(parameters, kSecCSRBasicContraintsPathLen);
    if (basic_contraints_ca || basic_contraints_num) {
        bool is_ca = basic_contraints_ca != kCFBooleanFalse;

        if (!is_ca && basic_contraints_num) {
            SecError(errSecParam, &localError, CFSTR("non-CA cert does not support path len"));
            goto out;
        }

        secdebug("csr", "encoding basic constraints");
        NSS_BasicConstraints basic_contraints = { is_ca ? asn1_true : asn1_false, {} };
        uint8_t path_len;

        if (basic_contraints_num) {
            int basic_contraints_path_len = 0;
            require_action(CFNumberGetValue(basic_contraints_num, kCFNumberIntType, &basic_contraints_path_len), out,
                           SecError(errSecParam, &localError, CFSTR("failed to get basic constraints path length value")));
            if (basic_contraints_path_len >= 0 && basic_contraints_path_len < 256) {
                secdebug("csr", "encoding basic constraints path len");
                path_len = (uint8_t)basic_contraints_path_len;
                basic_contraints.pathLenConstraint.Length = sizeof(path_len);
                basic_contraints.pathLenConstraint.Data = &path_len;
            }
        }

        csr_extension[num_extensions].extnId.Data = oidBasicConstraints.data;
        csr_extension[num_extensions].extnId.Length = oidBasicConstraints.length;
        csr_extension[num_extensions].critical = asn1_true;

        if (!SEC_ASN1EncodeItem(poolp, &csr_extension[num_extensions].value, &basic_contraints,
                                kSecAsn1BasicConstraintsTemplate)) {
            SecError(errSecParam, &localError, CFSTR("failed to encode basic constraints extension"));
            goto out;
        }
        require_action(num_extensions++ < max_extensions, out, SecError(errSecParam, &localError, CFSTR("too many extensions")));
    }

    CFDictionaryRef subject_alternate_names = CFDictionaryGetValue(parameters, kSecSubjectAltName);
    if (subject_alternate_names) {
        secdebug("csr", "encoding subjectAltName");
        require_action(CFGetTypeID(subject_alternate_names) == CFDictionaryGetTypeID(), out,
                       SecError(errSecParam, &localError, CFSTR("wrong value type for subjectAltName")));
        csr_extension[num_extensions].value = make_subjectAltName_extension(poolp, subject_alternate_names, error);
        /* set up subjectAltName cert request value */
        csr_extension[num_extensions].extnId.Length = oidSubjectAltName.length;
        csr_extension[num_extensions].extnId.Data = oidSubjectAltName.data;
        require_action(num_extensions++ < max_extensions, out, SecError(errSecParam, &localError, CFSTR("too many extensions")));
    }

    CFArrayRef extended_key_usages = CFDictionaryGetValue(parameters, kSecCertificateExtendedKeyUsage);
    if (extended_key_usages) {
        secdebug("csr", "encoding EKU");
        require_action(CFGetTypeID(extended_key_usages) == CFArrayGetTypeID(), out,
                       SecError(errSecParam, &localError, CFSTR("wrong value type for EKU")));
        require_action(make_EKU_extension(poolp, extended_key_usages, &csr_extension[num_extensions].value, error), out,
                       SecError(errSecParam, &localError, CFSTR("failed to make EKU extension")));
        csr_extension[num_extensions].extnId.Length = oidExtendedKeyUsage.length;
        csr_extension[num_extensions].extnId.Data = oidExtendedKeyUsage.data;
        require_action(num_extensions++ < max_extensions, out, SecError(errSecParam, &localError, CFSTR("too many extensions")));
    }

    CFNumberRef key_usage_requested = CFDictionaryGetValue(parameters, kSecCertificateKeyUsage);
    SecAsn1Item key_usage_asn1_value = { 0 };
    if (key_usage_requested && isNumber(key_usage_requested)) {
        secdebug("csr", "encoding keyUsage");
        int key_usage_value;
        require_action(CFNumberGetValue(key_usage_requested, kCFNumberIntType, &key_usage_value), out,
                       SecError(errSecParam, &localError, CFSTR("failed to get key usage value")));
        if (key_usage_value > 0) {
            uint32_t key_usage_value_be = 0, key_usage_mask = (uint32_t)0x80000000; // 1L<<31
            uint32_t key_usage_value_max_bitlen = 9, key_usage_value_bitlen = 0;
            while(key_usage_value_max_bitlen) {
                if (key_usage_value & 1) {
                    key_usage_value_be |= key_usage_mask;
                    key_usage_value_bitlen = 10 - key_usage_value_max_bitlen;
                }
                key_usage_value >>= 1;
                key_usage_value_max_bitlen--;
                key_usage_mask >>= 1;
            }

            SecAsn1Item key_usage_input = { key_usage_value_bitlen,
                ((uint8_t*)&key_usage_value_be) + 3 - (key_usage_value_bitlen >> 3) };
            if (!SEC_ASN1EncodeItem(poolp, &key_usage_asn1_value, &key_usage_input, kSecAsn1BitStringTemplate)) {
                SecError(errSecInternal, &localError, CFSTR("failed to encode keyUsage"));
                goto out;
            }

            csr_extension[num_extensions].extnId.Data = oidKeyUsage.data;
            csr_extension[num_extensions].extnId.Length = oidKeyUsage.length;
            csr_extension[num_extensions].critical = asn1_true;
            csr_extension[num_extensions].value = key_usage_asn1_value;
            require_action(num_extensions++ < max_extensions, out, SecError(errSecParam, &localError, CFSTR("too many extensions")));
        } else {
            secdebug("csr", "key usage of 0, skipping extension");
        }
    } else if (key_usage_requested) {
        SecError(errSecParam, &localError, CFSTR("failed to get key usage value"));
        goto out;
    }

    CFDictionaryRef custom_extension_requested = CFDictionaryGetValue(parameters, kSecCertificateExtensions);
    if (custom_extension_requested) {
        secdebug("csr", "encoding custom extension");
        require_action(CFGetTypeID(custom_extension_requested) == CFDictionaryGetTypeID(), out,
                       secerror("csr: wrong value type for custom extensions"));
        struct add_custom_extension_args args = {
            poolp,
            csr_extension,
            num_extensions,
            max_extensions,
            true,
            error
        };
        CFDictionaryApplyFunction(custom_extension_requested, add_custom_extension, &args);
        num_extensions = args.num_extensions;
    }

    CFDictionaryRef custom_encoded_extension_requested = CFDictionaryGetValue(parameters, kSecCertificateExtensionsEncoded);
    if (custom_encoded_extension_requested) {
        secdebug("csr", "encoding custom encoded extension");
        require_action(CFGetTypeID(custom_encoded_extension_requested) == CFDictionaryGetTypeID(), out,
                       SecError(errSecParam, &localError, CFSTR("wrong value type for custom extensions")));
        struct add_custom_extension_args args = {
            poolp,
            csr_extension,
            num_extensions,
            max_extensions,
            false,
            error
        };
        CFDictionaryApplyFunction(custom_encoded_extension_requested, add_custom_extension, &args);
        num_extensions = args.num_extensions;
    }

    /* extensions requested (subjectAltName, keyUsage) sequence of extension sequences */
    uint32_t ix = 0;
    for (ix = 0; ix < num_extensions; ix++) {
        csr_extensions[ix] = csr_extension[ix].extnId.Length ? &csr_extension[ix] : NULL;
    }

    *result = csr_extensions;
    return true;

out:
    if (localError) {
        secerror("csr: extension encoding failed: %@", localError);
        if (error) {*error = CFRetainSafe(localError);}
        CFReleaseNull(localError);
    }
    return false;
}

static bool parameters_contains_extensions(CFDictionaryRef parameters) {
    if (CFDictionaryContainsKey(parameters, kSecSubjectAltName) ||
        CFDictionaryContainsKey(parameters, kSecCertificateKeyUsage) ||
        CFDictionaryContainsKey(parameters, kSecCSRBasicConstraintsCA) ||
        CFDictionaryContainsKey(parameters, kSecCSRBasicContraintsPathLen) ||
        CFDictionaryContainsKey(parameters, kSecCertificateExtendedKeyUsage) ||
        CFDictionaryContainsKey(parameters, kSecCertificateExtensions) ||
        CFDictionaryContainsKey(parameters, kSecCertificateExtensionsEncoded)) {
        return true;
    }
    return false;
}

static
bool
extensions_from_parameters_or_request_extensions(PRArenaPool *poolp, CFDictionaryRef parameters, CFTypeRef requestExtensions, NSS_CertExtension ***resultExtensions, CFErrorRef *error)
{
    /* "CA" extensions set in the parameters dictionary always override the extensions in the request */
    if (isDictionary(parameters) && parameters_contains_extensions(parameters)) {
        secinfo("csr", "overriding request extensions with parameters extensions");
        return extensions_from_parameters(poolp, parameters, resultExtensions, error);
    } else if (isData(requestExtensions)) {
        SecAsn1Item requested_extensions = { CFDataGetLength(requestExtensions), (uint8_t*)CFDataGetBytePtr(requestExtensions) };
        require_noerr_action_quiet(SEC_ASN1DecodeItem(poolp, resultExtensions,
                    kSecAsn1SequenceOfCertExtensionTemplate, &requested_extensions), out,
                                   secerror("failed to decode requested CSR extensions"));
        return true;
    } else if (isDictionary(requestExtensions)) {
        return extensions_from_parameters(poolp, requestExtensions, resultExtensions, error);
    } else if ((!requestExtensions && !parameters) ||
               (!requestExtensions && isDictionary(parameters) && !parameters_contains_extensions(parameters))) {
        /* no extensions, not an error */
        secinfo("csr", "no extensions set");
        return true;
    }
out:
    secerror("csr: extensions or parameters wrong input type");
    return false;
}

static
NSS_Attribute **nss_attributes_from_parameters_dict(PRArenaPool *poolp, CFDictionaryRef parameters)
{
    /* A challenge-password attribute must have a single attribute value.

       ChallengePassword attribute values generated in accordance with this
       version of this document SHOULD use the PrintableString encoding
       whenever possible.  If internationalization issues make this
       impossible, the UTF8String alternative SHOULD be used.  PKCS #9-
       attribute processing systems MUST be able to recognize and process
       all string types in DirectoryString values.

       Upperbound of 255 defined for all PKCS#9 attributes.

       pkcs-9 OBJECT IDENTIFIER ::= {iso(1) member-body(2) us(840)
                                        rsadsi(113549) pkcs(1) 9}
       pkcs-9-at-challengePassword   OBJECT IDENTIFIER ::= {pkcs-9 7}

    */
    if (!parameters) {
        return NULL;
    }
    uint32_t num_attrs = 0;

    CFStringRef challenge = CFDictionaryGetValue(parameters, kSecCSRChallengePassword);
    NSS_Attribute challenge_password_attr = {};
    if (challenge) {
        CFIndex buffer_size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(challenge),
                                                                kCFStringEncodingUTF8); /* we only allow UTF8 or ASCII */
        char *buffer = (char *)PORT_ArenaZNewArray(poolp, uint8_t, buffer_size);
        bool utf8 = false;
        if (!CFStringGetCString(challenge, buffer, buffer_size, kCFStringEncodingASCII)) {
            if (!CFStringGetCString(challenge, buffer, buffer_size, kCFStringEncodingUTF8)) {
                secerror("csr: unable to get challenge bytes");
                return NULL;
            }
            utf8 = true;
        } else if (!printable_string(challenge, false)) {
            utf8 = true;
        }

        SecAsn1Item *challenge_password_value = PORT_ArenaZNewArray(poolp, SecAsn1Item, 1);
        SecAsn1Item challenge_password_raw = { strlen(buffer), (uint8_t*)buffer };
        if (!SEC_ASN1EncodeItem(poolp, challenge_password_value, &challenge_password_raw,
                                utf8 ? kSecAsn1UTF8StringTemplate : kSecAsn1PrintableStringTemplate)) {
            secerror("csr: failed to encode challenge");
            return NULL;
        }
        SecAsn1Item **challenge_password_values = PORT_ArenaZNewArray(poolp, SecAsn1Item *, 2);
        challenge_password_values[0] = challenge_password_value;
        challenge_password_attr.attrType.Length = sizeof(pkcs9ChallengePassword);
        challenge_password_attr.attrType.Data = (uint8_t*)&pkcs9ChallengePassword;
        challenge_password_attr.attrValue = challenge_password_values;
        num_attrs++;
    }

    NSS_CertExtension **extensions = NULL;

    if (!extensions_from_parameters(poolp, parameters, &extensions, NULL)) {
        secerror("csr: failed to encode extensions");
        return NULL;
    }
    NSS_Attribute extensions_requested_attr = {};
    if (extensions) {
        SecAsn1Item *extensions_requested_value = PORT_ArenaZNewArray(poolp, SecAsn1Item, 1);
        if(!SEC_ASN1EncodeItem(poolp, extensions_requested_value, &extensions, kSecAsn1SequenceOfCertExtensionTemplate)) {
            secerror("csr: failed to encode extension attributes");
            return NULL;
        }
        SecAsn1Item **extensions_requested_values = PORT_ArenaZNewArray(poolp, SecAsn1Item *, 2);
        extensions_requested_values[0] = extensions_requested_value;
        extensions_requested_values[1] = NULL;
        extensions_requested_attr.attrType.Length = sizeof(pkcs9ExtensionsRequested);
        extensions_requested_attr.attrType.Data = (uint8_t*)pkcs9ExtensionsRequested;
        extensions_requested_attr.attrValue = extensions_requested_values;
        num_attrs++;
    }

    NSS_Attribute **attributes_ptr = PORT_ArenaZNewArray(poolp, NSS_Attribute *, num_attrs + 1);
    NSS_Attribute *attributes = PORT_ArenaZNewArray(poolp, NSS_Attribute, num_attrs);
    if (challenge_password_attr.attrType.Length) {
        --num_attrs;
        attributes[num_attrs] = challenge_password_attr;
        attributes_ptr[num_attrs] = &attributes[num_attrs];
    }
    if (extensions_requested_attr.attrType.Length) {
        --num_attrs;
        attributes[num_attrs] = extensions_requested_attr;
        attributes_ptr[num_attrs] = &attributes[num_attrs];
    }
    return attributes_ptr;
}

static CF_RETURNS_RETAINED CFDataRef make_public_key (SecKeyRef publicKey, SecAsn1PubKeyInfo *publicKeyInfo, bool *allocated_parameters, CFErrorRef *error) {
    if (!publicKey) { return NULL; }
    CFDataRef publicKeyData = SecKeyCopyExternalRepresentation(publicKey, error);
    if (!publicKeyData) {
        return NULL;
    }
    uint8_t *spki_params = NULL;

    if (SecKeyGetAlgorithmId(publicKey) == kSecRSAAlgorithmID) {
        publicKeyInfo->algorithm.algorithm.Length = oidRsa.length;
        publicKeyInfo->algorithm.algorithm.Data = oidRsa.data;
        publicKeyInfo->algorithm.parameters = asn1_null;
        *allocated_parameters = false;
        secdebug("csr", "no allocated parameters");
    } else if (SecKeyGetAlgorithmId(publicKey) == kSecECDSAAlgorithmID) {
        publicKeyInfo->algorithm.algorithm.Length = oidEcPubKey.length;
        publicKeyInfo->algorithm.algorithm.Data = oidEcPubKey.data;
        size_t parameters_size = 0;
        SecECNamedCurve namedCurve = SecECKeyGetNamedCurve(publicKey);
        switch (namedCurve) {
            case kSecECCurveSecp256r1:
                parameters_size = oidEcPrime256v1.length + 2;
                spki_params = malloc(parameters_size);
                memcpy(spki_params + 2, oidEcPrime256v1.data, oidEcPrime256v1.length);
                break;
            case kSecECCurveSecp384r1:
                parameters_size = oidAnsip384r1.length + 2;
                spki_params = malloc(parameters_size);
                memcpy(spki_params + 2, oidAnsip384r1.data, oidAnsip384r1.length);
                break;
            case kSecECCurveSecp521r1:
                parameters_size = oidAnsip521r1.length + 2;
                spki_params = malloc(parameters_size);
                memcpy(spki_params + 2, oidAnsip521r1.data, oidAnsip521r1.length);
                break;
            default:
                SecError(errSecUnsupportedAlgorithm, error, CFSTR("unsupported EC curve"));
                CFReleaseNull(publicKeyData);
                return NULL;
        }
        spki_params[0] = 0x06;
        spki_params[1] = (uint8_t)(parameters_size - 2);
        publicKeyInfo->algorithm.parameters.Length = parameters_size;
        publicKeyInfo->algorithm.parameters.Data = spki_params;
        *allocated_parameters = true;
        secdebug("csr", "allocated parameters");
    } else {
        SecError(errSecUnsupportedAlgorithm, error, CFSTR("unsupported key algorithmId"));
        CFReleaseNull(publicKeyData);
        return NULL;
    }

    publicKeyInfo->subjectPublicKey.Data = (uint8_t *)CFDataGetBytePtr(publicKeyData);
    publicKeyInfo->subjectPublicKey.Length = CFDataGetLength(publicKeyData) * 8;

    return publicKeyData;
}

static CF_RETURNS_RETAINED CFDataRef make_signature (void *data_pointer, size_t data_length, SecKeyRef privateKey,
                                                     CFStringRef digestAlgorithm, SecAsn1AlgId *signature_algorithm_info, CFErrorRef *error) {
    SecKeyAlgorithm keyAlgorithm = NULL;
    CFIndex keyAlgorithmId = SecKeyGetAlgorithmId(privateKey);
    if (keyAlgorithmId == kSecRSAAlgorithmID) {
        if (!digestAlgorithm || CFEqualSafe(digestAlgorithm, kSecCMSHashingAlgorithmSHA1)) {
            /* default is SHA-1 for backwards compatibility */
            keyAlgorithm = kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1;
            signature_algorithm_info->algorithm.Length = oidSha1Rsa.length;
            signature_algorithm_info->algorithm.Data = oidSha1Rsa.data;
        } else if (CFEqualSafe(digestAlgorithm, kSecCMSHashingAlgorithmSHA256)) {
            keyAlgorithm = kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256;
            signature_algorithm_info->algorithm.Length = oidSha256Rsa.length;
            signature_algorithm_info->algorithm.Data = oidSha256Rsa.data;
        } else if (CFEqualSafe(digestAlgorithm, kSecCMSHashingAlgorithmSHA384)) {
            keyAlgorithm = kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA384;
            signature_algorithm_info->algorithm.Length = oidSha384Rsa.length;
            signature_algorithm_info->algorithm.Data = oidSha384Rsa.data;
        } else if (CFEqualSafe(digestAlgorithm, kSecCMSHashingAlgorithmSHA512)) {
            keyAlgorithm = kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA512;
            signature_algorithm_info->algorithm.Length = oidSha512Rsa.length;
            signature_algorithm_info->algorithm.Data = oidSha512Rsa.data;
        }
        /* All RSA signatures use NULL paramters */
        signature_algorithm_info->parameters = asn1_null;
    } else if (keyAlgorithmId == kSecECDSAAlgorithmID) {
        if (!digestAlgorithm || CFEqualSafe(digestAlgorithm, kSecCMSHashingAlgorithmSHA256)) {
            keyAlgorithm = kSecKeyAlgorithmECDSASignatureMessageX962SHA256;
            signature_algorithm_info->algorithm.Length = oidSha256Ecdsa.length;
            signature_algorithm_info->algorithm.Data = oidSha256Ecdsa.data;
        } else if (CFEqualSafe(digestAlgorithm, kSecCMSHashingAlgorithmSHA384)) {
            keyAlgorithm = kSecKeyAlgorithmECDSASignatureMessageX962SHA384;
            signature_algorithm_info->algorithm.Length = oidSha384Ecdsa.length;
            signature_algorithm_info->algorithm.Data = oidSha384Ecdsa.data;
        } else if (CFEqualSafe(digestAlgorithm, kSecCMSHashingAlgorithmSHA512)) {
            keyAlgorithm = kSecKeyAlgorithmECDSASignatureMessageX962SHA512;
            signature_algorithm_info->algorithm.Length = oidSha512Ecdsa.length;
            signature_algorithm_info->algorithm.Data = oidSha512Ecdsa.data;
        }
        /* All EC signatures use absent paramters */
        signature_algorithm_info->parameters.Length = 0;
        signature_algorithm_info->parameters.Data = NULL;
    }

    if (!keyAlgorithm) {
        SecError(errSecUnsupportedAlgorithm, error, CFSTR("unsupported key or hashing algorithm"));
        return NULL;
    }

    CFDataRef data = NULL, signature = NULL;
    if (!data_pointer || data_length == 0) { return NULL; } // Not an error, used to get signature algorithm without signature
    data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, data_pointer, data_length, kCFAllocatorNull);
    signature = SecKeyCreateSignature(privateKey, keyAlgorithm, data, error);
    CFReleaseSafe(data);

    return signature;
}

CFDataRef SecGenerateCertificateRequestWithParameters(SecRDN *subject,
    CFDictionaryRef parameters, SecKeyRef publicKey, SecKeyRef privateKey)
{
    if (subject == NULL || *subject == NULL) {
        return NULL;
    }

    CFDataRef csr = NULL;
    CFDataRef publicKeyData= NULL, signature = NULL;
    bool allocated_parameters = false;
    SecKeyRef realPublicKey = NULL; /* We calculate this from the private key rather than
                                     * trusting the caller to give us the right one. */
    PRArenaPool *poolp = PORT_NewArena(1024);
    CFErrorRef localError = NULL;

    if (!poolp) {
        secerror("csr: pool allocation failure");
        return NULL;
    }

    NSSCertRequest certReq;
    memset(&certReq, 0, sizeof(certReq));

    /* version */
    unsigned char version = 0;
    certReq.reqInfo.version.Length = sizeof(version);
    certReq.reqInfo.version.Data = &version;

    /* subject */
    unsigned atv_num = 0, num = 0;
    SecRDN *one_rdn;
    SecATV *one_atv;
    for (one_rdn = subject; *one_rdn; one_rdn++) {
        for (one_atv = *one_rdn; one_atv->oid; one_atv++) {
            atv_num++;
        }
        atv_num++; /* one more */
        num++;
    }
    const unsigned min1atv_num = atv_num > 0 ? atv_num : 1;
    const unsigned min1num = num > 0 ? num : 1;
    NSS_ATV *atvs = (NSS_ATV *)malloc(sizeof(NSS_ATV) * min1atv_num);
    NSS_ATV **atvps = (NSS_ATV **)malloc(sizeof(NSS_ATV *) * min1atv_num);
    NSS_RDN *rdns = (NSS_RDN *)malloc(sizeof(NSS_RDN) * min1num);
    NSS_RDN **rdnps = (NSS_RDN **)malloc(sizeof(NSS_RDN *) * (num + 1));
    atv_num = 0;
    unsigned rdn_num = 0;
    for (one_rdn = subject; *one_rdn; one_rdn++) {
        rdns[rdn_num].atvs = &atvps[atv_num];
        rdnps[rdn_num] = &rdns[rdn_num];
        rdn_num++;
        for (one_atv = *one_rdn; one_atv->oid; one_atv++) {
            if (!make_nss_atv(poolp, one_atv->oid, one_atv->value,
                              one_atv->type, &atvs[atv_num], &localError)) {
                secerror("csr: failed to encode atv");
                goto out;
            }
            atvps[atv_num] = &atvs[atv_num];
            atv_num++;
        }
        atvps[atv_num++] = NULL;
    }
    rdnps[rdn_num] = NULL;
    certReq.reqInfo.subject.rdns = rdnps;

    /* public key info */
    if (privateKey) {
        realPublicKey = SecKeyCopyPublicKey(privateKey);
    }
    if (!realPublicKey) {
        /* If we can't get the public key from the private key,
         * fall back to the public key provided by the caller. */
        secinfo("csr", "unable to create public key from private, using input public key");
        realPublicKey = CFRetainSafe(publicKey);
    }
    require_action_quiet(realPublicKey, out, secerror("csr: failed to get public key"));
    publicKeyData = make_public_key(realPublicKey, &certReq.reqInfo.subjectPublicKeyInfo, &allocated_parameters, &localError);
    require_action_quiet(publicKeyData, out, secerror("csr: failed to encode public key"));

    certReq.reqInfo.attributes = nss_attributes_from_parameters_dict(poolp, parameters);
    SecCmsArraySortByDER((void **)certReq.reqInfo.attributes, kSecAsn1AttributeTemplate, NULL);

    /* encode request info by itself to calculate signature */
    SecAsn1Item reqinfo = {};
    if (!SEC_ASN1EncodeItem(poolp, &reqinfo, &certReq.reqInfo, kSecAsn1CertRequestInfoTemplate)) {
        secerror("csr: failed to encode request info");
    }

    /* calculate signature and encode signature info */
    CFStringRef algorithm = NULL;
    if (parameters) {
        algorithm = CFDictionaryGetValue(parameters, kSecCMSSignHashAlgorithm);
    }
    signature = make_signature(reqinfo.Data, reqinfo.Length, privateKey, algorithm, &certReq.signatureAlgorithm, &localError);
    require_action_quiet(signature, out, secerror("csr: failed to create CSR signature"));
    certReq.signature.Data = (uint8_t *)CFDataGetBytePtr(signature);
    certReq.signature.Length = 8 * CFDataGetLength(signature);

    /* encode csr */
    SecAsn1Item cert_request = {};
    require_action_quiet(SEC_ASN1EncodeItem(poolp, &cert_request, &certReq, kSecAsn1CertRequestTemplate), out,
                         secerror("csr: failed to encode cert request"));
    csr = CFDataCreate(kCFAllocatorDefault, cert_request.Data, cert_request.Length);

out:
    if (allocated_parameters) {
        free(certReq.reqInfo.subjectPublicKeyInfo.algorithm.parameters.Data);
    }
    if (poolp) { PORT_FreeArena(poolp, PR_TRUE); }
    CFReleaseSafe(realPublicKey);
    CFReleaseSafe(publicKeyData);
    CFReleaseSafe(signature);
    free(atvs);
    free(atvps);
    free(rdns);
    free(rdnps);
    if (localError) {
        secerror("csr: cert request encoding failed: %@", localError);
        CFReleaseNull(localError);
    }
    return csr;
}

CFDataRef SecGenerateCertificateRequest(CFArrayRef subject,
    CFDictionaryRef parameters, SecKeyRef publicKey, SecKeyRef privateKey)
{
    CFDataRef csr = NULL;
    PRArenaPool *poolp = PORT_NewArena(1024);
    CFDataRef publicKeyData = NULL, signature = NULL;
    SecKeyRef realPublicKey = NULL; /* We calculate this from the private key rather than
                                     * trusting the caller to give us the right one. */
    bool allocated_parameters = false;
    CFErrorRef localError = NULL;

    if (!poolp) {
        secerror("csr: pool allocation failure");
        return NULL;
    }

    NSSCertRequest certReq;
    memset(&certReq, 0, sizeof(certReq));

    /* version */
    unsigned char version = 0;
    certReq.reqInfo.version.Length = sizeof(version);
    certReq.reqInfo.version.Data = &version;

    /* subject */
    certReq.reqInfo.subject.rdns = make_subject(poolp, (CFArrayRef)subject, &localError);

    /* public key info */
    if (privateKey) {
        realPublicKey = SecKeyCopyPublicKey(privateKey);
    }
    if (!realPublicKey) {
        /* If we can't get the public key from the private key,
         * fall back to the public key provided by the caller. */
        secinfo("csr", "unable to create public key from private, using input public key");
        realPublicKey = CFRetainSafe(publicKey);
    }
    require_action_quiet(realPublicKey, out, secerror("csr: failed to get public key"));
    publicKeyData = make_public_key(realPublicKey, &certReq.reqInfo.subjectPublicKeyInfo, &allocated_parameters, &localError);
    require_action_quiet(publicKeyData, out, secerror("csr: failed to encode public key"));

    certReq.reqInfo.attributes = nss_attributes_from_parameters_dict(poolp, parameters);
    SecCmsArraySortByDER((void **)certReq.reqInfo.attributes, kSecAsn1AttributeTemplate, NULL);

    /* encode request info by itself to calculate signature */
    SecAsn1Item reqinfo = {};
    if (!SEC_ASN1EncodeItem(poolp, &reqinfo, &certReq.reqInfo, kSecAsn1CertRequestInfoTemplate)) {
        secerror("csr: failed to encode request info");
    }

    /* calculate signature and encode signature info */
    CFStringRef algorithm = NULL;
    if (parameters) {
        algorithm = CFDictionaryGetValue(parameters, kSecCMSSignHashAlgorithm);
    }
    signature = make_signature(reqinfo.Data, reqinfo.Length, privateKey, algorithm, &certReq.signatureAlgorithm, &localError);
    require_action_quiet(signature, out, secerror("csr: failed to create CSR signature"));
    certReq.signature.Data = (uint8_t *)CFDataGetBytePtr(signature);
    certReq.signature.Length = 8 * CFDataGetLength(signature);

    /* encode csr */
    SecAsn1Item cert_request = {};
    require_action_quiet(SEC_ASN1EncodeItem(poolp, &cert_request, &certReq, kSecAsn1CertRequestTemplate), out,
                         secerror("csr: failed to encode cert request"));
    csr = CFDataCreate(kCFAllocatorDefault, cert_request.Data, cert_request.Length);

out:
    if (allocated_parameters) {
        free(certReq.reqInfo.subjectPublicKeyInfo.algorithm.parameters.Data);
    }
    if (poolp) { PORT_FreeArena(poolp, PR_TRUE); }
    CFReleaseSafe(realPublicKey);
    CFReleaseSafe(publicKeyData);
    CFReleaseSafe(signature);
    if (localError) {
        secerror("csr: cert request encoding failed: %@", localError);
        CFReleaseNull(localError);
    }
    return csr;
}

static SecKeyAlgorithm determine_key_algorithm(bool isRsa, SecAsn1AlgId *algId) {
    SecKeyAlgorithm keyAlg = NULL;
    SecAsn1Oid oid = algId->algorithm;

    /* We don't check the parameters match the algorithm OID since there was some RFC confusion
     * about NULL or absent parameters. */
    if (isRsa) {
        if (oid.Length == oidSha1Rsa.length &&
            (0 == memcmp(oidSha1Rsa.data, oid.Data, oid.Length))) {
            keyAlg = kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1;
        } else if (oid.Length == oidSha256Rsa.length &&
                           (0 == memcmp(oidSha256Rsa.data, oid.Data, oid.Length))) {
            keyAlg = kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256;
        } else if (oid.Length == oidSha384Rsa.length &&
                   (0 == memcmp(oidSha384Rsa.data, oid.Data, oid.Length))) {
            keyAlg = kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA384;
        } else if (oid.Length == oidSha512Rsa.length &&
                   (0 == memcmp(oidSha512Rsa.data, oid.Data, oid.Length))) {
            keyAlg = kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA512;
        }
    } else {
        if (oid.Length == oidSha256Ecdsa.length &&
            (0 == memcmp(oidSha256Ecdsa.data, oid.Data, oid.Length))) {
            keyAlg = kSecKeyAlgorithmECDSASignatureMessageX962SHA256;
        } else if (oid.Length == oidSha384Ecdsa.length &&
                   (0 == memcmp(oidSha384Ecdsa.data, oid.Data, oid.Length))) {
            keyAlg = kSecKeyAlgorithmECDSASignatureMessageX962SHA384;
        } else if (oid.Length == oidSha512Ecdsa.length &&
                   (0 == memcmp(oidSha512Ecdsa.data, oid.Data, oid.Length))) {
            keyAlg = kSecKeyAlgorithmECDSASignatureMessageX962SHA512;
        }
    }

    if (!keyAlg) {
        secerror("csr: unsupported signature algorithm");
    }

    return keyAlg;
}

bool SecVerifyCertificateRequest(CFDataRef csr, SecKeyRef *publicKey,
    CFStringRef *challenge, CFDataRef *subject, CFDataRef *extensions)
{
    PRArenaPool *poolp = PORT_NewArena(1024);
    SecKeyRef candidatePublicKey = NULL;
    CFMutableDictionaryRef keyAttrs = NULL;
    CFDataRef keyData = NULL, signature = NULL, data = NULL;
    bool valid = false;
	NSSCertRequest decodedCertReq;
    NSS_SignedCertRequest undecodedCertReq;
	memset(&decodedCertReq, 0, sizeof(decodedCertReq));
    memset(&undecodedCertReq, 0, sizeof(undecodedCertReq));

    /* Decode the CSR */
    SecAsn1Item csr_item = { CFDataGetLength(csr), (uint8_t*)CFDataGetBytePtr(csr) };
    require_noerr_action_quiet(SEC_ASN1DecodeItem(poolp, &decodedCertReq, kSecAsn1CertRequestTemplate,
        &csr_item), out, secerror("csr: failed to decode CSR"));
    require_noerr_action_quiet(SEC_ASN1DecodeItem(poolp, &undecodedCertReq, kSecAsn1SignedCertRequestTemplate,
        &csr_item), out, secerror("csr: failed to decode CSR"));

    /* get public key */
    bool isRsa = true;
    if (decodedCertReq.reqInfo.subjectPublicKeyInfo.algorithm.algorithm.Length == oidRsa.length &&
        0 == memcmp(oidRsa.data, decodedCertReq.reqInfo.subjectPublicKeyInfo.algorithm.algorithm.Data, oidRsa.length)) {
        secdebug("csr", "rsa CSR");
        require_action(candidatePublicKey = SecKeyCreateRSAPublicKey(kCFAllocatorDefault,
                                                              decodedCertReq.reqInfo.subjectPublicKeyInfo.subjectPublicKey.Data,
                                                              decodedCertReq.reqInfo.subjectPublicKeyInfo.subjectPublicKey.Length / 8,
                                                              kSecKeyEncodingPkcs1), out,
                       secerror("csr: failed to create RSA public key"));
    } else if (decodedCertReq.reqInfo.subjectPublicKeyInfo.algorithm.algorithm.Length == oidEcPubKey.length &&
               0 == memcmp(oidEcPubKey.data, decodedCertReq.reqInfo.subjectPublicKeyInfo.algorithm.algorithm.Data, oidEcPubKey.length)) {
        secdebug("csr", "ec CSR");
        keyData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
                                              decodedCertReq.reqInfo.subjectPublicKeyInfo.subjectPublicKey.Data,
                                              decodedCertReq.reqInfo.subjectPublicKeyInfo.subjectPublicKey.Length / 8,
                                              kCFAllocatorNull);
        keyAttrs = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                             &kCFTypeDictionaryValueCallBacks);
        CFDictionaryAddValue(keyAttrs, kSecAttrKeyType, kSecAttrKeyTypeECSECPrimeRandom);
        CFDictionaryAddValue(keyAttrs, kSecAttrKeyClass, kSecAttrKeyClassPublic);
        require_action(candidatePublicKey = SecKeyCreateWithData(keyData, keyAttrs, NULL),
                       out, secerror("csr: failed to create EC public key"));
        isRsa = false;
    } else {
        secerror("csr: unsupported public key algorithm in CSR");
        goto out;
    }

    /* get the signature algorithm */
    SecAsn1AlgId algId = decodedCertReq.signatureAlgorithm;
    /* check the parameters are NULL or absent */
    require_action(algId.parameters.Length == asn1_null.Length || algId.parameters.Length == 0, out,
                   secerror("csr: non-empty signature algorithm parameters"));
    require_action(algId.parameters.Length == 0 || 0 == memcmp(asn1_null.Data, algId.parameters.Data, asn1_null.Length), out,
                   secerror("csr: non-empty signature algorithm parameters"));
    SecKeyAlgorithm alg = determine_key_algorithm(isRsa, &algId);

    /* verify signature */
    signature = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, undecodedCertReq.signature.Data,
                                            undecodedCertReq.signature.Length / 8, kCFAllocatorNull);
    data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, undecodedCertReq.certRequestBlob.Data,
                                       undecodedCertReq.certRequestBlob.Length, kCFAllocatorNull);
    require_quiet(alg && signature && data, out);
    require_action_quiet(SecKeyVerifySignature(candidatePublicKey, alg, data, signature, NULL), out,
                         secerror("csr: failed to verify signature in CSR"));

    SecAsn1Item subject_item = { 0 }, extensions_item = { 0 }, challenge_item = { 0 };
    require_action_quiet(SEC_ASN1EncodeItem(poolp, &subject_item,
                                            &decodedCertReq.reqInfo.subject, kSecAsn1NameTemplate), out,
                         secerror("csr: failed to decode CSR subject name"));

    if (*decodedCertReq.reqInfo.attributes) {
        uint32_t ix;
        for (ix = 0; decodedCertReq.reqInfo.attributes[ix]; ix++) {
            NSS_Attribute *attr = decodedCertReq.reqInfo.attributes[ix];
            if ( (sizeof(pkcs9ChallengePassword) == attr->attrType.Length) &&
                !memcmp(pkcs9ChallengePassword, attr->attrType.Data, sizeof(pkcs9ChallengePassword))) {
                secdebug("csr", "challenge password found");
                challenge_item = *attr->attrValue[0];
            } else if ( (sizeof(pkcs9ExtensionsRequested) == attr->attrType.Length) &&
                       !memcmp(pkcs9ExtensionsRequested, attr->attrType.Data, sizeof(pkcs9ExtensionsRequested))) {
                secdebug("csr", "extensions requested");
                extensions_item = *attr->attrValue[0];
            }
        }
    }

    if (subject && subject_item.Length) {
        *subject = CFDataCreate(kCFAllocatorDefault, subject_item.Data, subject_item.Length);
    }
    if (extensions && extensions_item.Length) {
        *extensions = CFDataCreate(kCFAllocatorDefault, extensions_item.Data, extensions_item.Length);
    }
    if (challenge && challenge_item.Length) {
        SecAsn1Item string = { 0 };
        SECStatus rv = SEC_ASN1DecodeItem(poolp, &string, kSecAsn1UTF8StringTemplate, &challenge_item);
        if (rv) {
            secinfo("csr", "challenge failed to decode with UTF8, attempting PrintableString");
            rv = SEC_ASN1DecodeItem(poolp, &string, kSecAsn1PrintableStringTemplate, &challenge_item);
        }
        if (!rv) {
            *challenge = CFStringCreateWithBytes(kCFAllocatorDefault, string.Data, string.Length, kCFStringEncodingUTF8, false);
        } else {
            secerror("csr: failed to decode challenge");
            *challenge = NULL;
        }
    }
    if (publicKey) {
        *publicKey = candidatePublicKey;
        candidatePublicKey = NULL;
    }
    valid = true;
out:
    CFReleaseSafe(candidatePublicKey);
    CFReleaseNull(keyAttrs);
    CFReleaseNull(keyData);
    CFReleaseNull(data);
    CFReleaseNull(signature);
    if (poolp) { PORT_FreeArena(poolp, PR_TRUE); }
    return valid;
}

SecCertificateRef
SecGenerateSelfSignedCertificateWithError(CFArrayRef subject, CFDictionaryRef parameters,
                                          SecKeyRef __unused publicKey, SecKeyRef privateKey, 
                                          CFErrorRef *error)
{
    SecCertificateRef cert = NULL;
    PRArenaPool *poolp = PORT_NewArena(1024);
    CFDictionaryRef pubkey_attrs = NULL;
    CFDataRef publicKeyData = NULL, signature = NULL;
    SecKeyRef realPublicKey = NULL; /* We calculate this from the private key rather than
                                     * trusting the caller to give us the right one. */
    bool allocated_parameters = false;
    CFErrorRef localError = NULL;

    if (!poolp) {
        SecError(errSecMemoryError, error, CFSTR("pool allocation failure"));
        return NULL;
    }

    NSS_Certificate cert_tmpl;
    memset(&cert_tmpl, 0, sizeof(cert_tmpl));

    /* version */
    unsigned char version = 2;
    cert_tmpl.tbs.version.Length = sizeof(version);
    cert_tmpl.tbs.version.Data = &version;

    /* serialno */
    CFDataRef serialData = parameters ? CFDictionaryGetValue(parameters, kSecCertificateSerialNumber) : NULL;
    if (serialData) {
        if (CFGetTypeID(serialData) == CFDataGetTypeID()) {
            CFIndex length = CFDataGetLength(serialData);
            if (length < 1 || length > 20) {
                serialData = NULL; /* invalid, per RFC 5280 4.1.2.2 */
            } else {
                CFRetainSafe(serialData);
            }
        } else {
            serialData = NULL;
        }
    }
    if (!serialData) {
        const UInt8 serialNumber = 1;
        serialData = CFDataCreate(NULL, &serialNumber, (CFIndex)sizeof(UInt8));
        require_action_quiet(serialData, out, SecError(errSecMemoryError, &localError, CFSTR("failed to allocate serial number")));
    }
    cert_tmpl.tbs.serialNumber.Length = (size_t)CFDataGetLength(serialData);
    cert_tmpl.tbs.serialNumber.Data = (uint8_t *)CFDataGetBytePtr(serialData);

    /* subject/issuer */
    cert_tmpl.tbs.issuer.rdns = make_subject(poolp, (CFArrayRef)subject, &localError);
    cert_tmpl.tbs.subject.rdns = cert_tmpl.tbs.issuer.rdns;

    CFNumberRef lifetimeNum = parameters ? CFDictionaryGetValue(parameters, kSecCertificateLifetime) : NULL;
    int64_t lifetime = 3600*24*365;
    if (lifetimeNum) {
        CFNumberGetValue(lifetimeNum, kCFNumberLongLongType, &lifetime);
    }
    CFAbsoluteTime currentTime = CFAbsoluteTimeGetCurrent();
    require_noerr_action_quiet(SecAsn1EncodeTime(poolp, currentTime, &cert_tmpl.tbs.validity.notBefore), out,
                               SecError(errSecInternal, &localError, CFSTR("failed to encode notBefore")));
    require_noerr_action_quiet(SecAsn1EncodeTime(poolp, currentTime + lifetime, &cert_tmpl.tbs.validity.notAfter), out,
                               SecError(errSecInternal, &localError, CFSTR("failed to encode notAfter")));

    /* extensions */
    require_action_quiet(extensions_from_parameters(poolp, parameters, &cert_tmpl.tbs.extensions, &localError), out,
                         SecError(errSecInternal, &localError, CFSTR("failed to encode extensions")));

    /* encode public key */
    if (privateKey) {
        realPublicKey = SecKeyCopyPublicKey(privateKey);
    }
    require_action_quiet(realPublicKey, out, SecError(errSecInternal, &localError, CFSTR("failed get public key")));
    publicKeyData = make_public_key(realPublicKey, &cert_tmpl.tbs.subjectPublicKeyInfo, &allocated_parameters, &localError);
    require_action_quiet(publicKeyData, out, SecError(errSecInternal, &localError, CFSTR("failed to encode public key")));

    /* encode the signature algorithm info */
    CFStringRef algorithm = NULL;
    if (parameters) {
        algorithm = CFDictionaryGetValue(parameters, kSecCMSSignHashAlgorithm);
    }
    signature = make_signature(NULL, 0, privateKey, algorithm, &cert_tmpl.tbs.signature, &localError);
    CFReleaseNull(signature);

    /* encode request info by itself to calculate signature */
    SecAsn1Item tbscert = {};
    if (!SEC_ASN1EncodeItem(poolp, &tbscert, &cert_tmpl.tbs, kSecAsn1TBSCertificateTemplate)) {
        SecError(errSecInternal, &localError, CFSTR("failed to encode TBS"));
    }

    /* calculate signature and encode signature algorithm info */
    signature = make_signature(tbscert.Data, tbscert.Length, privateKey, algorithm, &cert_tmpl.signatureAlgorithm, &localError);
    require_action_quiet(signature, out, SecError(errSecInvalidSignature, &localError, CFSTR("failed to make signature")));
    cert_tmpl.signature.Data = (uint8_t *)CFDataGetBytePtr(signature);
    cert_tmpl.signature.Length = CFDataGetLength(signature) * 8;

    /* encode cert */
    SecAsn1Item signed_cert = {};
    require_action_quiet(SEC_ASN1EncodeItem(poolp, &signed_cert, &cert_tmpl,
                                            kSecAsn1SignedCertTemplate), out,
                         SecError(errSecInternal, &localError, CFSTR("failed to encode certificate")));
    cert = SecCertificateCreateWithBytes(kCFAllocatorDefault,
                                         signed_cert.Data, signed_cert.Length);
    if (!cert) {
        SecError(errSecInternal, &localError, CFSTR("failed to decode encoded certificate"));
    }

out:
    if (allocated_parameters) {
        free(cert_tmpl.tbs.subjectPublicKeyInfo.algorithm.parameters.Data);
    }
    if (poolp) { PORT_FreeArena(poolp, PR_TRUE); }
    CFReleaseSafe(realPublicKey);
    CFReleaseSafe(pubkey_attrs);
    CFReleaseNull(publicKeyData);
    CFReleaseNull(signature);
    CFReleaseNull(serialData);
    if (localError) {
        secerror("csr: failed to generate self-signed certificate %@", localError);
        CFReleaseNull(cert);
        if (error) { CFAssignRetained(*error, localError); }
    }
    return cert;
}

SecCertificateRef
SecGenerateSelfSignedCertificate(CFArrayRef subject, CFDictionaryRef parameters,
                                 SecKeyRef publicKey, SecKeyRef privateKey)
{
    return SecGenerateSelfSignedCertificateWithError(subject, parameters, publicKey, privateKey, NULL);
}

SecCertificateRef
SecIdentitySignCertificate(SecIdentityRef issuer, CFDataRef serialno,
                           SecKeyRef publicKey, CFTypeRef subject, CFTypeRef extensions) {
    return SecIdentitySignCertificateWithAlgorithm(issuer, serialno, publicKey, subject, extensions, NULL);
}

SecCertificateRef
SecIdentitySignCertificateWithAlgorithm(SecIdentityRef issuer, CFDataRef serialno,
    SecKeyRef publicKey, CFTypeRef subject, CFTypeRef extensions, CFStringRef hashingAlgorithm)
{
    if (hashingAlgorithm) {
        CFMutableDictionaryRef parameters = CFDictionaryCreateMutable(NULL, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionaryAddValue(parameters, kSecCMSSignHashAlgorithm, hashingAlgorithm);
        SecCertificateRef result = SecIdentitySignCertificateWithParameters(issuer, serialno, publicKey, subject, extensions, parameters);
        CFReleaseNull(parameters);
        return result;
    } else {
        return SecIdentitySignCertificateWithParameters(issuer, serialno, publicKey, subject, extensions, NULL);
    }
}

SecCertificateRef
SecIdentitySignCertificateWithParameters(SecIdentityRef issuer, CFDataRef serialno,
    SecKeyRef publicKey, CFTypeRef subject, CFTypeRef extensions, CFDictionaryRef parameters)
{
    SecCertificateRef cert = NULL;
    SecKeyRef privateKey = NULL;
    CFStringRef hashingAlgorithm = parameters ? CFDictionaryGetValue(parameters, kSecCMSSignHashAlgorithm) : NULL;
    bool allocated_parameters = false;
    SecCertificateRef issuer_cert = NULL;
    CFDataRef issuer_name = NULL;
    CFErrorRef localError = NULL;

    PRArenaPool *poolp = PORT_NewArena(1024);
    CFDataRef publicKeyData = NULL, signature = NULL;
    if (!poolp) {
        secerror("csr: pool allocation failure");
        return NULL;
    }

    NSS_Certificate cert_tmpl;
    memset(&cert_tmpl, 0, sizeof(cert_tmpl));

    /* version */
    unsigned char version = 2;
    cert_tmpl.tbs.version.Length = sizeof(version);
    cert_tmpl.tbs.version.Data = &version;

    /* serialno */
    cert_tmpl.tbs.serialNumber.Length = CFDataGetLength(serialno);
    cert_tmpl.tbs.serialNumber.Data = (uint8_t*)CFDataGetBytePtr(serialno);

    /* subject/issuer */
    if (isArray(subject)) {
        cert_tmpl.tbs.subject.rdns = make_subject(poolp, (CFArrayRef)subject, &localError);
    } else if (isData(subject)) {
        SecAsn1Item subject_item = { CFDataGetLength(subject), (uint8_t*)CFDataGetBytePtr(subject) };
        require_noerr_action_quiet(SEC_ASN1DecodeItem(poolp, &cert_tmpl.tbs.subject.rdns, kSecAsn1NameTemplate, &subject_item), out,
                                   secerror("csr: failed to decode input subject"));
    } else {
        secerror("csr: unsupported subject CFType");
        goto out;
    }

    require_noerr_action(SecIdentityCopyCertificate(issuer, &issuer_cert), out, secerror("csr: failed to get identity cert"));
    issuer_name = SecCertificateCopySubjectSequence(issuer_cert);
    SecAsn1Item issuer_item = { CFDataGetLength(issuer_name), (uint8_t*)CFDataGetBytePtr(issuer_name) };
    require_noerr_action_quiet(SEC_ASN1DecodeItem(poolp, &cert_tmpl.tbs.issuer.rdns,
        kSecAsn1NameTemplate, &issuer_item), out, secerror("csr: failed to decoder subject name from CA identity"));
    CFReleaseNull(issuer_name);

    CFNumberRef lifetimeNum = parameters ? CFDictionaryGetValue(parameters, kSecCertificateLifetime) : NULL;
    int64_t lifetime = 3600*24*365;
    if (lifetimeNum) {
        CFNumberGetValue(lifetimeNum, kCFNumberLongLongType, &lifetime);
    }
    CFAbsoluteTime currentTime = CFAbsoluteTimeGetCurrent();
    require_noerr_action_quiet(SecAsn1EncodeTime(poolp, currentTime, &cert_tmpl.tbs.validity.notBefore), out,
                               secerror("csr: failed to encode notBefore"));
    require_noerr_action_quiet(SecAsn1EncodeTime(poolp, currentTime + lifetime, &cert_tmpl.tbs.validity.notAfter), out,
                               secerror("csr: failed to encode notBefore"));

    require_action_quiet(extensions_from_parameters_or_request_extensions(poolp, parameters, extensions, &cert_tmpl.tbs.extensions, &localError), out,
                  secerror("csr: failed to encode extensions"));

    /* subject public key info */
    publicKeyData = make_public_key(publicKey, &cert_tmpl.tbs.subjectPublicKeyInfo, &allocated_parameters, &localError);
    require_action_quiet(publicKeyData, out, secerror("csr: failed to encode public key"));

    /* encode the signature algorithm info */
    require_noerr_action_quiet(SecIdentityCopyPrivateKey(issuer, &privateKey), out, secerror("csr: failed to get CA private key"));
    signature = make_signature(NULL, 0, privateKey, hashingAlgorithm, &cert_tmpl.tbs.signature, &localError);
    CFReleaseNull(signature);

    /* encode request info by itself to calculate signature */
    SecAsn1Item tbscert = {};
    if (!SEC_ASN1EncodeItem(poolp, &tbscert, &cert_tmpl.tbs, kSecAsn1TBSCertificateTemplate)) {
        secerror("csr: failed to encode TBS");
    }

    /* calculate signature and encode signature algorithm info */
    signature = make_signature(tbscert.Data, tbscert.Length, privateKey, hashingAlgorithm, &cert_tmpl.signatureAlgorithm, &localError);
    require_action_quiet(signature, out, secerror("csr: failed to make signature"));
    cert_tmpl.signature.Data = (uint8_t *)CFDataGetBytePtr(signature);
    cert_tmpl.signature.Length = CFDataGetLength(signature) * 8;

    /* encode cert */
    SecAsn1Item signed_cert = {};
    require_action_quiet(SEC_ASN1EncodeItem(poolp, &signed_cert, &cert_tmpl,
                                            kSecAsn1SignedCertTemplate), out,
                         secerror("csr: failed to encode certificate"));
    cert = SecCertificateCreateWithBytes(kCFAllocatorDefault,
                                         signed_cert.Data, signed_cert.Length);
    if (!cert) {
        secerror("csr: failed to decode encoded certificate");
    }

out:
    if (allocated_parameters) {
        free(cert_tmpl.tbs.subjectPublicKeyInfo.algorithm.parameters.Data);
    }
    CFReleaseNull(issuer_cert);
    CFReleaseNull(issuer_name);
    CFReleaseSafe(privateKey);
    if (poolp) { PORT_FreeArena(poolp, PR_TRUE); }
    CFReleaseSafe(publicKeyData);
    CFReleaseSafe(signature);
    if (localError) {
        secerror("csr: cert issuance failed: %@", localError);
        CFReleaseNull(localError);
    }
    return cert;
}

CF_RETURNS_RETAINED
CFDataRef
SecGenerateCertificateRequestSubject(SecCertificateRef ca_certificate, CFArrayRef subject)
{
    CFMutableDataRef sequence = NULL;
    PRArenaPool *poolp = PORT_NewArena(1024);
    if (!poolp) {
        secerror("csr: pool allocation failure");
        return NULL;
    }

    /*
        Going against the spec here:

            3.2.3.  GetCertInitial

           The messageData for this type consists of a DER-encoded
           IssuerAndSubject (Section 3.2.3.1).  The issuer is set to the
           issuerName from the certification authority from which we are issued
           certificates.  The Subject is set to the SubjectName we used when
           requesting the certificate.

        That clearly says use the issuer of the cert issuing certificate.  Since
        it is combined with the subject of the to-be-issued certificate, that
        seems a mistake.  If we take the subject of the issuer and the subject
        of the certificate we're interested in, we get the issuer and subject
        the certificate to be returned will have.

    */
    CFDataRef issuer_sequence = SecCertificateCopySubjectSequence(ca_certificate);
    SecAsn1Item subject_item = { 0 };
    SecAsn1Item issuer_item = { CFDataGetLength(issuer_sequence), (uint8_t*)CFDataGetBytePtr(issuer_sequence) };
    NSS_Name nss_subject = { make_subject(poolp, subject, NULL) };
    require_action_quiet(SEC_ASN1EncodeItem(poolp, &subject_item, &nss_subject, kSecAsn1NameTemplate), out,
                         secerror("csr: failed to encode subject"));

    DERSize sequence_length = DERLengthOfLength(subject_item.Length + issuer_item.Length);
    DERSize seq_len_length = subject_item.Length + issuer_item.Length + 1 /* SEQUENCE */ +
        sequence_length;
    sequence = CFDataCreateMutable(kCFAllocatorDefault, 0);
    CFDataSetLength(sequence, seq_len_length);
    uint8_t *sequence_ptr = CFDataGetMutableBytePtr(sequence);
    *sequence_ptr++ = 0x30; //ONE_BYTE_ASN1_CONSTR_SEQUENCE;
    require_noerr_action_quiet(DEREncodeLength(subject_item.Length + issuer_item.Length, sequence_ptr, &sequence_length), out,
                               secerror("csr: failed to encode outer subject sequence"));
    sequence_ptr += sequence_length;
    memcpy(sequence_ptr, issuer_item.Data, issuer_item.Length);
    memcpy(sequence_ptr + issuer_item.Length, subject_item.Data, subject_item.Length);

out:
    CFReleaseSafe(issuer_sequence);
    if (poolp) { PORT_FreeArena(poolp, PR_TRUE); }
    return sequence;
}
