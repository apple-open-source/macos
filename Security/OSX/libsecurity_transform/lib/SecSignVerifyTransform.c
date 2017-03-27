/*
 * Copyright (c) 2010-2014 Apple Inc. All Rights Reserved.
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


#include "SecSignVerifyTransform.h"
#include "SecCustomTransform.h"
#include "Utilities.h"
#include <Security/Security.h>
#include "misc.h"


const static CFStringRef SignName = CFSTR("com.apple.security.Sign"), VerifyName = CFSTR("com.apple.security.Verify");
const CFStringRef __nonnull kSecKeyAttributeName = CFSTR("KEY"), kSecSignatureAttributeName = CFSTR("Signature"), kSecInputIsAttributeName = CFSTR("InputIs");
// Internally we force kSecInputIsAttributeName to one of these 3 things, you can use == rather then CFStringCompare once that happens
const CFStringRef __nonnull kSecInputIsPlainText = CFSTR("PlainText"), kSecInputIsDigest = CFSTR("Digest"), kSecInputIsRaw = CFSTR("Raw");

static
CFErrorRef do_sec_fail(OSStatus code, const char *func, const char *file, int line) {
	CFStringRef msg = CFStringCreateWithFormat(NULL, NULL, CFSTR("Internal error #%x at %s %s:%d"), (unsigned)code, func, file, line);
	CFErrorRef err = fancy_error(CFSTR("Internal CSSM error"), code, msg);
	CFRelease(msg);
	
	return err;
}
#define SEC_FAIL(err) if (err) { \
    SecTransformCustomSetAttribute(ref, kSecTransformAbortAttributeName, kSecTransformMetaAttributeValue, do_sec_fail(err, __func__, __FILE__, __LINE__)); \
    return (CFTypeRef)NULL; \
}
#define GET_SEC_FAIL(err) do_sec_fail(err, __func__, __FILE__, __LINE__)

static
CFErrorRef accumulate_data(CFMutableArrayRef *a, CFDataRef d) {
	if (!*a) {
		*a = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		if (!*a) {
			return GetNoMemoryError();
		}
	}
	CFDataRef dc = CFDataCreateCopy(NULL, d);
	if (!dc) {
		return GetNoMemoryError();
	}
	CFIndex c = CFArrayGetCount(*a);
	CFArrayAppendValue(*a, dc);
	CFRelease(dc);
	if (CFArrayGetCount(*a) != c+1) {
		return GetNoMemoryError();
	}
	
	return NULL;
}

static
CFErrorRef fetch_and_clear_accumulated_data(CFMutableArrayRef *a, CFDataRef *data_out) {
	if (!*a) {
		*data_out = CFDataCreate(NULL, NULL, 0);
		return (*data_out) ? NULL : GetNoMemoryError();
	}

	CFIndex i, c = CFArrayGetCount(*a);
	CFIndex total = 0, prev_total = 0;
	
	for(i = 0; i < c; i++) {
		total += CFDataGetLength((CFDataRef)CFArrayGetValueAtIndex(*a, i));
		if (total < prev_total) {
			return GetNoMemoryError();
		}
		prev_total = total;
	}
	
	CFMutableDataRef out = CFDataCreateMutable(NULL, total);
	if (!out) {
		return GetNoMemoryError();
	}
	
	for(i = 0; i < c; i++) {
		CFDataRef d = (CFDataRef)CFArrayGetValueAtIndex(*a, i);
		CFDataAppendBytes(out, CFDataGetBytePtr(d), CFDataGetLength(d));
	}
	
	if (CFDataGetLength(out) != total) {
		CFRelease(out);
		return GetNoMemoryError();
	}
	
	CFArrayRef accumulator = *a;
	CFRelease(accumulator);
	*a = NULL;
	
	// This might be nice:
	//   *data_out = CFDataCreateCopy(NULL, out);
	//   CFRelease(out);
	// but that is slow (for large values) AND isn't really all that important anyway
	
	*data_out = out;
	
	return NULL;
}

struct digest_mapping {
	// These 3 values are "search values"
	CSSM_ALGORITHMS kclass;
	CFStringRef digest_name;
	int digest_length;
	
	// "data values"
	CSSM_ALGORITHMS plain_text_algo, digest_algo;
};

static
Boolean digest_mapping_equal(struct digest_mapping *a, struct digest_mapping *b) {
	if (a == b) {
		return TRUE;
	}
	
	if (a->kclass == b->kclass && a->digest_length == b->digest_length && !CFStringCompare(a->digest_name, b->digest_name, 0)) {
		return TRUE;
	}
	
	return FALSE;
}

static
CFHashCode digest_mapping_hash(struct digest_mapping *dm) {
	return CFHash(dm->digest_name) + dm->kclass + dm->digest_length;
}

static
CSSM_ALGORITHMS alg_for_signature_context(CFStringRef input_is, const struct digest_mapping *dm) {
	if (!CFStringCompare(kSecInputIsPlainText, input_is, 0)) {
		return dm->plain_text_algo;
	} else if (!CFStringCompare(kSecInputIsDigest, input_is, 0) || !CFStringCompare(kSecInputIsRaw, input_is, 0)) {
		return dm->kclass;
	} else {
		return CSSM_ALGID_NONE;
	}
}

static
CFErrorRef pick_sign_alg(CFStringRef digest, int digest_length, const CSSM_KEY *ckey, struct digest_mapping **picked) {
	static dispatch_once_t once = 0;
	static CFMutableSetRef algos = NULL;

	dispatch_once(&once, ^{
		struct digest_mapping digest_mappings_stack[] = {
			{CSSM_ALGID_RSA, kSecDigestSHA1, 0,   CSSM_ALGID_SHA1WithRSA, CSSM_ALGID_SHA1},
			{CSSM_ALGID_RSA, kSecDigestSHA1, 160,   CSSM_ALGID_SHA1WithRSA, CSSM_ALGID_SHA1},
			
			{CSSM_ALGID_RSA, kSecDigestMD2, 0,   CSSM_ALGID_MD2WithRSA, CSSM_ALGID_MD2},
			{CSSM_ALGID_RSA, kSecDigestMD2, 128,   CSSM_ALGID_MD2WithRSA, CSSM_ALGID_MD2},
			
			{CSSM_ALGID_RSA, kSecDigestMD5, 0,   CSSM_ALGID_MD5WithRSA, CSSM_ALGID_MD5},
			{CSSM_ALGID_RSA, kSecDigestMD5, 128,   CSSM_ALGID_MD5WithRSA, CSSM_ALGID_MD5},
			
			{CSSM_ALGID_RSA, kSecDigestSHA2, 0,   CSSM_ALGID_SHA512WithRSA, CSSM_ALGID_SHA512},
			{CSSM_ALGID_RSA, kSecDigestSHA2, 512,   CSSM_ALGID_SHA512WithRSA, CSSM_ALGID_SHA512},
			{CSSM_ALGID_RSA, kSecDigestSHA2, 384,   CSSM_ALGID_SHA384WithRSA, CSSM_ALGID_SHA384},
			{CSSM_ALGID_RSA, kSecDigestSHA2, 256,   CSSM_ALGID_SHA256WithRSA, CSSM_ALGID_SHA256},
			{CSSM_ALGID_RSA, kSecDigestSHA2, 224,   CSSM_ALGID_SHA224WithRSA, CSSM_ALGID_SHA224},
			
			
			{CSSM_ALGID_ECDSA, kSecDigestSHA1, 0,   CSSM_ALGID_SHA1WithECDSA, CSSM_ALGID_SHA1},
			{CSSM_ALGID_ECDSA, kSecDigestSHA1, 160,   CSSM_ALGID_SHA1WithECDSA, CSSM_ALGID_SHA1},
			
			{CSSM_ALGID_ECDSA, kSecDigestSHA2, 0,   CSSM_ALGID_SHA512WithECDSA, CSSM_ALGID_SHA512},
			{CSSM_ALGID_ECDSA, kSecDigestSHA2, 512,   CSSM_ALGID_SHA512WithECDSA, CSSM_ALGID_SHA512},
			{CSSM_ALGID_ECDSA, kSecDigestSHA2, 384,   CSSM_ALGID_SHA384WithECDSA, CSSM_ALGID_SHA384},
			{CSSM_ALGID_ECDSA, kSecDigestSHA2, 256,   CSSM_ALGID_SHA256WithECDSA, CSSM_ALGID_SHA256},
			{CSSM_ALGID_ECDSA, kSecDigestSHA2, 224,   CSSM_ALGID_SHA224WithECDSA, CSSM_ALGID_SHA224},
			
			{CSSM_ALGID_DSA, kSecDigestSHA1, 0,   CSSM_ALGID_SHA1WithDSA, CSSM_ALGID_SHA1},
			{CSSM_ALGID_DSA, kSecDigestSHA1, 160,   CSSM_ALGID_SHA1WithDSA, CSSM_ALGID_SHA1},
		};
		
		CFIndex mapping_count = sizeof(digest_mappings_stack)/sizeof(digest_mappings_stack[0]);
		void *digest_mappings = malloc(sizeof(digest_mappings_stack));
		memcpy(digest_mappings, digest_mappings_stack, sizeof(digest_mappings_stack));

		CFSetCallBacks dmcb = { .version = 0, .retain = NULL, .release = NULL, .copyDescription = NULL, .equal = (CFSetEqualCallBack)digest_mapping_equal, .hash = (CFSetHashCallBack)digest_mapping_hash };
		
		algos = CFSetCreateMutable(NULL, mapping_count, &dmcb);
		int i;
		for(i = 0; i < mapping_count; i++) {
			CFSetAddValue(algos, i + (struct digest_mapping *)digest_mappings);
		}
	});
	
	struct digest_mapping search;
	search.kclass = ckey->KeyHeader.AlgorithmId;
	search.digest_name = digest;
	search.digest_length = digest_length;
	
	struct digest_mapping *dmapping = (void*)CFSetGetValue(algos, &search);
	
	if (dmapping) {
		*picked = dmapping;
		return NULL;
	}
	
	// It is argueable better to gennerate these messages by looking at digest_mappings, but with only 3 keytypes and 4 digests (only one of which has signifigant length variations) a case statment is likely the best way.
	switch (ckey->KeyHeader.AlgorithmId) {
		case CSSM_ALGID_RSA:
			return fancy_error(kSecTransformErrorDomain, kSecTransformErrorInvalidAlgorithm, CFSTR("Invalid digest algorithm for RSA signature, choose one of: SHA1, SHA2 (512bits, 348bits, 256bits, or 224 bits), MD2, or MD5"));
			
		case CSSM_ALGID_ECDSA:
			return fancy_error(kSecTransformErrorDomain, kSecTransformErrorInvalidAlgorithm, CFSTR("Invalid digest algorithm for ECDSA signature, choose one of: SHA1, or SHA2 (512bits, 348bits, 256bits, or 224 bits)"));

		case CSSM_ALGID_DSA:
			return fancy_error(kSecTransformErrorDomain, kSecTransformErrorInvalidAlgorithm, CFSTR("Invalid digest algorithm for DSA signature, only SHA1 is supported"));

		default:
			return fancy_error(kSecTransformErrorDomain, kSecTransformErrorInvalidAlgorithm, CFSTR("Expected key to be RSA, DSA or ECDSA key"));
	}
}

static SecTransformInstanceBlock SignTransform(CFStringRef name, 
							SecTransformRef newTransform, 
							SecTransformImplementationRef ref)
{
	SecTransformInstanceBlock instanceBlock = ^
	{
		CFErrorRef result = NULL;
		SecTransformCustomSetAttribute(ref, kSecKeyAttributeName, kSecTransformMetaAttributeRequired, kCFBooleanTrue);
		SecTransformCustomSetAttribute(ref, kSecInputIsAttributeName, kSecTransformMetaAttributeRequired, kCFBooleanTrue);

		__block CSSM_CC_HANDLE cch;
		__block SecKeyRef key = NULL;
		__block SecTransformDataBlock first_process_data = NULL;
		__block CFStringRef digest = NULL;
		__block int digest_length = 0;
		__block CFStringRef input_is = NULL;
		__block CFMutableArrayRef data_accumulator = NULL;
		__block struct digest_mapping *sign_alg;
		
		SecTransformDataBlock plain_text_process_data = 
		^(CFTypeRef value) 
		{
			CFDataRef d = value;
			OSStatus rc;
			
			if (d) {
				CSSM_DATA c_d;
				c_d.Data = (void*)CFDataGetBytePtr(d);
				c_d.Length = CFDataGetLength(d);
				
				rc = CSSM_SignDataUpdate(cch, &c_d, 1);
				SEC_FAIL(rc);
			} else {
				CSSM_DATA sig;
				const int max_sig_size = 32*1024;
				unsigned char *sig_data = malloc(max_sig_size);	
				sig.Data = sig_data;
				sig.Length = max_sig_size;
				
				rc = CSSM_SignDataFinal(cch, &sig);
				SEC_FAIL(rc);
				assert(sig.Length <= 32*1024);
				CSSM_DeleteContext(cch);
				// Could use NoCopy and hold onto the allocation, and that will be a good idea when we can have it not so oversized
				CFDataRef result = CFDataCreate(NULL, sig.Data, sig.Length);
				SecTransformCustomSetAttribute(ref, kSecTransformOutputAttributeName, kSecTransformMetaAttributeValue, result);
				CFRelease(result);
				free(sig_data);
				
				key = NULL;
				
				CFRelease(digest);
				digest = NULL;
				
				digest_length = 0;
				
				SecTransformSetDataAction(ref, kSecTransformActionProcessData,  first_process_data);
				
				return (CFTypeRef)NULL;
			}
			
			return SecTransformNoData();
		};
		
		SecTransformDataBlock cooked_process_data = 
		^(CFTypeRef value) 
		{
			CFDataRef d = value;
			if (d) {
				accumulate_data(&data_accumulator, d);
			} else {
				CSSM_DATA sig;
				const int max_sig_size = 32*1024;
				unsigned char *sig_data = malloc(max_sig_size);	
				sig.Data = sig_data;
				sig.Length = max_sig_size;
				
				CFDataRef alldata;
				CFErrorRef err = fetch_and_clear_accumulated_data(&data_accumulator, &alldata);
				if (err) {
					return (CFTypeRef)err;
				}
				CSSM_DATA c_d;
				c_d.Data = (void*)CFDataGetBytePtr(alldata);
				c_d.Length = CFDataGetLength(alldata);
				
				OSStatus rc = CSSM_SignData(cch, &c_d, 1, (input_is == kSecInputIsDigest) ? sign_alg->digest_algo : CSSM_ALGID_NONE, &sig);
				SEC_FAIL(rc);
				CFRelease(alldata);
				
				assert(sig.Length <= 32*1024);
				CSSM_DeleteContext(cch);
				// Could use NoCopy and hold onto the allocation, and that will be a good idea when we can have it not so oversized
				CFDataRef result = CFDataCreate(NULL, sig.Data, sig.Length);
				SecTransformCustomSetAttribute(ref, kSecTransformOutputAttributeName, kSecTransformMetaAttributeValue, result);
				CFRelease(result);
				free(sig_data);
				
				key = NULL;
				
				CFRelease(digest);
				digest = NULL;
				
				digest_length = 0;
				
				SecTransformSetDataAction(ref, kSecTransformActionProcessData, first_process_data);
				
				return (CFTypeRef)NULL;
			}
			
			return SecTransformNoData();
		};
				
		first_process_data = Block_copy(^(CFTypeRef value) 
		{
			OSStatus rc;
			if (key && digest && input_is) 
			{
				const CSSM_KEY *cssm_key;
				rc = SecKeyGetCSSMKey(key, &cssm_key);
				SEC_FAIL(rc);
				
				CFErrorRef bad_alg = pick_sign_alg(digest, digest_length, cssm_key, &sign_alg);
				if (bad_alg) 
				{
					return (CFTypeRef)bad_alg;
				}

				CSSM_CSP_HANDLE csp;
				rc = SecKeyGetCSPHandle(key, &csp);
				SEC_FAIL(rc);
				
				const CSSM_ACCESS_CREDENTIALS *access_cred;
				rc = SecKeyGetCredentials(key, CSSM_ACL_AUTHORIZATION_SIGN, kSecCredentialTypeDefault, &access_cred);
				SEC_FAIL(rc);
				
				CSSM_CSP_CreateSignatureContext(csp, alg_for_signature_context(input_is, sign_alg), access_cred, cssm_key, &cch);
				SEC_FAIL(rc);
				
				rc = CSSM_SignDataInit(cch);
				SEC_FAIL(rc);
								
				SecTransformDataBlock pd = (input_is == kSecInputIsPlainText) ? plain_text_process_data : cooked_process_data;

				SecTransformSetDataAction(ref, kSecTransformActionProcessData, pd);
				return pd(value);
			} 
			else 
			{
				SecTransformPushbackAttribute(ref, kSecTransformInputAttributeName, value);
				return SecTransformNoData();
			}
		});
		
		SecTransformSetDataAction(ref, kSecTransformActionProcessData, first_process_data);
		
		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, kSecDigestTypeAttribute, 
			^(SecTransformAttributeRef ah, CFTypeRef value) 
			{
				digest = CFRetain(value);
				return value;
			});

		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, kSecKeyAttributeName, 
			^(SecTransformAttributeRef ah, CFTypeRef value) 
			{
				if (value == NULL) {
                    return value;
                }
                
                const CSSM_KEY *cssm_key;
				key = (SecKeyRef)value;
			
				OSStatus rc = SecKeyGetCSSMKey(key, &cssm_key);
				SEC_FAIL(rc);
			
				if (!cssm_key->KeyHeader.KeyUsage & CSSM_KEYUSE_SIGN)
				{
					key = NULL;
                    
                    CFTypeRef error = CreateSecTransformErrorRef(kSecTransformErrorInvalidInput, "Key %@ can not be used to sign", key);
					SecTransformCustomSetAttribute(ref, kSecTransformAbortAttributeName, kSecTransformMetaAttributeValue, error);
                    return (CFTypeRef)NULL;
				}
				return value;
			});
		
		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, kSecDigestLengthAttribute, 
			^(SecTransformAttributeRef ah, CFTypeRef value) 
			{
				CFNumberGetValue(value, kCFNumberIntType, &digest_length);
				return value;
			});
		
		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, kSecInputIsAttributeName,
			^(SecTransformAttributeRef ah, CFTypeRef value) 
			{
				if (!CFStringCompare(value, kSecInputIsPlainText, 0)) {
					input_is = kSecInputIsPlainText;
				} else if (!CFStringCompare(value, kSecInputIsDigest, 0)) {
					input_is = kSecInputIsDigest;
				} else if (!CFStringCompare(value, kSecInputIsRaw, 0)) {
					input_is = kSecInputIsRaw;
				} else {
					input_is = NULL;
					return (CFTypeRef)fancy_error(kSecTransformErrorDomain, kSecTransformErrorInvalidType, CFSTR("InputIs should be one of: PlainText, Digest, or Raw"));
				}
				return (CFTypeRef)input_is;
			});			
		
		SecTransformSetTransformAction(ref, kSecTransformActionFinalize, 
			^{
				Block_release(first_process_data);
				return (CFTypeRef)NULL;
			});
		
		return result;
	};
		
	return Block_copy(instanceBlock);	
}
							
SecTransformRef SecSignTransformCreate(SecKeyRef key, CFErrorRef* error)
{	
	static dispatch_once_t once;
	__block Boolean ok = TRUE;
			
	dispatch_block_t aBlock = ^
	{
		ok = SecTransformRegister(SignName, &SignTransform, error);
	};
	
	dispatch_once(&once, aBlock);

	if (!ok) 
	{
		return NULL;
	}	
	
	SecTransformRef tr = SecTransformCreate(SignName, error);
	if (!tr) {
		return tr;
	}
	SecTransformSetAttribute(tr, kSecKeyAttributeName, key, error);
	SecTransformSetAttribute(tr, kSecDigestTypeAttribute, kSecDigestSHA1, NULL);
	SecTransformSetAttribute(tr, kSecInputIsAttributeName, kSecInputIsPlainText, NULL);
	
	return tr;
}

static SecTransformInstanceBlock VerifyTransform(CFStringRef name, 
							SecTransformRef newTransform, 
							SecTransformImplementationRef ref)
{
	SecTransformInstanceBlock instanceBlock = ^
	{
		CFErrorRef result = NULL;
		SecTransformCustomSetAttribute(ref, kSecKeyAttributeName, kSecTransformMetaAttributeRequired, kCFBooleanTrue);
		SecTransformCustomSetAttribute(ref, kSecSignatureAttributeName, kSecTransformMetaAttributeRequired, kCFBooleanTrue);
		SecTransformCustomSetAttribute(ref, kSecInputIsAttributeName, kSecTransformMetaAttributeRequired, kCFBooleanTrue);
		
		__block CSSM_CC_HANDLE cch;
		__block const CSSM_KEY *cssm_key;
		__block CSSM_CSP_HANDLE csp;
		__block const CSSM_ACCESS_CREDENTIALS *access_cred;
		__block CFDataRef signature = NULL;
		__block unsigned char had_last_input = 0;
		__block CFStringRef digest = NULL;
		__block int digest_length = 0;
		__block SecTransformDataBlock first_process_data;
		__block SecKeyRef key = NULL;
		__block CFStringRef input_is = NULL;
		__block CFMutableArrayRef data_accumulator = NULL;
		__block struct digest_mapping *verify_alg = NULL;

		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, kSecInputIsAttributeName, 
			^(SecTransformAttributeRef ah, CFTypeRef value) 
			{
				if (!CFStringCompare(value, kSecInputIsPlainText, 0)) {
					input_is = kSecInputIsPlainText;
				} else if (!CFStringCompare(value, kSecInputIsDigest, 0)) {
					input_is = kSecInputIsDigest;
				} else if (!CFStringCompare(value, kSecInputIsRaw, 0)) {
					input_is = kSecInputIsRaw;
				} else {
					input_is = NULL;
					return (CFTypeRef)fancy_error(kSecTransformErrorDomain, kSecTransformErrorInvalidType, CFSTR("InputIs should be one of: PlainText, Digest, or Raw"));
				}
				return (CFTypeRef)input_is;
			});			
		
		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, kSecKeyAttributeName, 
			^(SecTransformAttributeRef ah, CFTypeRef value) 
			{
				OSStatus rc;
                
                if (value == NULL) {
                    return value;
                }
			
				rc = SecKeyGetCSSMKey((SecKeyRef)value, &cssm_key);
				SEC_FAIL(rc);
			
				if (!cssm_key->KeyHeader.KeyUsage & CSSM_KEYUSE_VERIFY)
				{
					// This key cannot verify!
					return (CFTypeRef)CreateSecTransformErrorRef(kSecTransformErrorInvalidInput, "Key %@ can not be used to verify", key);
				}
		
				// we don't need to retain this because the owning transform is doing that for us
				key = (SecKeyRef) value;
				return value;
			});
		
		// We call this when we get the last input and when we get the signature.   If both are true when it is 
		// called we are really done, and it gennerates the output
		void (^done)(void) = 
		^{
			if (signature && had_last_input) 
			{
				CSSM_DATA sig;
				OSStatus rc;
				sig.Data = (void*)CFDataGetBytePtr(signature);
				sig.Length = CFDataGetLength(signature);
				CFRelease(signature);
				signature = NULL;
				
				if (input_is == kSecInputIsPlainText) {
					rc = CSSM_VerifyDataFinal(cch, &sig);
				} else {
					CFDataRef alldata;
					CFErrorRef err = fetch_and_clear_accumulated_data(&data_accumulator, &alldata);
					if (err) {
						SecTransformCustomSetAttribute(ref, kSecTransformOutputAttributeName, kSecTransformMetaAttributeValue, (CFTypeRef)err);
						return;
					}
					
					CSSM_DATA c_d;
					c_d.Data = (void*)CFDataGetBytePtr(alldata);
					c_d.Length = CFDataGetLength(alldata);
					rc = CSSM_VerifyData(cch, &c_d, 1, (input_is == kSecInputIsDigest) ? verify_alg->digest_algo : CSSM_ALGID_NONE, &sig);
                    CFRelease(alldata);

				}
				CSSM_DeleteContext(cch);
				if (rc == 0 || rc == CSSMERR_CSP_VERIFY_FAILED) {
					SecTransformCustomSetAttribute(ref, kSecTransformOutputAttributeName, kSecTransformMetaAttributeValue, rc ? kCFBooleanFalse : kCFBooleanTrue);
					SecTransformCustomSetAttribute(ref, kSecTransformOutputAttributeName, kSecTransformMetaAttributeValue, NULL);
				} else {
					SecTransformCustomSetAttribute(ref, kSecTransformOutputAttributeName, kSecTransformMetaAttributeValue, GET_SEC_FAIL(rc));
				}
				had_last_input = FALSE;
				SecTransformSetDataAction(ref, kSecTransformActionProcessData, first_process_data);	
			}
		};
		
		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, kSecSignatureAttributeName, 
			^(SecTransformAttributeRef ah, CFTypeRef value) 
			{
				if (value) {
					signature = CFRetain(value);
				}
			
				done();
			
				return (CFTypeRef)value;
			});
		
		SecTransformDataBlock process_data = 
			^(CFTypeRef value) 
			{
				OSStatus rc;
				CFDataRef d = value;
			
				if (d) {
					if (input_is == kSecInputIsPlainText) {
						CSSM_DATA c_d;
						c_d.Data = (void*)CFDataGetBytePtr(d);
						c_d.Length = CFDataGetLength(d);
					
						rc = CSSM_VerifyDataUpdate(cch, &c_d, 1);
						SEC_FAIL(rc);
					} else {
						accumulate_data(&data_accumulator, d);
					}
				} else {
					had_last_input = 1;
					done();
				}
			
				return SecTransformNoData();
			};
		
		first_process_data = 
			^(CFTypeRef value) 
			{
				if (key && digest && input_is) {
					// XXX: For RSA keys, signal an error if the digest size>keysize
				
					OSStatus rc = SecKeyGetCSPHandle(key, &csp);
					SEC_FAIL(rc);
				
					rc = SecKeyGetCredentials(key, CSSM_ACL_AUTHORIZATION_ANY, kSecCredentialTypeDefault, &access_cred);
					SEC_FAIL(rc);
				
					CFErrorRef bad_alg = pick_sign_alg(digest, digest_length, cssm_key, &verify_alg);
					if (bad_alg) {
						return (CFTypeRef)bad_alg;
					}
				
					CSSM_CSP_CreateSignatureContext(csp, alg_for_signature_context(input_is, verify_alg), NULL, cssm_key, &cch);
					SEC_FAIL(rc);
				
					rc = CSSM_VerifyDataInit(cch);
					SEC_FAIL(rc);

					SecTransformSetDataAction(ref, kSecTransformActionProcessData, process_data);
					return process_data(value);
				} else {
					SecTransformPushbackAttribute(ref, kSecTransformInputAttributeName, value);
					return SecTransformNoData();
				}
			};
		first_process_data = Block_copy(first_process_data);
		
		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, kSecDigestTypeAttribute, 
			^(SecTransformAttributeRef ah, CFTypeRef value) 
			{
				digest = CFRetain(value);
				return value;
			});
		
		SecTransformSetTransformAction(ref, kSecTransformActionFinalize, 
			^{
				Block_release(first_process_data);
				return (CFTypeRef)NULL;
			});
		
		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, kSecDigestLengthAttribute, 
			^(SecTransformAttributeRef ah, CFTypeRef value) 
			{
				CFNumberGetValue(value, kCFNumberIntType, &digest_length);
				return value;
			});
		
		SecTransformSetDataAction(ref, kSecTransformActionProcessData, first_process_data);	
		
		return result;
	};
	
	return Block_copy(instanceBlock);
}

SecTransformRef SecVerifyTransformCreate(SecKeyRef key, CFDataRef signature, CFErrorRef* error)
{
	static dispatch_once_t once;
	__block Boolean ok = TRUE;
			
	dispatch_block_t aBlock = ^
	{
		ok = SecTransformRegister(VerifyName, &VerifyTransform, error);
	};
	
	dispatch_once(&once, aBlock);

	if (!ok) 
	{
		return NULL;
	}
	
	
	SecTransformRef tr = SecTransformCreate(VerifyName, error);
	if (!tr) {
		return tr;
	}
	
	SecTransformSetAttribute(tr, kSecKeyAttributeName, key, error);
	if (signature) 
	{
		SecTransformSetAttribute(tr, kSecSignatureAttributeName, signature, error);
	}
	SecTransformSetAttribute(tr, kSecDigestTypeAttribute, kSecDigestSHA1, NULL);
	SecTransformSetAttribute(tr, kSecDigestTypeAttribute, kSecDigestSHA1, NULL);
	SecTransformSetAttribute(tr, kSecInputIsAttributeName, kSecInputIsPlainText, NULL);

	return tr;
}
