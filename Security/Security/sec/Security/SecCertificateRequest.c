/*
 * Copyright (c) 2008-2009,2012-2014 Apple Inc. All Rights Reserved.
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

#include <TargetConditionals.h>
#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
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

#include <utilities/SecCFWrappers.h>

#include <AssertMacros.h>

#include "SecCertificateRequest.h"

CFTypeRef kSecOidCommonName = CFSTR("CN");
CFTypeRef kSecOidCountryName = CFSTR("C");
CFTypeRef kSecOidStateProvinceName = CFSTR("ST");
CFTypeRef kSecOidLocalityName = CFSTR("L");
CFTypeRef kSecOidOrganization = CFSTR("O");
CFTypeRef kSecOidOrganizationalUnit = CFSTR("OU");
//CFTypeRef kSecOidEmailAddress = CFSTR("1.2.840.113549.1.9.1");
// keep natural order: C > ST > L > O > OU > CN > Email

const unsigned char SecASN1PrintableString = SEC_ASN1_PRINTABLE_STRING;
const unsigned char SecASN1UTF8String = SEC_ASN1_UTF8_STRING;

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

Consider using IA5String for email address
*/

static inline bool printable_string(CFStringRef string)
{
    bool result = true;
    
    CFCharacterSetRef printable_charset = 
        CFCharacterSetCreateWithCharactersInString(kCFAllocatorDefault,
            CFSTR("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                    "abcdefghijklmnopqrstuvwxyz"
                    "0123456789 '()+,-./:=?"));
    CFCharacterSetRef not_printable_charset = 
        CFCharacterSetCreateInvertedSet(kCFAllocatorDefault, printable_charset);
    CFRange found;
    if (CFStringFindCharacterFromSet(string, not_printable_charset, 
        CFRangeMake(0, CFStringGetLength(string)), 0, &found))
            result = false;

    CFReleaseSafe(printable_charset);
    CFReleaseSafe(not_printable_charset);

    return result;
}

static bool make_nss_atv(PRArenaPool *poolp, 
    const void * oid, const void * value, const unsigned char type_in, NSS_ATV *nss_atv)
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
            if (!CFStringGetCString(value, buffer, length, kCFStringEncodingUTF8))
                return false;
            if (type && type != SecASN1UTF8String)
                return false;
            type = SecASN1UTF8String;
        }
        else {
            if (!type || type == SecASN1PrintableString) {
                if (!printable_string(value))
                    type = SEC_ASN1_IA5_STRING;
                else
                    type = SEC_ASN1_PRINTABLE_STRING;
            }
        }
        length = strlen(buffer);
    }
    else if (CFGetTypeID(value) == CFDataGetTypeID()) {
        /* will remain valid for the duration of the operation, still maybe copy into pool */
        length = CFDataGetLength(value);
        buffer = (char *)CFDataGetBytePtr(value);
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
        } else {
            oid_data = oid_der_data(poolp, oid, &oid_length);
            require(oid_data, out);
        }
    } else if (CFGetTypeID(oid) == CFDataGetTypeID()) {
        /* will remain valid for the duration of the operation, still maybe copy into pool */
        oid_length = CFDataGetLength(oid);
        oid_data = (uint8_t *)CFDataGetBytePtr(oid);
    }    
    NSS_ATV stage_nss_atv = { { oid_length, oid_data }, 
        { { length, (uint8_t*)buffer }, type } };
    *nss_atv = stage_nss_atv;
    return true;
out:
    return false;
}

static NSS_RDN **make_subject(PRArenaPool *poolp, CFArrayRef subject)
{
    if (!subject)
        return NULL;
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
            if ((CFArrayGetCount(atv) != 2) 
                || !make_nss_atv(poolp, CFArrayGetValueAtIndex(atv, 0),
                        CFArrayGetValueAtIndex(atv, 1), 0, &atvs[atv_ix]))
                return NULL;
        }
    }
    return rdnps;
}

struct make_general_names_context {
    PRArenaPool *poolp;
    SecAsn1Item *names;
    uint32_t count;
    uint32_t capacity;
};

