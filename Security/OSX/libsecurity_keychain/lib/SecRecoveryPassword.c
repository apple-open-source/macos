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
#include <Security/SecCFAllocator.h>
#include <Security/SecImportExport.h>
#include <Security/SecItem.h>
#include <Security/SecKey.h>
#include <Security/SecRandom.h>
#include <CommonCrypto/CommonKeyDerivation.h>
#include <CommonCrypto/CommonCryptor.h>
#include <CommonNumerics/CommonBaseXX.h>
#include <CoreFoundation/CFBase.h>
#include <fcntl.h>
#include <asl.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/debugging.h>

CFStringRef kSecRecVersionNumber = CFSTR("SRVersionNumber");
CFStringRef kSecRecQuestions = CFSTR("SRQuestions");
CFStringRef kSecRecLocale = CFSTR("SRLocale");
CFStringRef kSecRecIV = CFSTR("SRiv");
CFStringRef kSecRecWrappedPassword = CFSTR("SRWrappedPassword");


static const char *const std_log_prefix = "###SecRecovery Function: %s - ";
static const char *std_ident = "Security.framework";
static const char *std_facility = "InfoSec";
static uint32_t	std_options = 0;

static aslclient aslhandle = NULL;
static aslmsg msgptr = NULL;

// Error Reporting

void ccdebug_imp(int level, char *funcname, char *format, ...);

#define secDebug(lvl,fmt,...) sec_debug_imp(lvl, __PRETTY_FUNCTION__, fmt, __VA_ARGS__)

static void
sec_debug_init(void) {
	char *ccEnvStdErr = getenv("CC_STDERR");
	
	if(ccEnvStdErr != NULL && strncmp(ccEnvStdErr, "yes", 3) == 0) std_options |= ASL_OPT_STDERR;
	aslhandle = asl_open(std_ident, std_facility, std_options);
	
	msgptr = asl_new(ASL_TYPE_MSG);
	asl_set(msgptr, ASL_KEY_FACILITY, "com.apple.infosec");
}


static void
sec_debug_imp(int level, const char *funcname, const char *format, ...)
    __attribute__((format(printf, 3, 4)));

static void
sec_debug_imp(int level, const char *funcname, const char *format, ...) {
	if(aslhandle == NULL) sec_debug_init();
	
	char buf1[256], buf2[256];

	snprintf(buf1, sizeof(buf1), std_log_prefix, funcname);
	va_list argp;
	va_start(argp, format);
	vsnprintf(buf2, sizeof(buf2), format, argp);
	va_end(argp);

	asl_log(aslhandle, msgptr, level, "%s%s", buf1, buf2);
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

    CFDataRef inputString = CFStringCreateExternalRepresentation(kCFAllocatorDefault, str, kCFStringEncodingUTF8, 0xff);

    unsigned char outBuffer[CC_SHA256_DIGEST_LENGTH] = {0};
    (void)CC_SHA256(CFDataGetBytePtr(inputString), (CC_LONG)CFDataGetLength(inputString), outBuffer);
    retval = CFDataCreate(kCFAllocatorDefault, outBuffer, CC_SHA256_DIGEST_LENGTH);

    CFReleaseNull(inputString);
    return retval;
}

static CFDataRef CF_RETURNS_RETAINED
encryptOrDecryptAESCBC(CCOperation op, SecKeyRef wrapKey, CFDataRef iv, CFDataRef input, CFErrorRef *error)
{
    CCCryptorStatus status = kCCUnspecifiedError;
    CCCryptorRef cryptor = NULL;
    CFMutableDataRef output = NULL;
    CFDataRef outputCopy = NULL;

    CFDataRef keyBytes = NULL;
    OSStatus exportStatus = SecItemExport(wrapKey, kSecFormatRawKey, 0, NULL, &keyBytes);
    if (exportStatus != errSecSuccess) {
        if (error) {
            CFStringRef description = SecCopyErrorMessageString(exportStatus, NULL) ?: CFSTR("");
            CFAssignRetained(*error, CFErrorCreateWithUserInfoKeysAndValues(kCFAllocatorDefault, kCFErrorDomainOSStatus, exportStatus, (const void **)&kCFErrorLocalizedDescriptionKey, (const void **)&description, 1));
            CFReleaseNull(description);
        }
        goto out;
    }

    status = CCCryptorCreate(op, kCCAlgorithmAES, kCCOptionPKCS7Padding, CFDataGetBytePtr(keyBytes), CFDataGetLength(keyBytes), CFDataGetBytePtr(iv), &cryptor);
    if (status != kCCSuccess) {
        CFAssignRetained(*error, CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus, status, NULL));
        goto out;
    }
    size_t outputLen = CCCryptorGetOutputLength(cryptor, (size_t)CFDataGetLength(input), true);
    output = CFDataCreateMutableWithScratch(SecCFAllocatorZeroize(), outputLen);
    if (!output) {
        goto out;
    }
    UInt8 *outputPtr = CFDataGetMutableBytePtr(output);
    size_t chunkLen = 0;
    status = CCCryptorUpdate(cryptor, CFDataGetBytePtr(input), (size_t)CFDataGetLength(input), outputPtr, outputLen, &chunkLen);
    if (status != kCCSuccess) {
        CFAssignRetained(*error, CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus, status, NULL));
        goto out;
    }
    size_t remainingLen = outputLen - chunkLen;
    size_t finalChunkLen = 0;
    status = CCCryptorFinal(cryptor, &outputPtr[chunkLen], remainingLen, &finalChunkLen);
    if (status != kCCSuccess) {
        CFAssignRetained(*error, CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus, status, NULL));
        goto out;
    }
    CFDataSetLength(output, (CFIndex)(chunkLen + finalChunkLen));

