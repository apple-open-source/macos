/*
 * Copyright (c) 2003-2004,2006-2007,2009-2010 Apple Inc. All Rights Reserved.
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
 * scep.c
 */

#include <TargetConditionals.h>
#if TARGET_OS_EMBEDDED

#include "SecurityCommands.h"

#include <unistd.h>
#include <uuid/uuid.h>
#include <AssertMacros.h>

#include <Security/SecItem.h>
#include <Security/SecCertificateRequest.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecSCEP.h>
#include <Security/SecCMS.h>

#include <utilities/array_size.h>
#include <utilities/SecIOFormat.h>

#include <CommonCrypto/CommonDigest.h>

#include <CFNetwork/CFNetwork.h>
#include "SecBase64.h"


#include <fcntl.h>
static inline void write_data(const char * path, CFDataRef data)
{
    int data_file = open(path, O_CREAT|O_WRONLY|O_TRUNC, 0644);
    write(data_file, CFDataGetBytePtr(data), CFDataGetLength(data));
    close(data_file);
}

#define BUFSIZE 1024

static CFHTTPMessageRef load_request(CFHTTPMessageRef request, CFMutableDataRef data, int retry, bool validate_cert)
{
    CFHTTPMessageRef result = NULL;
    
    if (retry < 0)
        return result;

    CFReadStreamRef rs;
    rs = CFReadStreamCreateForHTTPRequest(NULL, request);

	if (!validate_cert) {
		const void *keys[] = {
			kCFStreamSSLValidatesCertificateChain,
		};
		const void *values[] = {
			kCFBooleanFalse,
		};
		CFDictionaryRef dict = CFDictionaryCreate(NULL, keys, values,
			array_size(keys),
			&kCFTypeDictionaryKeyCallBacks,
			&kCFTypeDictionaryValueCallBacks);
		CFReadStreamSetProperty(rs, kCFStreamPropertySSLSettings, dict);
		CFRelease(dict);
		CFReadStreamSetProperty(rs, kCFStreamPropertyHTTPAttemptPersistentConnection, kCFBooleanTrue);
	}
    
    if (CFReadStreamOpen(rs)) {
        do {
            UInt8 buf[BUFSIZE];
            CFIndex bytesRead = CFReadStreamRead(rs, buf, BUFSIZE);
            if (bytesRead > 0) {
                CFDataAppendBytes(data, buf, bytesRead);
            } else if (bytesRead == 0) {
                result = (CFHTTPMessageRef)CFReadStreamCopyProperty(rs, kCFStreamPropertyHTTPResponseHeader);
                break;
            } else {
                CFStreamStatus status = CFReadStreamGetStatus(rs);
                CFStreamError error = CFReadStreamGetError(rs);
                fprintf(stderr, "CFReadStreamRead status=%ld error(domain=%" PRIdCFIndex " error=%ld)\n",
                    status, error.domain, (long) error.error);
                break;
            }
        } while (true);
    } else {
        CFStreamStatus status = CFReadStreamGetStatus(rs);
        CFStreamError error = CFReadStreamGetError(rs);
        fprintf(stderr, "CFReadStreamRead status=%ld error(domain=%" PRIdCFIndex " error=%ld)\n",
            status, error.domain, (long) error.error);
    }

    CFReadStreamClose(rs);
    CFRelease(rs);
    return result;
}

static CFDataRef MCNetworkLoadRequest(CFURLRef url, CFDataRef content, CFStringRef type,
    CFStringRef *contentType, bool validate_cert)
{
    CFMutableDataRef out_data = CFDataCreateMutable(kCFAllocatorDefault, 0);
    CFHTTPMessageRef response = NULL;
    CFStringRef request_type = content ? CFSTR("POST") : CFSTR("GET");
    CFHTTPMessageRef request = CFHTTPMessageCreateRequest(kCFAllocatorDefault, 
        request_type, url, kCFHTTPVersion1_0);
    if (content)
        CFHTTPMessageSetBody(request, content);
    CFHTTPMessageSetHeaderFieldValue(request, CFSTR("Content-Type"), type);

    int retries = 1;
    do {
        response = load_request(request, out_data, 1, validate_cert);
        if (!response && retries) {
            sleep(5);
            CFDataSetLength(out_data, 0);
        }
    } while (!response && retries--);
    
    CFRelease(request);
    
    CFIndex status_code = response ? CFHTTPMessageGetResponseStatusCode(response) : 0;
    if (!response || (200 != status_code)) {
        CFStringRef url_string = CFURLGetString(url);
        if (url_string && request_type && out_data)
            fprintf(stderr, "MCNetworkLoadRequest failed to load\n");
        return NULL;
    }

    if (contentType)
        *contentType = CFHTTPMessageCopyHeaderFieldValue(response, CFSTR("Content-Type"));
        
    CFRelease(response);
    return out_data;
}