static void make_general_names(const void *key, const void *value, void *context)
{
    struct make_general_names_context *gn = (struct make_general_names_context *)context;
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
    }

    require(entry_count > 0, out);
    
    require(key,out);
    require(CFGetTypeID(key) == CFStringGetTypeID(), out);

    if (!gn->names || (gn->count == gn->capacity)) {
        uint32_t capacity = gn->capacity;
        if (capacity)
            capacity *= 2;
        else
            capacity = 10;
        
        void * new_array = PORT_ArenaZNewArray(gn->poolp, SecAsn1Item, capacity);
        if (gn->names)
            memcpy(new_array, gn->names, gn->capacity);
        gn->names = new_array;
        gn->capacity = capacity;
    }

    NSS_GeneralName general_name_item = { { }, -1 };
    if (kCFCompareEqualTo == CFStringCompare(CFSTR("dNSName"), key, kCFCompareCaseInsensitive))
        general_name_item.tag = NGT_DNSName;
    else if (kCFCompareEqualTo == CFStringCompare(CFSTR("rfc822Name"), key, kCFCompareCaseInsensitive))
        general_name_item.tag = NGT_RFC822Name;
    else if (kCFCompareEqualTo == CFStringCompare(CFSTR("uniformResourceIdentifier"), key, kCFCompareCaseInsensitive))
        general_name_item.tag = NGT_URI;
	else if (kCFCompareEqualTo == CFStringCompare(CFSTR("ntPrincipalName"), key, kCFCompareCaseInsensitive))
	{
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
        typedef struct {
               SecAsn1Oid typeId;
               SecAsn1Item value;
        } nt_principal_other_name;
        nt_principal_other_name name = {};

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

        size_t length = 0;
        char *buffer = NULL;

		require(gn_value, out);
        require(CFGetTypeID(gn_value) == CFStringGetTypeID(), out);
        length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value),
            kCFStringEncodingUTF8);
        buffer = PORT_ArenaAlloc(gn->poolp, length);
        if (!CFStringGetCString(value, buffer, length, kCFStringEncodingUTF8))
            goto out;

        name.typeId.Length = sizeof(nt_principal_oid);
        name.typeId.Data = nt_principal_oid;
        name.value.Length = strlen(buffer);
        name.value.Data = (uint8_t*)buffer;
		SEC_ASN1EncodeItem(gn->poolp, &gn->names[gn->count], &name, my_other_name_template_cons);
        gn->count++;

        /* We already encoded the value for the general name */
		goto out;
	}
	else
        goto out;
    
    if (gn_values) {
        for (entry_ix = 0; entry_ix < entry_count; entry_ix++) {
            CFTypeRef entry_value = CFArrayGetValueAtIndex(gn_values, entry_ix);
            CFIndex buffer_size = CFStringGetMaximumSizeForEncoding(CFStringGetLength((CFStringRef)entry_value),
                kCFStringEncodingUTF8); /* we only allow ASCII => only expect IA5Strings */
            char *buffer = (char *)PORT_ArenaZNewArray(gn->poolp, uint8_t, buffer_size);
            require(CFStringGetCString((CFStringRef)entry_value, buffer, buffer_size, kCFStringEncodingASCII), out);
            general_name_item.item.Data = (uint8_t*)buffer;
            general_name_item.item.Length = strlen(buffer);
            SEC_ASN1EncodeItem(gn->poolp, &gn->names[gn->count], &general_name_item, kSecAsn1GeneralNameTemplate);
            gn->count++;
        }
    } else if (gn_value) {
        CFIndex buffer_size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(gn_value),
            kCFStringEncodingUTF8);
        char *buffer = (char *)PORT_ArenaZNewArray(gn->poolp, uint8_t, buffer_size);
        require(CFStringGetCString(gn_value, buffer, buffer_size, kCFStringEncodingASCII), out);
        general_name_item.item.Data = (uint8_t*)buffer;
        general_name_item.item.Length = strlen(buffer);
        SEC_ASN1EncodeItem(gn->poolp, &gn->names[gn->count], &general_name_item, kSecAsn1GeneralNameTemplate);
        gn->count++;
    }
out:
    return;
}

static SecAsn1Item make_subjectAltName_extension(PRArenaPool *poolp, CFDictionaryRef subjectAltNames)
{
    SecAsn1Item subjectAltExt = {};
    
    struct make_general_names_context context = { poolp, NULL, 0 };
    CFDictionaryApplyFunction(subjectAltNames, make_general_names, &context);

    // all general names in a sequence:
    uint32_t ix;
    SecAsn1Item **general_names = PORT_ArenaZNewArray(poolp, SecAsn1Item *, context.count + 1);
    for (ix = 0; ix < context.count; ix++)
        general_names[ix] = &context.names[ix];
    NSS_GeneralNames gnames = { general_names };
    SEC_ASN1EncodeItem(poolp, &subjectAltExt, &gnames, kSecAsn1GeneralNamesTemplate);

    return subjectAltExt;
}

CFTypeRef kSecCSRChallengePassword = CFSTR("csrChallengePassword");
CFTypeRef kSecSubjectAltName = CFSTR("subjectAltName");
CFTypeRef kSecCertificateKeyUsage = CFSTR("keyUsage");
CFTypeRef kSecCSRBasicContraintsPathLen = CFSTR("basicConstraints");
CFTypeRef kSecCertificateExtensions = CFSTR("certificateExtensions");
CFTypeRef kSecCertificateExtensionsEncoded = CFSTR("certificateExtensionsEncoded");

static const uint8_t pkcs9ExtensionsRequested[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 14 };
static const uint8_t pkcs9ChallengePassword[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 7 };

static const uint8_t encoded_asn1_true = 0xFF;
static const SecAsn1Item asn1_true =
    { sizeof(encoded_asn1_true), (uint8_t*)&encoded_asn1_true };

__unused static inline uint32_t highest_bit(uint32_t n)
{
    return ((n) >> 16 ? ((n)>>=16, 16) : 0) + \
            ((n) >> 8 ? ((n)>>=8, 8) : 0) + \
            ((n) >> 4 ? ((n)>>=4, 4) : 0) + \
            ((n) >> 2 ? ((n)>>=2, 2) : 0) + \
            ((n) >> 1 ? ((n)>>=1, 1) : 0) + \
            (n);
}

struct add_custom_extension_args {
    PLArenaPool *poolp;
    NSS_CertExtension *csr_extension;
    uint32_t num_extensions;
    uint32_t max_extensions;
    bool encodeData;
};