out:
    outputCopy = output ? CFDataCreateCopy(SecCFAllocatorZeroize(), output) : NULL;
    CFReleaseNull(output);
    (void)CCCryptorRelease(cryptor);
    CFReleaseNull(keyBytes);

    return outputCopy;
}

static CFDataRef CF_RETURNS_RETAINED
encodeOrDecodeWithEncoding(CNEncodings encoding, CNEncodingDirection direction, CFDataRef input)
{
    CNEncoderRef encoder = NULL;
    CFMutableDataRef output = NULL;
    CFDataRef outputCopy = NULL;

    if (CNEncoderCreate(encoding, direction, &encoder) != kCNSuccess) {
        return NULL;
    }
    size_t outputLen = CNEncoderGetOutputLength(encoder, (size_t)CFDataGetLength(input));
    output = CFDataCreateMutableWithScratch(kCFAllocatorDefault, outputLen);
    if (!output) {
        goto out;
    }
    UInt8 *outputPtr = CFDataGetMutableBytePtr(output);
    size_t chunkLen = outputLen;
    // `chunkLen` is an in-out parameter: it's the total size of the output
    // buffer on the way in, and the number of bytes written on the way out.
    if (CNEncoderUpdate(encoder, CFDataGetBytePtr(input), (size_t)CFDataGetLength(input), outputPtr, &chunkLen) != kCNSuccess) {
        goto out;
    }
    size_t remainingLen = outputLen - chunkLen;
    size_t finalChunkLen = remainingLen;
    if (CNEncoderFinal(encoder, &outputPtr[chunkLen], &finalChunkLen) != kCNSuccess) {
        goto out;
    }
    CFDataSetLength(output, (CFIndex)(chunkLen + finalChunkLen));

out:
    outputCopy = output ? CFDataCreateCopy(kCFAllocatorDefault, output) : NULL;
    CFReleaseNull(output);
    (void)CNEncoderRelease(&encoder);

    return outputCopy;
}

static inline CFDataRef CF_RETURNS_RETAINED
b64encode(CFDataRef input)
{
    return encodeOrDecodeWithEncoding(kCNEncodingBase64, kCNEncode, input);
}

static inline CFDataRef CF_RETURNS_RETAINED
b64decode(CFDataRef input)
{
    return encodeOrDecodeWithEncoding(kCNEncodingBase64, kCNDecode, input);
}

static inline CFDataRef
b32encode(CFDataRef input)
{
    // `kCNEncodingBase32Recovery` uses the RFC 4648 Base 32 alphabet, but
    // replaces "I" with 8 and "S" with 9. It's the same alphabet as the
    // `kSecBase32FDEEncoding`.
    return encodeOrDecodeWithEncoding(kCNEncodingBase32Recovery, kCNEncode, input);
}

static CFDataRef CF_RETURNS_RETAINED
encryptString(SecKeyRef wrapKey, CFDataRef iv, CFStringRef str)
{
    CFDataRef retval = NULL;
    CFErrorRef error = NULL;
    CFDataRef inputString = CFStringCreateExternalRepresentation(kCFAllocatorDefault, str, kCFStringEncodingMacRoman, 0xff);
    CFDataRef encryptedData = NULL;

    encryptedData = encryptOrDecryptAESCBC(kCCEncrypt, wrapKey, iv, inputString, &error);
    if (!encryptedData) {
        goto out;
    }
    retval = b64encode(encryptedData);

out:
    if (error) {
        secerror("Failed to encrypt recovery password: %@", error);
    }

    CFReleaseNull(error);
    CFReleaseNull(inputString);
    CFReleaseNull(encryptedData);

    return retval;
}


static CFStringRef CF_RETURNS_RETAINED
decryptString(SecKeyRef wrapKey, CFDataRef iv, CFDataRef wrappedPassword)
{
    CFStringRef retval = NULL;
    CFErrorRef error = NULL;

    CFDataRef encryptedData = NULL;
    CFDataRef decryptedData = NULL;

    encryptedData = b64decode(wrappedPassword);
    if (!encryptedData) {
        goto out;
    }
    decryptedData = encryptOrDecryptAESCBC(kCCDecrypt, wrapKey, iv, encryptedData, &error);
    if (!decryptedData) {
        goto out;
    }
    retval = CFStringCreateFromExternalRepresentation(kCFAllocatorDefault, decryptedData, kCFStringEncodingMacRoman);

out:
    if (error) {
        secerror("Failed to decrypt recovery password: %@", error);
    }

    CFReleaseNull(error);
    CFReleaseNull(decryptedData);
    CFReleaseNull(encryptedData);

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
    if (!iv) {
        CFRelease(wrapKey);
        return NULL;
    }
	
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

    encodedData = b32encode(randData);
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