static void _query_string_apply(CFMutableStringRef query_string, const void *key, const void *value)
{
    CFStringRef escaped_key = 
        CFURLCreateStringByAddingPercentEscapes(kCFAllocatorDefault, 
            (CFStringRef)key, NULL, CFSTR("+/="), kCFStringEncodingUTF8);
    CFStringRef escaped_value = 
        CFURLCreateStringByAddingPercentEscapes(kCFAllocatorDefault, 
            (CFStringRef)value, NULL, CFSTR("+/="), kCFStringEncodingUTF8);
    
    CFStringRef format;
    if (CFStringGetLength(query_string) > 1)
        format = CFSTR("&%@=%@");
    else
        format = CFSTR("%@=%@");

    CFStringAppendFormat(query_string, NULL, format, escaped_key, escaped_value);
    CFRelease(escaped_key);
    CFRelease(escaped_value);
}

static CFURLRef scep_url_operation(CFStringRef base, CFStringRef operation, CFStringRef message)
{
    CFURLRef url = NULL, base_url = NULL;
    CFMutableStringRef query_string = 
        CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("?"));
    require(query_string, out);
    require(operation, out);
    _query_string_apply(query_string, CFSTR("operation"), operation);
    if (message)
        _query_string_apply(query_string, CFSTR("message"), message);
    base_url = CFURLCreateWithString(kCFAllocatorDefault, base, NULL);
    url = CFURLCreateWithString(kCFAllocatorDefault, query_string, base_url);
out:
    if (query_string)
        CFRelease(query_string);
    if (base_url)
        CFRelease(base_url);
    return url;
}

static CFDataRef perform_pki_op(CFStringRef scep_base_url, CFDataRef scep_request, bool scep_can_use_post, bool validate_cert)
{
	CFDataRef scep_reply = NULL;
	CFURLRef pki_op = NULL;
    if (scep_can_use_post) {
        pki_op = scep_url_operation(scep_base_url, CFSTR("PKIOperation"), NULL);
        scep_reply = MCNetworkLoadRequest(pki_op, scep_request, CFSTR("application/x-pki-message"), NULL, validate_cert);
    } else {
        SecBase64Result base64_result;
        size_t buffer_length = CFDataGetLength(scep_request)*2+1;
        char *buffer = malloc(buffer_length);
        require(buffer, out);
        size_t output_size = SecBase64Encode2(CFDataGetBytePtr(scep_request), CFDataGetLength(scep_request), buffer, buffer_length,
            kSecB64_F_LINE_LEN_INFINITE, -1, &base64_result);
        *(buffer + output_size) = '\0';
        require(!base64_result, out);
        CFStringRef message = CFStringCreateWithCString(kCFAllocatorDefault, buffer, kCFStringEncodingASCII);
        require(message, out);
		pki_op = scep_url_operation(scep_base_url, CFSTR("PKIOperation"), message);
        CFRelease(message);
        fprintf(stderr, "Performing PKIOperation using GET\n");
        scep_reply = MCNetworkLoadRequest(pki_op, NULL, CFSTR("application/x-pki-message"), NULL, validate_cert);
    }
out:
	if (pki_op) CFRelease(pki_op);
	return scep_reply;
}


static inline CFStringRef uuid_cfstring()
{
    char uuid_string[40] = "CN=";
    uuid_t uuid;
    uuid_generate_random(uuid);
    uuid_unparse(uuid, uuid_string+3);
    return CFStringCreateWithCString(kCFAllocatorDefault, uuid_string, kCFStringEncodingASCII);
}