static void add_custom_extension(const void *key, const void *value, void *context)
{
    struct add_custom_extension_args *args = (struct add_custom_extension_args *)context;
    size_t der_data_len;

    require(args->num_extensions < args->max_extensions, out);

    uint8_t * der_data = oid_der_data(args->poolp, key, &der_data_len);
    SecAsn1Item encoded_value = {};

    if (CFGetTypeID(value) == CFStringGetTypeID()) {
      	if (!args->encodeData) {
	  goto out;
	}
        CFIndex buffer_size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value), kCFStringEncodingUTF8);
        char *buffer = (char *)PORT_ArenaZNewArray(args->poolp, uint8_t, buffer_size);
        if (!CFStringGetCString(value, buffer, buffer_size, kCFStringEncodingUTF8))
            goto out;

        SecAsn1Item buffer_item = { strlen(buffer), (uint8_t*)buffer };
        SEC_ASN1EncodeItem(args->poolp, &encoded_value, &buffer_item, kSecAsn1UTF8StringTemplate);
    } else if (CFGetTypeID(value) == CFDataGetTypeID()) {
      	if (args->encodeData) {
	  	SecAsn1Item data_item = { CFDataGetLength(value), (uint8_t*)CFDataGetBytePtr(value) };
		SEC_ASN1EncodeItem(args->poolp, &encoded_value, &data_item, kSecAsn1OctetStringTemplate);
	}
	else {
	  	encoded_value.Length = CFDataGetLength(value);
	  	encoded_value.Data = (uint8_t*)CFDataGetBytePtr(value);
	}
    } else
        goto out;


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
NSS_CertExtension **
extensions_from_parameters(PRArenaPool *poolp, CFDictionaryRef parameters)
{
    uint32_t num_extensions = 0, max_extensions = 10;
    NSS_CertExtension **csr_extensions = PORT_ArenaZNewArray(poolp, NSS_CertExtension *, max_extensions + 1); /* NULL terminated array */
    NSS_CertExtension *csr_extension = PORT_ArenaZNewArray(poolp, NSS_CertExtension, max_extensions);

    CFNumberRef basic_contraints_num = CFDictionaryGetValue(parameters, kSecCSRBasicContraintsPathLen);
    if (basic_contraints_num) {
        NSS_BasicConstraints basic_contraints = { asn1_true, {} };
        uint8_t path_len;
        
        int basic_contraints_path_len = 0;
        require(CFNumberGetValue(basic_contraints_num, kCFNumberIntType, &basic_contraints_path_len), out);
        if (basic_contraints_path_len >= 0 && basic_contraints_path_len < 256) {
            path_len = (uint8_t)basic_contraints_path_len;
            basic_contraints.pathLenConstraint.Length = sizeof(path_len);
            basic_contraints.pathLenConstraint.Data = &path_len;
        }
        
        csr_extension[num_extensions].extnId.Data = oidBasicConstraints.data;
        csr_extension[num_extensions].extnId.Length = oidBasicConstraints.length;
        csr_extension[num_extensions].critical = asn1_true;
        
        SEC_ASN1EncodeItem(poolp, &csr_extension[num_extensions].value, &basic_contraints, 
            kSecAsn1BasicConstraintsTemplate);
        require(num_extensions++ < max_extensions, out);
    }

    CFDictionaryRef subject_alternate_names = CFDictionaryGetValue(parameters, kSecSubjectAltName);
    if (subject_alternate_names) {
        require(CFGetTypeID(subject_alternate_names) == CFDictionaryGetTypeID(), out);
        csr_extension[num_extensions].value = make_subjectAltName_extension(poolp, subject_alternate_names);
        /* set up subjectAltName cert request value */
        csr_extension[num_extensions].extnId.Length = oidSubjectAltName.length;
        csr_extension[num_extensions].extnId.Data = oidSubjectAltName.data;
        require(num_extensions++ < max_extensions, out);
    }

    CFNumberRef key_usage_requested = CFDictionaryGetValue(parameters, kSecCertificateKeyUsage);
    SecAsn1Item key_usage_asn1_value = { 0 };
    if (key_usage_requested) {
        int key_usage_value;
        require(CFNumberGetValue(key_usage_requested, kCFNumberIntType, &key_usage_value), out);
        if (key_usage_value > 0) {
            uint32_t key_usage_value_be = 0, key_usage_mask = 1<<31;
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
            SEC_ASN1EncodeItem(poolp, &key_usage_asn1_value, &key_usage_input, kSecAsn1BitStringTemplate);

            csr_extension[num_extensions].extnId.Data = oidKeyUsage.data;
            csr_extension[num_extensions].extnId.Length = oidKeyUsage.length;
            csr_extension[num_extensions].critical = asn1_true;
            csr_extension[num_extensions].value = key_usage_asn1_value;
            require(num_extensions++ < max_extensions, out);
        }
    }

    CFDictionaryRef custom_extension_requested = CFDictionaryGetValue(parameters, kSecCertificateExtensions);
    if (custom_extension_requested) {
        require(CFGetTypeID(custom_extension_requested) == CFDictionaryGetTypeID(), out);
        struct add_custom_extension_args args = {
            poolp,
            csr_extension,
            num_extensions,
            max_extensions,
	    true
        };
        CFDictionaryApplyFunction(custom_extension_requested, add_custom_extension, &args);
        num_extensions = args.num_extensions;
    }

    CFDictionaryRef custom_encoded_extension_requested = CFDictionaryGetValue(parameters, kSecCertificateExtensionsEncoded);
    if (custom_encoded_extension_requested) {
        require(CFGetTypeID(custom_encoded_extension_requested) == CFDictionaryGetTypeID(), out);
        struct add_custom_extension_args args = {
            poolp,
            csr_extension,
            num_extensions,
            max_extensions,
	    false
        };
        CFDictionaryApplyFunction(custom_encoded_extension_requested, add_custom_extension, &args);
        num_extensions = args.num_extensions;
    }

    /* extensions requested (subjectAltName, keyUsage) sequence of extension sequences */
    uint32_t ix = 0;
    for (ix = 0; ix < num_extensions; ix++)
        csr_extensions[ix] = csr_extension[ix].extnId.Length ? &csr_extension[ix] : NULL;

out:
    return csr_extensions;
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
    if (!parameters)
        return NULL;
    uint32_t num_attrs = 0;

    CFStringRef challenge = CFDictionaryGetValue(parameters, kSecCSRChallengePassword);
    NSS_Attribute challenge_password_attr = {};
    if (challenge) {
        CFIndex buffer_size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(challenge),
            kCFStringEncodingUTF8); /* we only allow UTF8 or ASCII */
        char *buffer = (char *)PORT_ArenaZNewArray(poolp, uint8_t, buffer_size);
        bool utf8 = false;
        if (!CFStringGetCString(challenge, buffer, buffer_size, kCFStringEncodingASCII)) {
            if (!CFStringGetCString(challenge, buffer, buffer_size, kCFStringEncodingUTF8))
                return NULL;
            utf8 = true;
        } else
            if (!printable_string(challenge))
                utf8 = true;

        SecAsn1Item *challenge_password_value = PORT_ArenaZNewArray(poolp, SecAsn1Item, 1);        
        SecAsn1Item challenge_password_raw = { strlen(buffer), (uint8_t*)buffer };
        SEC_ASN1EncodeItem(poolp, challenge_password_value, &challenge_password_raw, 
            utf8 ? kSecAsn1UTF8StringTemplate : kSecAsn1PrintableStringTemplate);
        SecAsn1Item **challenge_password_values = PORT_ArenaZNewArray(poolp, SecAsn1Item *, 2);        
        challenge_password_values[0] = challenge_password_value;
        challenge_password_attr.attrType.Length = sizeof(pkcs9ChallengePassword);
        challenge_password_attr.attrType.Data = (uint8_t*)&pkcs9ChallengePassword;
        challenge_password_attr.attrValue = challenge_password_values;
        num_attrs++;
    }

    NSS_CertExtension **extensions = extensions_from_parameters(poolp, parameters);
    NSS_Attribute extensions_requested_attr = {};
    if (extensions) {
        SecAsn1Item *extensions_requested_value = PORT_ArenaZNewArray(poolp, SecAsn1Item, 1);
        SEC_ASN1EncodeItem(poolp, extensions_requested_value, &extensions, kSecAsn1SequenceOfCertExtensionTemplate);
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
#if 0
out:
    return NULL;
#endif
}

