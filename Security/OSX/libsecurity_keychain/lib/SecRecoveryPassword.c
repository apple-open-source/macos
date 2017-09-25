/*
 * Copyright (c) 2010-2012 Apple Inc. All Rights Reserved.
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

#include "SecRecoveryPassword.h"
#include <Security/SecTransform.h>
#include <Security/SecEncodeTransform.h>
#include <Security/SecDecodeTransform.h>
#include <Security/SecDigestTransform.h>
#include <Security/SecEncryptTransform.h>
#include <Security/SecItem.h>
#include <Security/SecKey.h>
#include <Security/SecRandom.h>
#include <CommonCrypto/CommonKeyDerivation.h>
#include <CommonCrypto/CommonCryptor.h>
#include <CoreFoundation/CFBase.h>
#include <fcntl.h>
#include <asl.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <utilities/SecCFRelease.h>
#include <utilities/debugging.h>

CFStringRef kSecRecVersionNumber = CFSTR("SRVersionNumber");
CFStringRef kSecRecQuestions = CFSTR("SRQuestions");
CFStringRef kSecRecLocale = CFSTR("SRLocale");
CFStringRef kSecRecIV = CFSTR("SRiv");
CFStringRef kSecRecWrappedPassword = CFSTR("SRWrappedPassword");


static char *std_log_prefix = "###SecRecovery Function: %s - %s";
static const char *std_ident = "Security.framework";
static const char *std_facility = "InfoSec";
static uint32_t	std_options = 0;

static aslclient aslhandle = NULL;
static aslmsg msgptr = NULL;

// Error Reporting

void ccdebug_imp(int level, char *funcname, char *format, ...);

#define secDebug(lvl,fmt,...) sec_debug_imp(lvl, __PRETTY_FUNCTION__, fmt, __VA_ARGS__)

static void
sec_debug_init() {
	char *ccEnvStdErr = getenv("CC_STDERR");
	
	if(ccEnvStdErr != NULL && strncmp(ccEnvStdErr, "yes", 3) == 0) std_options |= ASL_OPT_STDERR;
	aslhandle = asl_open(std_ident, std_facility, std_options);
	
	msgptr = asl_new(ASL_TYPE_MSG);
	asl_set(msgptr, ASL_KEY_FACILITY, "com.apple.infosec");
}


static void
sec_debug_imp(int level, const char *funcname, char *format, ...) {
	va_list argp;
	char fmtbuffer[256];
	
	if(aslhandle == NULL) sec_debug_init();
	
	sprintf(fmtbuffer, std_log_prefix, funcname, format);
	va_start(argp, format);
	asl_vlog(aslhandle, msgptr, level, fmtbuffer, argp);
	va_end(argp);
}

// Read /dev/random for random bytes

static CFDataRef
createRandomBytes(size_t len)
{
    CFMutableDataRef data = CFDataCreateMutable(NULL, len);
    if (data == NULL)
        return NULL;
    CFDataSetLength(data, len);
    if (SecRandomCopyBytes(kSecRandomDefault, len, CFDataGetMutableBytePtr(data)) != noErr) {
        CFRelease(data);
        return NULL;
    }
    return data;
}

// This is the normalization routine - subject to change.  We need to make sure that whitespace is removed and
// that upper/lower case is normalized, etc for all possible languages.

static void secNormalize(CFMutableStringRef theString, CFLocaleRef theLocale)
{
	CFRange theRange;
	
	CFStringFold(theString, kCFCompareCaseInsensitive | kCFCompareDiacriticInsensitive | kCFCompareWidthInsensitive, theLocale);
	CFStringNormalize(theString, kCFStringNormalizationFormKC);
	CFStringTrimWhitespace(theString);
	while(CFStringFindCharacterFromSet(theString, CFCharacterSetGetPredefined(kCFCharacterSetWhitespace), CFRangeMake(0, CFStringGetLength(theString)), kCFCompareBackwards, &theRange))
		CFStringDelete(theString, theRange);
}

/*
 * This will derive a 128 bit (16 byte) key from a set of answers to questions in a CFArray of CFStrings.
 * it normalizes each answer and concats them into a collector buffer.  The resulting string is run through
 * PBKDF2-HMAC-SHA256 to form a key.
 *
 * Todo: For version 2 it would be better to randomly generate the salt and make the iteration count flexible.
 * This would require a different return value because that information would need to be returned up the stack
 * to the callers.  Given the time left in this release (Lion) we're going with set values for this.
 */