/* /O=foo/CN=blah => [ [ [ O, foo ] ], [ [ CN, blah ] ] ] */
static void make_subject_pairs(const void *value, void *context)
{
    CFArrayRef entries = NULL, array = NULL;
    if (!CFStringGetLength(value))
        return; /* skip '/'s that aren't separating key/vals */
    entries = CFStringCreateArrayBySeparatingStrings(kCFAllocatorDefault, value, CFSTR("="));
    require(entries, out);
    if (CFArrayGetCount(entries)) {
        array = CFArrayCreate(kCFAllocatorDefault, (const void **)&entries, 1, &kCFTypeArrayCallBacks);
        require(array, out);
        CFArrayAppendValue((CFMutableArrayRef)context, array);
    }
out:
    if (entries) CFRelease(entries);
    if (array) CFRelease(array);
}

static CFArrayRef make_scep_subject(CFStringRef scep_subject_name)
{
    CFMutableArrayRef subject = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    require(subject, out);
    CFArrayRef entries = NULL;
    entries = CFStringCreateArrayBySeparatingStrings(kCFAllocatorDefault, scep_subject_name, CFSTR("/"));
    require(entries, out);
    CFArrayApplyFunction(entries, CFRangeMake(0, CFArrayGetCount(entries)), make_subject_pairs, subject);
    CFRelease(entries);
    if (CFArrayGetCount(subject))
        return subject;
out:
    if (subject) CFRelease(subject);
    return NULL;
}