static const uint8_t encoded_null[2] = { SEC_ASN1_NULL, 0 };
static const SecAsn1Item asn1_null = { sizeof(encoded_null), (uint8_t*)encoded_null };

CFDataRef SecGenerateCertificateRequestWithParameters(SecRDN *subject, 
    CFDictionaryRef parameters, SecKeyRef publicKey, SecKeyRef privateKey)
{
    CFDataRef csr = NULL;
    PRArenaPool *poolp = PORT_NewArena(1024);
    CFDictionaryRef pubkey_attrs = NULL;
    
    if (!poolp)
        return NULL;

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
        for (one_atv = *one_rdn; one_atv->oid; one_atv++)
            atv_num++;
        atv_num++; /* one more */
        num++;
    }
    const unsigned min1atv_num = atv_num > 0 ? atv_num : 1;
    const unsigned min1num = num > 0 ? num : 1;
    NSS_ATV atvs[min1atv_num];
    NSS_ATV *atvps[min1atv_num];
    NSS_RDN rdns[min1num];
    NSS_RDN *rdnps[num+1];
    atv_num = 0;
    unsigned rdn_num = 0;
    for (one_rdn = subject; *one_rdn; one_rdn++) {
        rdns[rdn_num].atvs = &atvps[atv_num];
        rdnps[rdn_num] = &rdns[rdn_num];
        rdn_num++;
        for (one_atv = *one_rdn; one_atv->oid; one_atv++) {
            if (!make_nss_atv(poolp, one_atv->oid, one_atv->value, 
                    one_atv->type, &atvs[atv_num]))
                return NULL;
            atvps[atv_num] = &atvs[atv_num];
            atv_num++;
        }
        atvps[atv_num++] = NULL;
    }
    rdnps[rdn_num] = NULL;
    certReq.reqInfo.subject.rdns = rdnps;
    
    /* public key info */
    certReq.reqInfo.subjectPublicKeyInfo.algorithm.algorithm.Length = oidRsa.length;
    certReq.reqInfo.subjectPublicKeyInfo.algorithm.algorithm.Data = oidRsa.data;
    certReq.reqInfo.subjectPublicKeyInfo.algorithm.parameters = asn1_null;
    
    pubkey_attrs = SecKeyCopyAttributeDictionary(publicKey);
    CFDataRef pkcs1_pubkey = (CFDataRef)CFDictionaryGetValue(pubkey_attrs, kSecValueData);
    uint8_t signature[8 * CFDataGetLength(pkcs1_pubkey)];
    size_t signature_length = sizeof(signature);

    certReq.reqInfo.subjectPublicKeyInfo.subjectPublicKey.Length = 8 * CFDataGetLength(pkcs1_pubkey);
    certReq.reqInfo.subjectPublicKeyInfo.subjectPublicKey.Data = (uint8_t*)CFDataGetBytePtr(pkcs1_pubkey);
    
    certReq.reqInfo.attributes = nss_attributes_from_parameters_dict(poolp, parameters);
    SecCmsArraySortByDER((void **)certReq.reqInfo.attributes, kSecAsn1AttributeTemplate, NULL);

    /* encode request info by itself to calculate signature */
    SecAsn1Item reqinfo = {};
    SEC_ASN1EncodeItem(poolp, &reqinfo, &certReq.reqInfo, kSecAsn1CertRequestInfoTemplate);

    /* calculate signature */
    uint8_t reqinfo_hash[CC_SHA1_DIGEST_LENGTH];
    CCDigest(kCCDigestSHA1, reqinfo.Data, (CC_LONG)reqinfo.Length, reqinfo_hash);
    require_noerr_quiet(SecKeyRawSign(privateKey, kSecPaddingPKCS1SHA1, 
        reqinfo_hash, sizeof(reqinfo_hash), signature, &signature_length), out);
    
    /* signature and info */
    certReq.signatureAlgorithm.algorithm.Length = oidSha1Rsa.length;
    certReq.signatureAlgorithm.algorithm.Data = oidSha1Rsa.data;
    certReq.signatureAlgorithm.parameters = asn1_null;
    certReq.signature.Data = signature;
    certReq.signature.Length = signature_length * 8;
    
    /* encode csr */
    SecAsn1Item cert_request = {};
    require_quiet(SEC_ASN1EncodeItem(poolp, &cert_request, &certReq, 
        kSecAsn1CertRequestTemplate), out);
    csr = CFDataCreate(kCFAllocatorDefault, cert_request.Data, cert_request.Length);
    