#define RETURN_KEY_SIZE 16
#define MAXANSWERBUFF 4096
#define PBKDF_ROUNDS 100000

static SecKeyRef CF_RETURNS_RETAINED
secDeriveKeyFromAnswers(CFArrayRef answers, CFLocaleRef theLocale)
{
    static const uint8_t salt[16] = { 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x1F };
    static const int saltLen = sizeof(salt);
    
    SecKeyRef theKey = NULL;
    uint8_t rawKeyData[RETURN_KEY_SIZE];
    
    CFIndex encodedAnswers = 0;
    CFIndex numAnswers = CFArrayGetCount(answers);
    const size_t concatenatedAnswersSize = MAXANSWERBUFF * numAnswers;
    
    char *concatenatedAnswers = (char *)malloc(concatenatedAnswersSize);
    if (concatenatedAnswers == NULL) {
        return NULL;
    }
    
    concatenatedAnswers[0] = 0; // NUL terminate
    
    int i;
    for (i = 0; i < numAnswers; i++) {
        CFMutableStringRef answer = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFArrayGetValueAtIndex(answers, i));
        if (answer) {
            secNormalize(answer, theLocale);
            
            CFIndex theAnswerLen = CFStringGetLength(answer);
            CFIndex theAnswerSize = CFStringGetMaximumSizeForEncoding(theAnswerLen, kCFStringEncodingUTF8);
            char *theAnswer = (char *)malloc(theAnswerSize + 1); // add space for NUL byte
            if (theAnswer) {
                if (theAnswerLen == CFStringGetBytes(answer, CFRangeMake(0, CFStringGetLength(answer)), kCFStringEncodingUTF8, '?', FALSE, (UInt8*)theAnswer, theAnswerSize, &theAnswerSize)) {
                    theAnswer[theAnswerSize] = 0; // NUL terminate
                    if (strlcat(concatenatedAnswers, theAnswer, concatenatedAnswersSize) < concatenatedAnswersSize) {
                        encodedAnswers += 1;
                    }
                }
                bzero(theAnswer, theAnswerSize);
                free(theAnswer);
            }
            CFRelease(answer);
        }
    }
    
    // one or more of the answers failed to encode
    if (encodedAnswers != numAnswers) {
        free(concatenatedAnswers);
        return NULL;
    }
    
    if (CCKeyDerivationPBKDF(kCCPBKDF2, concatenatedAnswers, strlen(concatenatedAnswers), salt, saltLen, kCCPRFHmacAlgSHA256, PBKDF_ROUNDS, rawKeyData, RETURN_KEY_SIZE)) {
        free(concatenatedAnswers);
        return NULL;
    }
    
    CFDataRef keyData = CFDataCreate(kCFAllocatorDefault, rawKeyData, RETURN_KEY_SIZE);
    if (keyData) {
        CFMutableDictionaryRef params = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (params) {
            CFErrorRef error = NULL;
            CFDictionaryAddValue(params, kSecAttrKeyType, kSecAttrKeyTypeAES);
            theKey = SecKeyCreateFromData(params, keyData, &error);
            if (error) {
                CFRelease(error);
            }
            CFRelease(params);
        }
        CFRelease(keyData);
    }
    
    bzero(rawKeyData, RETURN_KEY_SIZE);
    bzero(concatenatedAnswers, concatenatedAnswersSize);
    free(concatenatedAnswers);
    return theKey;
}


// Single shot CFString processing routines for digests/encoding/encrypt/decrypt

static CFDataRef
createDigestString(CFStringRef str)
{
	CFDataRef retval = NULL;
 	CFErrorRef error = NULL;
    
    CFDataRef inputString = CFStringCreateExternalRepresentation(kCFAllocatorDefault, str, kCFStringEncodingUTF8, 0xff);
    
    SecTransformRef	digestTrans = SecDigestTransformCreate(kSecDigestSHA2, 256, &error);
    if(error == NULL) {
        SecTransformSetAttribute(digestTrans, kSecTransformInputAttributeName, inputString, &error);
        if(error == NULL) {
        	retval = SecTransformExecute(digestTrans, &error);
            if(retval == NULL) {
                CFStringRef errorReason = CFErrorCopyFailureReason(error);
                secDebug(ASL_LEVEL_ERR, "Couldn't create digest %s\n", CFStringGetCStringPtr(errorReason, kCFStringEncodingUTF8));
                CFReleaseNull(errorReason);
            }
        }
    }
    CFReleaseNull(digestTrans);
    CFReleaseNull(inputString);
    return retval;
}