extern int command_scep(int argc, char * const *argv)
{
    int             result = 1, verbose = 0;
    char            ch;
    int key_usage = 1, key_bitsize = 1024;
    bool validate_cert = true;
    CFStringRef scep_challenge = NULL, scep_instance_name = NULL,
        scep_subject_name = uuid_cfstring(), scep_subject_alt_name = NULL,
        scep_capabilities = NULL;

    while ((ch = getopt(argc, argv, "vu:b:c:n:s:h:xo:")) != -1)
    {
        switch (ch)
        {
        case 'v':
            verbose++;
            break;
        case 'u':
            key_usage = atoi(optarg);
            break;
        case 'b':
            key_bitsize = atoi(optarg);
            break;
        case 'c':
            scep_challenge = CFStringCreateWithCString(kCFAllocatorDefault, optarg, kCFStringEncodingUTF8);
            break;
        case 'n':
            scep_instance_name = CFStringCreateWithCString(kCFAllocatorDefault, optarg, kCFStringEncodingUTF8);
            break;
        case 's':
            scep_subject_name = CFStringCreateWithCString(kCFAllocatorDefault, optarg, kCFStringEncodingUTF8);
            break;
        case 'h':
            scep_subject_alt_name = CFStringCreateWithCString(kCFAllocatorDefault, optarg, kCFStringEncodingUTF8);
            break;
	case 'x':
	    validate_cert = false;
	    break;
        case 'o':
            scep_capabilities = CFStringCreateWithCString(kCFAllocatorDefault, optarg, kCFStringEncodingUTF8);
            break;
        default:
            return 2; /* Trigger usage message. */
        }
    }

    argc -= optind;
    argv += optind;

    if (argc != 1)
        return 2; /* Trigger usage message. */

    CFDataRef scep_request = NULL;
    CFArrayRef issued_certs = NULL;
    SecCertificateRef leaf = NULL;
    SecIdentityRef candidate_identity = NULL;
    CFMutableDictionaryRef csr_parameters = NULL;
    CFDataRef scep_reply = NULL;
    SecKeyRef phone_publicKey = NULL, phone_privateKey = NULL;
    CFStringRef scep_base_url = NULL;
    CFDictionaryRef identity_add = NULL;

    scep_base_url = CFStringCreateWithCString(kCFAllocatorDefault, argv[0], kCFStringEncodingASCII);

#if 0
    CFStringRef uuid_cfstr = uuid_cfstring();
    require(uuid_cfstr, out);
    const void * ca_cn[] = { kSecOidCommonName, uuid_cfstr };
    CFArrayRef ca_cn_dn = CFArrayCreate(kCFAllocatorDefault, ca_cn, 2, NULL);
    const void *ca_dn_array[1];
    ca_dn_array[0] = CFArrayCreate(kCFAllocatorDefault, (const void **)&ca_cn_dn, 1, NULL);
    CFArrayRef scep_subject = CFArrayCreate(kCFAllocatorDefault, ca_dn_array, array_size(ca_dn_array), NULL);
#else
    CFArrayRef scep_subject = make_scep_subject(scep_subject_name);
    require(scep_subject, out);
    CFShow(scep_subject);
#endif

    CFNumberRef scep_key_usage = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &key_usage);
    CFNumberRef scep_key_bitsize = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &key_bitsize);

    const void *keygen_keys[] = { kSecAttrKeyType, kSecAttrKeySizeInBits };
    const void *keygen_vals[] = { kSecAttrKeyTypeRSA, scep_key_bitsize };
    CFDictionaryRef keygen_parameters = CFDictionaryCreate(kCFAllocatorDefault, 
        keygen_keys, keygen_vals, array_size(keygen_vals),
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
    CFRelease(scep_key_bitsize);
    require_noerr(SecKeyGeneratePair(keygen_parameters, &phone_publicKey, &phone_privateKey), out);
    CFRelease(keygen_parameters);

    /* GetCACert
    
        A binary X.509 CA certificate is sent back as a MIME object with a
        Content-Type of application/x-x509-ca-cert.
        
        When an RA exists, both CA and RA certificates must be sent back in
        the response to the GetCACert request.  The RA certificate(s) must be
        signed by the CA.  A certificates-only PKCS#7 [RFC2315] SignedData is
        used to carry the certificates to the requester, with a Content-Type
        of application/x-x509-ca-ra-cert.
    */
    CFDataRef data = NULL;
    CFStringRef ctype = NULL;
    SecCertificateRef ca_certificate = NULL, ra_certificate = NULL;
    SecCertificateRef ra_encryption_certificate = NULL;
    CFArrayRef ra_certificates = NULL;
    CFTypeRef scep_certificates = NULL;
    SecCertificateRef scep_signing_certificate = NULL;

    CFURLRef url = scep_url_operation(scep_base_url, CFSTR("GetCACert"), scep_instance_name);
    data = MCNetworkLoadRequest(url, NULL, NULL, &ctype, validate_cert);

    if (data && ctype) {
        if (CFEqual(CFSTR("application/x-x509-ca-cert"), ctype)) {
            ca_certificate = SecCertificateCreateWithData(kCFAllocatorDefault, (CFDataRef)data);
            fprintf(stderr, "GetCACert returned a single CA certificate.\n");
        } else if (CFEqual(ctype, CFSTR("application/x-x509-ca-ra-cert"))) {
            CFArrayRef cert_array = SecCMSCertificatesOnlyMessageCopyCertificates(data);
            
            require_noerr(SecSCEPValidateCACertMessage(cert_array,
                NULL,
                &ca_certificate,
                &ra_certificate,
                &ra_encryption_certificate), out);

            if (ra_certificate && ra_encryption_certificate) {
                const void *ra_certs[] = { ra_encryption_certificate, ra_certificate };
                ra_certificates = CFArrayCreate(kCFAllocatorDefault, ra_certs, array_size(ra_certs), &kCFTypeArrayCallBacks);
                fprintf(stderr, "GetCACert returned a separate signing and encryption certificates for RA.\n");
            }
            CFRelease(cert_array);
        }
    }

    fprintf(stderr, "CA certificate to issue cert:\n");
    CFShow(ca_certificate);
    
    if (ra_certificates) {
        scep_certificates = ra_certificates;
        scep_signing_certificate = ra_certificate;
    } else if (ra_certificate) {
        scep_certificates = ra_certificate;
        scep_signing_certificate = ra_certificate;
    } else if (ca_certificate) { 
        scep_certificates = ca_certificate;
        scep_signing_certificate = ca_certificate;
    } else {
        fprintf(stderr, "Unsupported GetCACert configuration: please file a bug.\n");
        goto out;
    }

#if 0
        GetCACaps capabilities advertised by SCEP server:

   +--------------------+----------------------------------------------+
   | Keyword            | Description                                  |
   +--------------------+----------------------------------------------+
   | "GetNextCACert"    | CA Supports the GetNextCACert message.       |
   | "POSTPKIOperation" | PKIOPeration messages may be sent via HTTP   |
   |                    | POST.                                        |
   | "Renewal"          | Clients may use current certificate and key  |
   |                    | to authenticate an enrollment request for a  |
   |                    | new certificate.                             |
   | "SHA-512"          | CA Supports the SHA-512 hashing algorithm in |
   |                    | signatures and fingerprints.                 |
   | "SHA-256"          | CA Supports the SHA-256 hashing algorithm in |
   |                    | signatures and fingerprints.                 |
   | "SHA-1"            | CA Supports the SHA-1 hashing algorithm in   |
   |                    | signatures and fingerprints.                 |
   | "DES3"             | CA Supports triple-DES for encryption.       |
   +--------------------+----------------------------------------------+
#endif

    bool scep_can_use_post = false;
    bool scep_use_3des = false;
    bool scep_can_use_sha1 = false;

    CFArrayRef caps = NULL;
    if (!scep_capabilities) {
        CFURLRef ca_caps_url = scep_url_operation(scep_base_url, CFSTR("GetCACaps"), scep_instance_name);
        require(ca_caps_url, out);
        CFDataRef caps_data = MCNetworkLoadRequest(ca_caps_url, NULL, NULL, NULL, validate_cert);
        CFRelease(ca_caps_url);
        if (caps_data) {
            CFStringRef caps_data_string = CFStringCreateFromExternalRepresentation(kCFAllocatorDefault, caps_data, kCFStringEncodingASCII);
            require(caps_data_string, out);
            caps = CFStringCreateArrayBySeparatingStrings(kCFAllocatorDefault, caps_data_string, CFSTR("\n"));
            if (!caps) {
                fprintf(stderr, "GetCACaps couldn't be parsed:\n");
                CFShow(caps_data);
            }
            CFRelease(caps_data);
        }
    } else {
        caps = CFStringCreateArrayBySeparatingStrings(kCFAllocatorDefault, scep_capabilities, CFSTR(","));
    }

    if (caps) {
        fprintf(stderr, "GetCACaps advertised following capabilities:\n");
        CFShow(caps);

        CFRange caps_length = CFRangeMake(0, CFArrayGetCount(caps));
        scep_can_use_post = CFArrayContainsValue(caps, caps_length, CFSTR("POSTPKIOperation"));
        scep_use_3des = CFArrayContainsValue(caps, caps_length, CFSTR("DES3"));
        scep_can_use_sha1 = CFArrayContainsValue(caps, caps_length, CFSTR("SHA-1"));
    }

    scep_use_3des = true;
    scep_can_use_sha1 = true;

    csr_parameters = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, 
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (scep_key_usage)
        CFDictionarySetValue(csr_parameters, kSecCertificateKeyUsage, scep_key_usage);
    if (scep_challenge)
        CFDictionarySetValue(csr_parameters, kSecCSRChallengePassword, scep_challenge);
    else
        fprintf(stderr, "No SCEP challenge provided, hope that's ok.\n");

    if (!scep_use_3des) {
        CFDictionarySetValue(csr_parameters, kSecCMSBulkEncryptionAlgorithm, kSecCMSEncryptionAlgorithmDESCBC);
        fprintf(stderr, "SCEP server does not support 3DES, falling back to DES.  You should reconfigure your server.\n");
    }
    if (!scep_can_use_sha1) {
        CFDictionarySetValue(csr_parameters, kSecCMSSignHashAlgorithm, kSecCMSHashingAlgorithmMD5);
        fprintf(stderr, "SCEP server does not support SHA-1, falling back to MD5.  You should reconfigure your server.\n");    
    }

    if (scep_subject_alt_name) {
        fprintf(stderr, "Adding subjectAltName to request\n");
        CFStringRef name = CFSTR("dnsName");
        CFDictionaryRef subject_alt_name = CFDictionaryCreate(kCFAllocatorDefault,
            (const void **)&name, (const void **)&scep_subject_alt_name,
            1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(csr_parameters, kSecSubjectAltName, subject_alt_name);
    }

    SecIdentityRef self_signed_identity = SecSCEPCreateTemporaryIdentity(phone_publicKey, phone_privateKey);

    // store encryption identity in the keychain because the decrypt function looks in there only
    identity_add = CFDictionaryCreate(NULL,
        &kSecValueRef, (const void **)&self_signed_identity, 1, NULL, NULL);
	require_noerr(SecItemAdd(identity_add, NULL), out);

    require(scep_request = SecSCEPGenerateCertificateRequest((CFArrayRef)scep_subject, 
        csr_parameters, phone_publicKey, phone_privateKey, self_signed_identity,
        scep_certificates), out);

    fprintf(stderr, "storing scep_request.der\n");
    write_data("scep_request.der", scep_request);

	scep_reply = perform_pki_op(scep_base_url, scep_request, scep_can_use_post, validate_cert);
    require(scep_reply, out);

    require_action(CFDataGetLength(scep_reply), out, fprintf(stderr, "Empty scep_reply, exiting.\n"));
    fprintf(stderr, "Storing scep_reply.der\n");
    write_data("scep_reply.der", scep_reply);

	CFErrorRef server_error = NULL;
	int retry_count = 3;
    while ( !(issued_certs = SecSCEPVerifyReply(scep_request, scep_reply, scep_certificates, &server_error)) &&
			server_error &&
			retry_count--)
	{
		CFDataRef retry_get_cert_initial = NULL;
		CFDictionaryRef error_dict = CFErrorCopyUserInfo(server_error);
		retry_get_cert_initial = SecSCEPGetCertInitial(ra_certificate ? ra_certificate : ca_certificate, scep_subject, NULL, error_dict, self_signed_identity, scep_certificates);
		if (scep_reply) CFRelease(scep_reply);
		fprintf(stderr, "Waiting 10 seconds before trying a GetCertInitial\n");
		sleep(10);
		scep_reply = perform_pki_op(scep_base_url, retry_get_cert_initial, scep_can_use_post, validate_cert);
	}

    require(issued_certs, out);
	require_string(CFArrayGetCount(issued_certs) > 0, out, "No certificates issued.");
    
    leaf = (SecCertificateRef)CFArrayGetValueAtIndex(issued_certs, 0);
    require(leaf, out);
    CFDataRef leaf_data = SecCertificateCopyData(leaf);
    if (leaf_data) {
        fprintf(stderr, "Storing issued_cert.der\n");
        write_data("issued_cert.der", leaf_data);
        CFRelease(leaf_data);
    }
    CFShow(leaf);

    candidate_identity = SecIdentityCreate(kCFAllocatorDefault, leaf, phone_privateKey);

    const void *keys_ref_to_persist[] = { 
        /*kSecReturnPersistentRef, */kSecValueRef, kSecAttrLabel };
    const void *values_ref_to_persist[] = { 
        /*kCFBooleanTrue, */candidate_identity, scep_subject_name };
    CFDictionaryRef dict = CFDictionaryCreate(NULL, 
            (const void **)keys_ref_to_persist, 
            (const void **)values_ref_to_persist, 
            array_size(keys_ref_to_persist),
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    OSStatus status = SecItemAdd(dict, NULL);
    require_noerr_action(status, out, fprintf(stderr, "failed to store new identity, SecItemAdd: %" PRIdOSStatus, status));
    result = 0;
    
out:
    SecItemDelete(identity_add);
    if (identity_add) CFRelease(identity_add);
    //if (uuid_cfstr) CFRelease(uuid_cfstr);
    if (candidate_identity) CFRelease(candidate_identity);
    if (scep_request) CFRelease(scep_request);
    if (scep_reply) CFRelease(scep_reply);
    if (csr_parameters) CFRelease(csr_parameters);
    
    return result;
}


#endif // TARGET_OS_MAC