out:
    if (poolp)
        PORT_FreeArena(poolp, PR_TRUE);
    CFReleaseSafe(pubkey_attrs);
    return csr;
}

CFDataRef SecGenerateCertificateRequest(CFArrayRef subject, 
    CFDictionaryRef parameters, SecKeyRef publicKey, SecKeyRef privateKey)
{
    CFDataRef csr = NULL;
    PRArenaPool *poolp = PORT_NewArena(1024);
    CFDictionaryRef pubkey_attrs = NULL;
    
    if (!poolp)
        return NULL;

	NSSCertRequest certReq;
	memset(&certReq, 0, sizeof(certReq));

    /* version */
    unsigned char version = 0;
    certReq.reqInfo.version.Length = sizeof(version);
    certReq.reqInfo.version.Data = &version;

    /* subject */
    certReq.reqInfo.subject.rdns = make_subject(poolp, (CFArrayRef)subject);
    
    /* public key info */
    certReq.reqInfo.subjectPublicKeyInfo.algorithm.algorithm.Length = oidRsa.length;
    certReq.reqInfo.subjectPublicKeyInfo.algorithm.algorithm.Data = oidRsa.data;
    certReq.reqInfo.subjectPublicKeyInfo.algorithm.parameters = asn1_null;
    
    pubkey_attrs = SecKeyCopyAttributeDictionary(publicKey);
    CFDataRef pkcs1_pubkey = (CFDataRef)CFDictionaryGetValue(pubkey_attrs, kSecValueData);
    uint8_t signature[8 * CFDataGetLength(pkcs1_pubkey)];
    size_t signature_length = sizeof(signature);

    certReq.reqInfo.subjectPublicKeyInfo.subjectPublicKey.Length = 8 * CFDataGetLength(pkcs1_pubkey);
    certReq.reqInfo.subjectPublicKeyInfo.subjectPublicKey.Data = (uint8_t*)CFDataGetBytePtr(pkcs1_pubkey);
    
    certReq.reqInfo.attributes = nss_attributes_from_parameters_dict(poolp, parameters);
    SecCmsArraySortByDER((void **)certReq.reqInfo.attributes, kSecAsn1AttributeTemplate, NULL);

    /* encode request info by itself to calculate signature */
    SecAsn1Item reqinfo = {};
    SEC_ASN1EncodeItem(poolp, &reqinfo, &certReq.reqInfo, kSecAsn1CertRequestInfoTemplate);

    /* calculate signature */
    uint8_t reqinfo_hash[CC_SHA1_DIGEST_LENGTH];
    CCDigest(kCCDigestSHA1, reqinfo.Data, reqinfo.Length, reqinfo_hash);
    require_noerr_quiet(SecKeyRawSign(privateKey, kSecPaddingPKCS1SHA1, 
        reqinfo_hash, sizeof(reqinfo_hash), signature, &signature_length), out);
    
    /* signature and info */
    certReq.signatureAlgorithm.algorithm.Length = oidSha1Rsa.length;
    certReq.signatureAlgorithm.algorithm.Data = oidSha1Rsa.data;
    certReq.signatureAlgorithm.parameters = asn1_null;
    certReq.signature.Data = signature;
    certReq.signature.Length = signature_length * 8;
    
    /* encode csr */
    SecAsn1Item cert_request = {};
    require_quiet(SEC_ASN1EncodeItem(poolp, &cert_request, &certReq, 
        kSecAsn1CertRequestTemplate), out);
    csr = CFDataCreate(kCFAllocatorDefault, cert_request.Data, cert_request.Length);
    
out:
    if (poolp)
        PORT_FreeArena(poolp, PR_TRUE);
    CFReleaseSafe(pubkey_attrs);
    return csr;
}