static CFDataRef CF_RETURNS_RETAINED
b64encode(CFDataRef input)
{
	CFDataRef retval = NULL;
    CFErrorRef error = NULL;
	SecTransformRef encodeTrans = SecEncodeTransformCreate(kSecBase64Encoding, &error);
    if(error == NULL) SecTransformSetAttribute(encodeTrans, kSecTransformInputAttributeName, input, &error);
   	if(error == NULL) retval = SecTransformExecute(encodeTrans, &error);
	if(encodeTrans) CFRelease(encodeTrans);
    return retval;
}

static CFDataRef CF_RETURNS_RETAINED
b64decode(CFDataRef input)
{
	CFDataRef retval = NULL;
    CFErrorRef error = NULL;
	SecTransformRef decodeTrans = SecDecodeTransformCreate(kSecBase64Encoding, &error);
    if(error == NULL) SecTransformSetAttribute(decodeTrans, kSecTransformInputAttributeName, input, &error);
    if(error == NULL) retval = SecTransformExecute(decodeTrans, &error);
	if(decodeTrans) CFRelease(decodeTrans);
    return retval;
}

static CFDataRef CF_RETURNS_RETAINED
encryptString(SecKeyRef wrapKey, CFDataRef iv, CFStringRef str)
{
	CFDataRef retval = NULL;
 	CFErrorRef error = NULL;
    CFDataRef inputString = CFStringCreateExternalRepresentation(kCFAllocatorDefault, str, kCFStringEncodingMacRoman, 0xff);
    SecTransformRef encrypt = NULL;
    SecTransformRef encode = NULL;
    SecTransformRef group = NULL;

    encrypt = SecEncryptTransformCreate(wrapKey, &error);
    if (error) goto out;
    SecTransformSetAttribute(encrypt, kSecEncryptionMode, kSecModeCBCKey, &error);
    if (error) goto out;
    SecTransformSetAttribute(encrypt, kSecPaddingKey, kSecPaddingPKCS7Key, &error);
    if (error) goto out;
    SecTransformSetAttribute(encrypt, kSecTransformInputAttributeName, inputString, &error);
    if (error) goto out;
    SecTransformSetAttribute(encrypt, kSecIVKey, iv, &error);
    if (error) goto out;
    
    encode = SecEncodeTransformCreate(kSecBase64Encoding, &error);
    if (error) goto out;
    
    group = SecTransformCreateGroupTransform();
    SecTransformConnectTransforms(encrypt, kSecTransformOutputAttributeName, encode, kSecTransformInputAttributeName, group, &error);
    if (error) goto out;
    retval = SecTransformExecute(group, &error);
    if (error) goto out;
    
out:
    if (error) {
        secerror("Failed to encrypt recovery password: %@", error);
    }
    
    CFReleaseNull(error);
    CFReleaseNull(inputString);
    CFReleaseNull(encrypt);
    CFReleaseNull(encode);
    CFReleaseNull(group);
    
    return retval;
}