bool SecVerifyCertificateRequest(CFDataRef csr, SecKeyRef *publicKey,
    CFStringRef *challenge, CFDataRef *subject, CFDataRef *extensions)
{
    PRArenaPool *poolp = PORT_NewArena(1024);
    SecKeyRef candidatePublicKey = NULL;
    bool valid = false;
	NSSCertRequest certReq;
	memset(&certReq, 0, sizeof(certReq));
    SecAsn1Item csr_item = { CFDataGetLength(csr), (uint8_t*)CFDataGetBytePtr(csr) };
    require_noerr_quiet(SEC_ASN1DecodeItem(poolp, &certReq, kSecAsn1CertRequestTemplate, 
        &csr_item), out);

    /* signature and info */
    require(certReq.signatureAlgorithm.algorithm.Length == oidSha1Rsa.length, out);
    require_noerr(memcmp(oidSha1Rsa.data, certReq.signatureAlgorithm.algorithm.Data,
        oidSha1Rsa.length), out);
    require(certReq.signatureAlgorithm.parameters.Length == asn1_null.Length, out);
    require_noerr(memcmp(asn1_null.Data, certReq.signatureAlgorithm.parameters.Data, 
        asn1_null.Length), out);

    /* encode request info by itself to calculate signature */
    SecAsn1Item reqinfo = {};
    SEC_ASN1EncodeItem(poolp, &reqinfo, &certReq.reqInfo, kSecAsn1CertRequestInfoTemplate);

    /* calculate signature */
    uint8_t reqinfo_hash[CC_SHA1_DIGEST_LENGTH];
    require(reqinfo.Length<=UINT32_MAX, out);
    CCDigest(kCCDigestSHA1, reqinfo.Data, (CC_LONG)reqinfo.Length, reqinfo_hash);

    /* @@@ check for version 0 */

    require(candidatePublicKey = SecKeyCreateRSAPublicKey(kCFAllocatorDefault, 
        certReq.reqInfo.subjectPublicKeyInfo.subjectPublicKey.Data,
        certReq.reqInfo.subjectPublicKeyInfo.subjectPublicKey.Length / 8, 
        kSecKeyEncodingPkcs1), out);

    require_noerr_quiet(SecKeyRawVerify(candidatePublicKey, kSecPaddingPKCS1SHA1, 
        reqinfo_hash, sizeof(reqinfo_hash), 
        certReq.signature.Data, certReq.signature.Length / 8), out);
        
    SecAsn1Item subject_item = { 0 }, extensions_item = { 0 }, challenge_item = { 0 };
    require_quiet(SEC_ASN1EncodeItem(poolp, &subject_item, 
        &certReq.reqInfo.subject, kSecAsn1NameTemplate), out);

    if (*certReq.reqInfo.attributes) {
        uint32_t ix;
        for (ix = 0; certReq.reqInfo.attributes[ix]; ix++) {
            NSS_Attribute *attr = certReq.reqInfo.attributes[ix];
            if ( (sizeof(pkcs9ChallengePassword) == attr->attrType.Length) &&
                !memcmp(pkcs9ChallengePassword, attr->attrType.Data, sizeof(pkcs9ChallengePassword)))
                    challenge_item = *attr->attrValue[0];
            else if ( (sizeof(pkcs9ExtensionsRequested) == attr->attrType.Length) &&
                !memcmp(pkcs9ExtensionsRequested, attr->attrType.Data, sizeof(pkcs9ExtensionsRequested)))
                    extensions_item = *attr->attrValue[0];
        }
    }
    
    if (subject && subject_item.Length)
        *subject = CFDataCreate(kCFAllocatorDefault, subject_item.Data, subject_item.Length);
    if (extensions && extensions_item.Length)
        *extensions = CFDataCreate(kCFAllocatorDefault, extensions_item.Data, extensions_item.Length);
    if (challenge && challenge_item.Length) {
        SecAsn1Item string = { 0 };
        SECStatus rv = SEC_ASN1DecodeItem(poolp, &string, kSecAsn1UTF8StringTemplate, &challenge_item);
        if (rv)
            rv = SEC_ASN1DecodeItem(poolp, &string, kSecAsn1PrintableStringTemplate, &challenge_item);
        if (!rv)
            *challenge = CFStringCreateWithBytes(kCFAllocatorDefault, string.Data, string.Length, kCFStringEncodingUTF8, false);
        else
            *challenge = NULL;
    }
    if (publicKey) {
        *publicKey = candidatePublicKey;
        candidatePublicKey = NULL;
    }
    valid = true;
out:
    CFReleaseSafe(candidatePublicKey);
    if (poolp)
        PORT_FreeArena(poolp, PR_TRUE);
    return valid;
}

#define HIDIGIT(v) (((v) / 10) + '0')    
#define LODIGIT(v) (((v) % 10) + '0')     

static OSStatus
DER_CFDateToUTCTime(PRArenaPool *poolp, CFAbsoluteTime date, SecAsn1Item * utcTime)
{
    unsigned char *d;
    
    utcTime->Length = 13;
    utcTime->Data = d = PORT_ArenaAlloc(poolp, 13);
    if (!utcTime->Data)
        return SECFailure;
    
    __block int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    __block bool result;
    SecCFCalendarDoWithZuluCalendar(^(CFCalendarRef zuluCalendar) {
        result = CFCalendarDecomposeAbsoluteTime(zuluCalendar, date, "yMdHms", &year, &month, &day, &hour, &minute, &second);
    });
    if (!result)
        return SECFailure;
    
    /* UTC time does not handle the years before 1950 */
    if (year < 1950)
        return SECFailure;
    
    /* remove the century since it's added to the year by the
     CFAbsoluteTimeGetGregorianDate routine, but is not needed for UTC time */
    year %= 100;
    
    d[0] = HIDIGIT(year);
    d[1] = LODIGIT(year);
    d[2] = HIDIGIT(month);
    d[3] = LODIGIT(month);
    d[4] = HIDIGIT(day);
    d[5] = LODIGIT(day);
    d[6] = HIDIGIT(hour);
    d[7] = LODIGIT(hour);
    d[8] = HIDIGIT(minute);
    d[9] = LODIGIT(minute);
    d[10] = HIDIGIT(second);
    d[11] = LODIGIT(second);
    d[12] = 'Z';
    return SECSuccess;
}

SecCertificateRef
SecGenerateSelfSignedCertificate(CFArrayRef subject, CFDictionaryRef parameters, 
    SecKeyRef publicKey, SecKeyRef privateKey)
{
    SecCertificateRef cert = NULL;
    PRArenaPool *poolp = PORT_NewArena(1024);
    CFDictionaryRef pubkey_attrs = NULL;
    if (!poolp)
        return NULL;

    NSS_Certificate cert_tmpl;
    memset(&cert_tmpl, 0, sizeof(cert_tmpl));

    /* version */
    unsigned char version = 2;
    cert_tmpl.tbs.version.Length = sizeof(version);
    cert_tmpl.tbs.version.Data = &version;

    /* serialno */
    unsigned char serialNumber = 1;
    cert_tmpl.tbs.serialNumber.Length = sizeof(serialNumber);
    cert_tmpl.tbs.serialNumber.Data = &serialNumber;

    /* subject/issuer */
    cert_tmpl.tbs.issuer.rdns = make_subject(poolp, (CFArrayRef)subject);
    cert_tmpl.tbs.subject.rdns = cert_tmpl.tbs.issuer.rdns;

    DER_CFDateToUTCTime(poolp, CFAbsoluteTimeGetCurrent(), &cert_tmpl.tbs.validity.notBefore.item);
    cert_tmpl.tbs.validity.notBefore.tag = SEC_ASN1_UTC_TIME;
    DER_CFDateToUTCTime(poolp, CFAbsoluteTimeGetCurrent() + 3600*24*365, &cert_tmpl.tbs.validity.notAfter.item);
    cert_tmpl.tbs.validity.notAfter.tag = SEC_ASN1_UTC_TIME;

    /* extensions */
    cert_tmpl.tbs.extensions = extensions_from_parameters(poolp, parameters);

    /* @@@ we only handle rsa keys */
    pubkey_attrs = SecKeyCopyAttributeDictionary(publicKey);
    CFTypeRef key_type = CFDictionaryGetValue(pubkey_attrs, kSecAttrKeyType);
    if (key_type && CFEqual(key_type, kSecAttrKeyTypeRSA)) {
        /* public key data and algorithm */
        cert_tmpl.tbs.subjectPublicKeyInfo.algorithm.algorithm = CSSMOID_RSA;
        cert_tmpl.tbs.subjectPublicKeyInfo.algorithm.parameters = asn1_null;

        CFDataRef pkcs1_pubkey = (CFDataRef)CFDictionaryGetValue(pubkey_attrs, kSecValueData);
        cert_tmpl.tbs.subjectPublicKeyInfo.subjectPublicKey.Length = 8 * CFDataGetLength(pkcs1_pubkey);
        cert_tmpl.tbs.subjectPublicKeyInfo.subjectPublicKey.Data = (uint8_t*)CFDataGetBytePtr(pkcs1_pubkey);

        /* signature algorithm */
        cert_tmpl.tbs.signature.algorithm = CSSMOID_SHA1WithRSA;
        cert_tmpl.tbs.signature.parameters = asn1_null;
        cert_tmpl.signatureAlgorithm.algorithm = CSSMOID_SHA1WithRSA;
        cert_tmpl.signatureAlgorithm.parameters = asn1_null;

        /* encode request info by itself to calculate signature */
        SecAsn1Item tbscert = {};
        SEC_ASN1EncodeItem(poolp, &tbscert, &cert_tmpl.tbs, kSecAsn1TBSCertificateTemplate);

        /* calculate signature */
        uint8_t tbscert_hash[CC_SHA1_DIGEST_LENGTH];
        CCDigest(kCCDigestSHA1, tbscert.Data, tbscert.Length, tbscert_hash);
        uint8_t signature[8 * CFDataGetLength(pkcs1_pubkey)];
        size_t signature_length = sizeof(signature);
        require_noerr_quiet(SecKeyRawSign(privateKey, kSecPaddingPKCS1SHA1, 
                    tbscert_hash, sizeof(tbscert_hash), signature, &signature_length), out);

        /* signature */
        cert_tmpl.signature.Data = signature;
        cert_tmpl.signature.Length = signature_length * 8;

        /* encode cert */
        SecAsn1Item signed_cert = {};
        require_quiet(SEC_ASN1EncodeItem(poolp, &signed_cert, &cert_tmpl, 
                    kSecAsn1SignedCertTemplate), out);
        cert = SecCertificateCreateWithBytes(kCFAllocatorDefault, 
                signed_cert.Data, signed_cert.Length);
    }
out:
    if (poolp)
        PORT_FreeArena(poolp, PR_TRUE);
    CFReleaseSafe(pubkey_attrs);
    return cert;
}