static CFStringRef CF_RETURNS_RETAINED
decryptString(SecKeyRef wrapKey, CFDataRef iv, CFDataRef wrappedPassword)
{
	CFStringRef retval = NULL;
	CFDataRef retData = NULL;
    CFErrorRef error = NULL;
    SecTransformRef decode = NULL;
    SecTransformRef decrypt = NULL;
    SecTransformRef group = NULL;

    decode = SecDecodeTransformCreate(kSecBase64Encoding, &error);
    if (error) goto out;
    SecTransformSetAttribute(decode, kSecTransformInputAttributeName, wrappedPassword, &error);
    if (error) goto out;
    
    decrypt = SecDecryptTransformCreate(wrapKey, &error);
    if (error) goto out;
    SecTransformSetAttribute(decrypt, kSecEncryptionMode, kSecModeCBCKey, &error);
    if (error) goto out;
    SecTransformSetAttribute(decrypt, kSecPaddingKey, kSecPaddingPKCS7Key, &error);
    if (error) goto out;
    SecTransformSetAttribute(decrypt, kSecIVKey, iv, &error);
    if (error) goto out;
    
    group = SecTransformCreateGroupTransform();
    SecTransformConnectTransforms(decode, kSecTransformOutputAttributeName, decrypt, kSecTransformInputAttributeName, group, &error);
    if (error) goto out;
    retData =  SecTransformExecute(group, &error);
    if (error) goto out;
    retval = CFStringCreateFromExternalRepresentation(kCFAllocatorDefault, retData, kCFStringEncodingMacRoman);
    
out:
    if (error) {
        secerror("Failed to decrypt recovery password: %@", error);
    }
    
    CFReleaseNull(retData);
    CFReleaseNull(error);
    CFReleaseNull(decode);
    CFReleaseNull(decrypt);
    CFReleaseNull(group);
    
    return retval;
}

// IV for the recovery ref is currently the leftmost 16 bytes of the digest of the recovery password.
#define IVBYTECOUNT 16

static CFDataRef
createIVFromPassword(CFStringRef password)
{
    CFDataRef 		   hashedPassword, retval;
    CFMutableDataRef   iv;
    if((hashedPassword = createDigestString(password)) == NULL) return NULL;
    iv = CFDataCreateMutableCopy(kCFAllocatorDefault, CFDataGetLength(hashedPassword)+1, hashedPassword);
    CFDataDeleteBytes(iv, CFRangeMake(IVBYTECOUNT, CFDataGetLength(iv)-IVBYTECOUNT));
    retval = CFDataCreateCopy(kCFAllocatorDefault, iv);
    CFRelease(hashedPassword);
    CFRelease(iv);
    return retval;
}


/*
 * API functions
 */

/*
 * Function:	SecWrapRecoveryPasswordWithAnswers
 * Description:	This will wrap a password by using answers to generate a key.  The resulting
 * 				wrapped password and the questions used to get the answers are saved in a 
 *              recovery dictionary.
 */

CFDictionaryRef CF_RETURNS_RETAINED
SecWrapRecoveryPasswordWithAnswers(CFStringRef password, CFArrayRef questions, CFArrayRef answers)
{
    uint32_t 	vers = 1;
    CFDataRef	iv = NULL;
	CFDataRef	wrappedPassword = NULL;
	CFMutableDictionaryRef retval = NULL;
	CFLocaleRef theLocale = CFLocaleCopyCurrent();
    CFStringRef theLocaleString = CFLocaleGetIdentifier(theLocale);
    SecKeyRef wrapKey = NULL;
    
    CFIndex ix, limit;
    
    if (!password || !questions || !answers) {
        goto error;
    }
    
    limit = CFArrayGetCount(answers);
    if (limit != CFArrayGetCount(questions)) {
        goto error;
    }
	CFTypeRef chkval;
    for (ix=0; ix<limit; ix++)
	{
		chkval =  CFArrayGetValueAtIndex(answers, ix);
        if (!chkval || CFGetTypeID(chkval)!=CFStringGetTypeID() || CFEqual((CFStringRef)chkval, CFSTR(""))) {
            goto error;
        }
        chkval = CFArrayGetValueAtIndex(questions, ix);
        if (!chkval || CFGetTypeID(chkval)!=CFStringGetTypeID() || CFEqual((CFStringRef)chkval, CFSTR(""))) {
            goto error;
        }
    }
	
    iv = createIVFromPassword(password);
    
    wrapKey = secDeriveKeyFromAnswers(answers, theLocale);
	
    if((wrappedPassword = encryptString(wrapKey, iv, password)) != NULL) {
        retval = CFDictionaryCreateMutable(kCFAllocatorDefault, 5, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &vers);
		CFDictionaryAddValue(retval, kSecRecVersionNumber, num);
        CFReleaseNull(num);
		CFDictionaryAddValue(retval, kSecRecQuestions, questions);
		CFDictionaryAddValue(retval, kSecRecLocale, theLocaleString);
        CFDataRef ivdata = b64encode(iv);
		CFDictionaryAddValue(retval, kSecRecIV, ivdata);
        CFReleaseNull(ivdata);
		CFDictionaryAddValue(retval, kSecRecWrappedPassword, wrappedPassword);
	}
	
    CFReleaseNull(wrappedPassword);
    goto out;
error:
    CFReleaseNull(retval);
out:
    CFReleaseNull(iv);
    CFReleaseNull(wrapKey);
    CFReleaseNull(theLocale);

  	return retval;
}

/*
 * Function:	SecUnwrapRecoveryPasswordWithAnswers
 * Description:	This will unwrap a password contained in a recovery dictionary by using answers
 * 				to generate a key.
 */

CFStringRef CF_RETURNS_RETAINED
SecUnwrapRecoveryPasswordWithAnswers(CFDictionaryRef recref, CFArrayRef answers)
{    
	if(answers == NULL || CFArrayGetCount(answers) < 3) return NULL;

	CFStringRef theLocaleString = (CFStringRef) CFDictionaryGetValue(recref, kSecRecLocale);
	CFDataRef tmpIV = (CFDataRef) CFDictionaryGetValue(recref, kSecRecIV);
	CFDataRef wrappedPassword = (CFDataRef) CFDictionaryGetValue(recref, kSecRecWrappedPassword);
	
	if(theLocaleString == NULL || tmpIV == NULL || wrappedPassword == NULL) {
		return NULL;
	}
	

    CFLocaleRef theLocale = CFLocaleCreate(kCFAllocatorDefault, theLocaleString);
	SecKeyRef wrapKey = secDeriveKeyFromAnswers(answers, theLocale);
	CFRelease(theLocale);
	
    CFDataRef iv = b64decode(tmpIV);
	
	CFStringRef recoveryPassword =  decryptString(wrapKey, iv, wrappedPassword);
	CFRelease(wrapKey);
   
    if(recoveryPassword != NULL) {
    	CFDataRef comphash = createIVFromPassword(recoveryPassword);
       	if(!CFEqual(comphash, iv)) {
            secDebug(ASL_LEVEL_ERR, "Failed reconstitution of password for recovery\n", NULL);
			CFRelease(recoveryPassword);
			recoveryPassword = NULL;
		}
		CFRelease(comphash);
    }
	CFRelease(iv);
	return recoveryPassword;
}

/*
 * Function:	SecCreateRecoveryPassword
 * Description:	This function will get a random 128 bit number and base32
 * 				encode and format that value
 */
 
CFStringRef 
SecCreateRecoveryPassword(void)
{
	CFStringRef result = NULL;
	CFErrorRef error = NULL;
 	CFDataRef encodedData = NULL;
    CFDataRef randData = createRandomBytes(16);
	int i;
	
	// base32FDE is a "private" base32 encoding, it has no 0/O or L/l/1 in it (it uses 8 and 9).
	SecTransformRef	encodeTrans = SecEncodeTransformCreate(CFSTR("base32FDE"), &error);
    if(error == NULL) {
		SecTransformSetAttribute(encodeTrans, kSecTransformInputAttributeName, randData, &error);
		if(error == NULL) encodedData = SecTransformExecute(encodeTrans, &error);
   	}
    CFReleaseNull(encodeTrans);
    CFRelease(randData);

	if(encodedData != NULL && error == NULL) {
        CFStringRef	b32string = CFStringCreateFromExternalRepresentation(kCFAllocatorDefault, encodedData, kCFStringEncodingMacRoman);
        CFMutableStringRef encodedString = CFStringCreateMutableCopy(kCFAllocatorDefault, 64, b32string);
       
        // Add some hyphens to make the generated password easier to use
        for(i = 4; i < 34; i += 5)  CFStringInsert(encodedString, i, CFSTR("-"));
        // Trim so the last section is 4 characters long
		CFStringDelete(encodedString, CFRangeMake(29,CFStringGetLength(encodedString)-29));
        result = CFStringCreateCopy(kCFAllocatorDefault, encodedString);
        CFRelease(encodedString);
        CFRelease(b32string);
	} else {
        secDebug(ASL_LEVEL_ERR, "Failed to base32 encode random data for recovery password\n", NULL);
    }
    CFReleaseNull(encodedData);

	return result;
	
}