SecCertificateRef
SecIdentitySignCertificate(SecIdentityRef issuer, CFDataRef serialno,
    SecKeyRef publicKey, CFTypeRef subject, CFTypeRef extensions)
{
    SecCertificateRef cert = NULL;
    SecKeyRef privateKey = NULL;

    PRArenaPool *poolp = PORT_NewArena(1024);
    CFDictionaryRef pubkey_attrs = NULL;
    if (!poolp)
        return NULL;

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
    if (CFArrayGetTypeID() == CFGetTypeID(subject))
        cert_tmpl.tbs.subject.rdns = make_subject(poolp, (CFArrayRef)subject);
    else if (CFDataGetTypeID() == CFGetTypeID(subject)) {
        SecAsn1Item subject_item = { CFDataGetLength(subject), (uint8_t*)CFDataGetBytePtr(subject) };
        require_noerr_quiet(SEC_ASN1DecodeItem(poolp, &cert_tmpl.tbs.subject.rdns, kSecAsn1NameTemplate, &subject_item), out);
    } else
        goto out;

    SecCertificateRef issuer_cert = NULL;
    require_noerr(SecIdentityCopyCertificate(issuer, &issuer_cert), out);
    CFDataRef issuer_name = SecCertificateCopySubjectSequence(issuer_cert);
    SecAsn1Item issuer_item = { CFDataGetLength(issuer_name), (uint8_t*)CFDataGetBytePtr(issuer_name) };
    require_noerr_action_quiet(SEC_ASN1DecodeItem(poolp, &cert_tmpl.tbs.issuer.rdns, 
        kSecAsn1NameTemplate, &issuer_item), out, CFReleaseNull(issuer_name));
    CFReleaseNull(issuer_name);

    DER_CFDateToUTCTime(poolp, CFAbsoluteTimeGetCurrent(), &cert_tmpl.tbs.validity.notBefore.item);
    cert_tmpl.tbs.validity.notBefore.tag = SEC_ASN1_UTC_TIME;
    DER_CFDateToUTCTime(poolp, CFAbsoluteTimeGetCurrent() + 3600*24*365, &cert_tmpl.tbs.validity.notAfter.item);
    cert_tmpl.tbs.validity.notAfter.tag = SEC_ASN1_UTC_TIME;

    /* extensions */
	if (extensions) {
        if (CFDataGetTypeID() == CFGetTypeID(extensions)) {
            SecAsn1Item requested_extensions = { CFDataGetLength(extensions), (uint8_t*)CFDataGetBytePtr(extensions) };
            //NSS_CertExtension **requested_extensions_decoded;
            require_noerr_quiet(SEC_ASN1DecodeItem(poolp, &cert_tmpl.tbs.extensions,
                        kSecAsn1SequenceOfCertExtensionTemplate, &requested_extensions), out);
        } else if (CFDictionaryGetTypeID() == CFGetTypeID(extensions)) {
            cert_tmpl.tbs.extensions = extensions_from_parameters(poolp, extensions);
        }
    }

    /* @@@ we only handle rsa keys */
    pubkey_attrs = SecKeyCopyAttributeDictionary(publicKey);
    CFTypeRef key_type = CFDictionaryGetValue(pubkey_attrs, kSecAttrKeyType);
    if (key_type && CFEqual(key_type, kSecAttrKeyTypeRSA)) {
        /* public key data and algorithm */
        cert_tmpl.tbs.subjectPublicKeyInfo.algorithm.algorithm = CSSMOID_RSA;
        cert_tmpl.tbs.subjectPublicKeyInfo.algorithm.parameters = asn1_null;

        CFDataRef pkcs1_pubkey = (CFDataRef)CFDictionaryGetValue(pubkey_attrs, kSecValueData);
        cert_tmpl.tbs.subjectPublicKeyInfo.subjectPublicKey.Length = 8 * CFDataGetLength(pkcs1_pubkey);
        cert_tmpl.tbs.subjectPublicKeyInfo.subjectPublicKey.Data = (uint8_t*)CFDataGetBytePtr(pkcs1_pubkey);

        /* signature algorithm */
        cert_tmpl.tbs.signature.algorithm = CSSMOID_SHA1WithRSA;
        cert_tmpl.tbs.signature.parameters = asn1_null;
        cert_tmpl.signatureAlgorithm.algorithm = CSSMOID_SHA1WithRSA;
        cert_tmpl.signatureAlgorithm.parameters = asn1_null;

        /* encode request info by itself to calculate signature */
        SecAsn1Item tbscert = {};
        SEC_ASN1EncodeItem(poolp, &tbscert, &cert_tmpl.tbs, kSecAsn1TBSCertificateTemplate);

        /* calculate signature */
        uint8_t tbscert_hash[CC_SHA1_DIGEST_LENGTH];
        CCDigest(kCCDigestSHA1, tbscert.Data, tbscert.Length, tbscert_hash);
        uint8_t signature[8 * CFDataGetLength(pkcs1_pubkey)];
        size_t signature_length = sizeof(signature);
        
        require_noerr_quiet(SecIdentityCopyPrivateKey(issuer, &privateKey), out);
        require_noerr_quiet(SecKeyRawSign(privateKey, kSecPaddingPKCS1SHA1, 
                    tbscert_hash, sizeof(tbscert_hash), signature, &signature_length), out);

        /* signature */
        cert_tmpl.signature.Data = signature;
        cert_tmpl.signature.Length = signature_length * 8;

        /* encode cert */
        SecAsn1Item signed_cert = {};
        require_quiet(SEC_ASN1EncodeItem(poolp, &signed_cert, &cert_tmpl, 
                    kSecAsn1SignedCertTemplate), out);
        cert = SecCertificateCreateWithBytes(kCFAllocatorDefault, 
                signed_cert.Data, signed_cert.Length);
    }
out:
        CFReleaseSafe(privateKey);
    if (poolp)
        PORT_FreeArena(poolp, PR_TRUE);
    CFReleaseSafe(pubkey_attrs);
    return cert;
}

CF_RETURNS_RETAINED
CFDataRef
SecGenerateCertificateRequestSubject(SecCertificateRef ca_certificate, CFArrayRef subject)
{
    CFMutableDataRef sequence = NULL;
    PRArenaPool *poolp = PORT_NewArena(1024);
    if (!poolp)
        return NULL;

    /*
        Going agains the spec here:

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
    NSS_Name nss_subject = { make_subject(poolp, subject) };
    require_quiet(SEC_ASN1EncodeItem(poolp, &subject_item, &nss_subject, kSecAsn1NameTemplate), out);

    DERSize sequence_length = DERLengthOfLength(subject_item.Length + issuer_item.Length);
    DERSize seq_len_length = subject_item.Length + issuer_item.Length + 1 /* SEQUENCE */ +
        sequence_length;
    sequence = CFDataCreateMutable(kCFAllocatorDefault, 0);
    CFDataSetLength(sequence, seq_len_length);
    uint8_t *sequence_ptr = CFDataGetMutableBytePtr(sequence);
    *sequence_ptr++ = 0x30; //ASN1_CONSTR_SEQUENCE;
    require_noerr_quiet(DEREncodeLength(subject_item.Length + issuer_item.Length, sequence_ptr, &sequence_length), out);
    sequence_ptr += sequence_length;
    memcpy(sequence_ptr, issuer_item.Data, issuer_item.Length);
    memcpy(sequence_ptr + issuer_item.Length, subject_item.Data, subject_item.Length);

out:
    CFReleaseSafe(issuer_sequence);
    if (poolp)
        PORT_FreeArena(poolp, PR_TRUE);
    return sequence;
}
