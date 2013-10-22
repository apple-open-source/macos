/*
 * Copyright (c) 2012 Apple Computer, Inc. All Rights Reserved.
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
 * tsaSupport.c -  ASN1 templates Time Stamping Authority requests and responses
 */

/*
#include <Security/SecCmsDigestContext.h>
#include <Security/SecCmsMessage.h>
#include <security_asn1/secasn1.h>
#include <security_asn1/secerr.h>
*/

#include <security_utilities/debugging.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

#include <Security/SecCmsDecoder.h>
#include <Security/SecCmsMessage.h>
#include <Security/SecCmsContentInfo.h>
#include <Security/SecCmsSignedData.h>
#include <Security/SecCmsSignerInfo.h>
#include "tsaTemplates.h"
#include <Security/SecAsn1Coder.h>
#include <AssertMacros.h>
#include <Security/SecPolicy.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecImportExport.h>
#include <Security/SecCertificatePriv.h>
#include <security_keychain/SecCertificateP.h>
#include <security_keychain/SecCertificatePrivP.h>

#include "tsaSupport.h"
#include "tsaSupportPriv.h"
#include "tsaTemplates.h"
#include "cmslocal.h"

#include "secoid.h"
#include "secitem.h"
#include <fcntl.h>

const CFStringRef kTSAContextKeyURL = CFSTR("ServerURL");
const CFStringRef kTSAContextKeyNoCerts = CFSTR("NoCerts");
const CFStringRef kTSADebugContextKeyBadReq = CFSTR("DebugBadReq");
const CFStringRef kTSADebugContextKeyBadNonce = CFSTR("DebugBadNonce");

extern const SecAsn1Template kSecAsn1TSATSTInfoTemplate[];

extern OSStatus impExpImportCertCommon(
	const CSSM_DATA		*cdata,
	SecKeychainRef		importKeychain, // optional
	CFMutableArrayRef	outArray);		// optional, append here

#pragma mark ----- Debug Logs -----

#ifndef NDEBUG
#define TSA_USE_SYSLOG 1
#endif

#if TSA_USE_SYSLOG
#include <syslog.h>
    #include <time.h>
    #include <sys/time.h>
    #define tsaDebug(fmt, ...) \
        do { if (true) { \
            char buf[64];   \
            struct timeval time_now;   \
            gettimeofday(&time_now, NULL);  \
            struct tm* time_info = localtime(&time_now.tv_sec);    \
            strftime(buf, sizeof(buf), "[%Y-%m-%d %H:%M:%S]", time_info);   \
                    fprintf(stderr, "%s " fmt, buf, ## __VA_ARGS__); \
                    syslog(LOG_ERR, " " fmt, ## __VA_ARGS__); \
                    } } while (0)
    #define tsa_secdebug(scope, format...) \
    { \
        syslog(LOG_NOTICE, format); \
        secdebug(scope, format); \
        printf(format); \
    }
#else
    #define tsaDebug(args...)			tsa_secdebug("tsa", ## args)
#define tsa_secdebug(scope, format...) \
        secdebug(scope, format)
#endif

#ifndef NDEBUG
#define TSTINFO_DEBUG	1   //jch
#endif

#if	TSTINFO_DEBUG
#define dtprintf(args...)    tsaDebug(args)
#else
#define dtprintf(args...)
#endif

#define kHTTPResponseCodeContinue               100
#define kHTTPResponseCodeOK                     200
#define kHTTPResponseCodeNoContent              204
#define kHTTPResponseCodeBadRequest             400
#define kHTTPResponseCodeUnauthorized           401
#define kHTTPResponseCodeForbidden              403
#define kHTTPResponseCodeNotFound               404
#define kHTTPResponseCodeConflict               409
#define kHTTPResponseCodeExpectationFailed      417
#define kHTTPResponseCodeServFail               500
#define kHTTPResponseCodeServiceUnavailable     503
#define kHTTPResponseCodeInsufficientStorage    507

#pragma mark ----- Debug/Utilities -----

static OSStatus remapHTTPErrorCodes(OSStatus status)
{
    switch (status)
    {
    case kHTTPResponseCodeOK:
    case kHTTPResponseCodeContinue:
        return noErr;
    case kHTTPResponseCodeBadRequest:
        return errSecTimestampBadRequest;
    case kHTTPResponseCodeNoContent:
    case kHTTPResponseCodeUnauthorized:
    case kHTTPResponseCodeForbidden:
    case kHTTPResponseCodeNotFound:
    case kHTTPResponseCodeConflict:
    case kHTTPResponseCodeExpectationFailed:
    case kHTTPResponseCodeServFail:
    case kHTTPResponseCodeInsufficientStorage:
    case kHTTPResponseCodeServiceUnavailable:
        return errSecTimestampServiceNotAvailable;
    default:
        return status;
    }
    return status;

}

static void printDataAsHex(const char *title, const CSSM_DATA *d, unsigned maxToPrint) // 0 means print it all
{
#ifndef NDEBUG
    unsigned i;
    bool more = false;
    uint32 len = (uint32)d->Length;
    uint8 *cp = d->Data;
    char *buffer = NULL;
    size_t bufferSize;
    int offset, sz = 0;
    const int wrapwid = 24;     // large enough so SHA-1 hashes fit on one line...

    if ((maxToPrint != 0) && (len > maxToPrint))
    {
        len = maxToPrint;
        more = true;
    }

    bufferSize = wrapwid+3*len;
    buffer = (char *)malloc(bufferSize);

    offset = sprintf(buffer, "%s [len = %u]\n", title, len);
    dtprintf("%s", buffer);
    offset = 0;

    for (i=0; (i < len) && (offset+3 < bufferSize); i++, offset += sz)
    {
        sz = sprintf(buffer + offset, " %02x", (unsigned int)cp[i] & 0xff);
        if ((i % wrapwid) == (wrapwid-1))
        {
            dtprintf("%s", buffer);
            offset = 0;
            sz = 0;
        }
    }

    sz=sprintf(buffer + offset, more?" ...\n":"\n");
        offset += sz;
    buffer[offset+1]=0;

//    fprintf(stderr, "%s", buffer);
    dtprintf("%s", buffer);
#endif
}

#ifndef NDEBUG
int tsaWriteFileX(const char *fileName, const unsigned char *bytes, size_t numBytes)
{
    int rtn;
    int fd;

    fd = open(fileName, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (fd <= 0)
        return errno;

    rtn = (int)write(fd, bytes, numBytes);
    if(rtn != (int)numBytes)
    {
        if (rtn >= 0)
            fprintf(stderr, "writeFile: short write\n");
        rtn = EIO;
    }
    else
        rtn = 0;

    close(fd);
    return rtn;
}
#endif

char *cfStringToChar(CFStringRef inStr)
{
    // Caller must free
    char *result = NULL;
    const char *str = NULL;

	if (!inStr)
        return strdup("");     // return a null string

	// quick path first
	if ((str = CFStringGetCStringPtr(inStr, kCFStringEncodingUTF8))) {
        result = strdup(str);
	} else {
        // need to extract into buffer
        CFIndex length = CFStringGetLength(inStr);  // in 16-bit character units
        CFIndex bytesToAllocate = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
        result = malloc(bytesToAllocate);
        if (!CFStringGetCString(inStr, result, bytesToAllocate, kCFStringEncodingUTF8))
            result[0] = 0;
    }
    
	return result;
}

/* Oids longer than this are considered invalid. */
#define MAX_OID_SIZE				32

#ifndef NDEBUG
/* FIXME: There are other versions of this in SecCertifcate.c and SecCertificateP.c */
static CFStringRef SecDERItemCopyOIDDecimalRepresentation(CFAllocatorRef allocator, const CSSM_OID *oid)
{
	if (oid->Length == 0)
        return CFSTR("<NULL>");

	if (oid->Length > MAX_OID_SIZE)
        return CFSTR("Oid too long");

    CFMutableStringRef result = CFStringCreateMutable(allocator, 0);

	// The first two levels are encoded into one byte, since the root levelq
	// has only 3 nodes (40*x + y).  However if x = joint-iso-itu-t(2) then
	// y may be > 39, so we have to add special-case handling for this.
	uint32_t x = oid->Data[0] / 40;
	uint32_t y = oid->Data[0] % 40;
	if (x > 2)
	{
		// Handle special case for large y if x = 2
		y += (x - 2) * 40;
		x = 2;
	}
    CFStringAppendFormat(result, NULL, CFSTR("%u.%u"), x, y);

	uint32_t value = 0;
	for (x = 1; x < oid->Length; ++x)
	{
		value = (value << 7) | (oid->Data[x] & 0x7F);
        /* @@@ value may not span more than 4 bytes. */
        /* A max number of 20 values is allowed. */
		if (!(oid->Data[x] & 0x80))
		{
            CFStringAppendFormat(result, NULL, CFSTR(".%lu"), (unsigned long)value);
			value = 0;
		}
	}
	return result;
}
#endif

static void debugSaveCertificates(CSSM_DATA **outCerts)
{
#ifndef NDEBUG
    if (outCerts)
    {
        CSSM_DATA_PTR *certp;
        unsigned jx = 0;
        const char *certNameBase = "/tmp/tsa-resp-cert-";
        char fname[PATH_MAX];
        unsigned certCount = SecCmsArrayCount((void **)outCerts);
        dtprintf("Found %d certs\n",certCount);

        for (certp=outCerts;*certp;certp++, ++jx)
        {
            char numstr[32];
            strncpy(fname, certNameBase, strlen(certNameBase)+1);
            sprintf(numstr,"%u", jx);
            strcat(fname,numstr);
            tsaWriteFileX(fname, (*certp)->Data, (*certp)->Length);
            if (jx > 5)
                break;  //something wrong
        }
    }
#endif
}

static void debugShowSignerInfo(SecCmsSignedDataRef signedData)
{
#ifndef NDEBUG
    int numberOfSigners = SecCmsSignedDataSignerInfoCount (signedData);
    dtprintf("numberOfSigners : %d\n", numberOfSigners);
    int ix;
    for (ix=0;ix < numberOfSigners;ix++)
    {
        SecCmsSignerInfoRef sigi = SecCmsSignedDataGetSignerInfo(signedData,ix);
        if (sigi)
        {
            CFStringRef commonName = SecCmsSignerInfoGetSignerCommonName(sigi);
            const char *signerhdr = "      signer    : ";
            if (commonName)
            {
                char *cn = cfStringToChar(commonName);
                dtprintf("%s%s\n", signerhdr, cn);
                if (cn)
                    free(cn);
            }
            else
                dtprintf("%s<NULL>\n", signerhdr);
         }
    }
#endif
}

static void debugShowContentTypeOID(SecCmsContentInfoRef contentInfo)
{
#ifndef NDEBUG

    CSSM_OID *typeOID = SecCmsContentInfoGetContentTypeOID(contentInfo);
    if (typeOID)
    {
        CFStringRef oidCFStr = SecDERItemCopyOIDDecimalRepresentation(kCFAllocatorDefault, typeOID);
        char *oidstr = cfStringToChar(oidCFStr);
        printDataAsHex("oid:", typeOID, (unsigned int)typeOID->Length);
        dtprintf("\toid: %s\n", oidstr);
        if (oidCFStr)
            CFRelease(oidCFStr);
        if (oidstr)
            free(oidstr);
    }
#endif
}

uint64_t tsaDER_ToInt(const CSSM_DATA *DER_Data)
{
	uint64_t    rtn = 0;
	unsigned	i = 0;

	while(i < DER_Data->Length) {
		rtn |= DER_Data->Data[i];
		if(++i == DER_Data->Length) {
			break;
		}
		rtn <<= 8;
	}
	return rtn;
}

void displayTSTInfo(SecAsn1TSATSTInfo *tstInfo)
{
#ifndef NDEBUG
    dtprintf("--- TSTInfo ---\n");
    if (!tstInfo)
        return;

    if (tstInfo->version.Data)
    {
        uint64_t vers = tsaDER_ToInt(&tstInfo->version);
        dtprintf("Version:\t\t%u\n", (int)vers);
    }

    if (tstInfo->serialNumber.Data)
    {
        uint64_t sn = tsaDER_ToInt(&tstInfo->serialNumber);
        dtprintf("SerialNumber:\t%llu\n", sn);
    }

    if (tstInfo->ordering.Data)
    {
        uint64_t ord = tsaDER_ToInt(&tstInfo->ordering);
        dtprintf("Ordering:\t\t%s\n", ord?"yes":"no");
    }

    if (tstInfo->nonce.Data)
    {
        uint64_t nonce = tsaDER_ToInt(&tstInfo->nonce);
        dtprintf("Nonce:\t\t%llu\n", nonce);
    }
    else
        dtprintf("Nonce:\t\tnot specified\n");

    if (tstInfo->genTime.Data)
    {
        char buf[tstInfo->genTime.Length+1];
        memcpy(buf, (const char *)tstInfo->genTime.Data, tstInfo->genTime.Length);
        buf[tstInfo->genTime.Length]=0;
        dtprintf("GenTime:\t\t%s\n", buf);
    }

    dtprintf("-- MessageImprint --\n");
    if (true)   // SecAsn1TSAMessageImprint
    {
        printDataAsHex(" Algorithm:",&tstInfo->messageImprint.hashAlgorithm.algorithm, 0);
        printDataAsHex(" Message  :", &tstInfo->messageImprint.hashedMessage, 0);//tstInfo->messageImprint.hashedMessage.Length);
    }
#endif
}

#pragma mark ----- TimeStamp Response using XPC -----

#include <xpc/private.h>

static OSStatus checkForNonDERResponse(const unsigned char *resp, size_t respLen)
{
    /*
        Good start is something like 30 82 0c 03 30 15 02 01  00 30 10 0c 0e 4f 70 65

        URL:    http://timestamp-int.corp.apple.com/signserver/process?TimeStampSigner
        Resp:   Http/1.1 Service Unavailable

        URL:    http://timestamp-int.corp.apple.com/ts01
        Resp:   blank

        URL:    http://cutandtaste.com/404 (or other forced 404 site)
        Resp:   404
    */

    OSStatus status = noErr;
    const char ader[2] = { 0x30, 0x82 };
    char *respStr = NULL;
    size_t maxlen = 0;
    size_t badResponseCount;

    const char *badResponses[] =
    {
        "<!DOCTYPE html>",
        "Http/1.1 Service Unavailable",
        "blank"
    };

    require_action(resp && respLen, xit, status = errSecTimestampServiceNotAvailable);

    // This is usual case
    if ((respLen > 1) && (memcmp(resp, ader, 2)==0))    // might be good; pass on to DER decoder
        return noErr;

    badResponseCount = sizeof(badResponses)/sizeof(char *);
    int ix;
    for (ix = 0; ix < badResponseCount; ++ix)
        if (strlen(badResponses[ix]) > maxlen)
            maxlen = strlen(badResponses[ix]);

    // Prevent a large response from allocating a ton of memory
    if (respLen > maxlen)
        respLen = maxlen;

    respStr = (char *)malloc(respLen+1);
    strlcpy(respStr, (const char *)resp, respLen);

    for (ix = 0; ix < badResponseCount; ++ix)
        if (strcmp(respStr, badResponses[ix])==0)
            return errSecTimestampServiceNotAvailable;

xit:
    if (respStr)
        free((void *)respStr);

    return status;
}

static OSStatus sendTSARequestWithXPC(const unsigned char *tsaReq, size_t tsaReqLength, const unsigned char *tsaURL, unsigned char **tsaResp, size_t *tsaRespLength)
{
    __block OSStatus result = noErr;
    int timeoutInSeconds = 15;
    extern xpc_object_t xpc_create_with_format(const char * format, ...);

	dispatch_queue_t xpc_queue = dispatch_queue_create("com.apple.security.XPCTimeStampingService", DISPATCH_QUEUE_SERIAL);
    __block dispatch_semaphore_t waitSemaphore = dispatch_semaphore_create(0);
    dispatch_time_t finishTime = dispatch_time(DISPATCH_TIME_NOW, timeoutInSeconds * NSEC_PER_SEC);

    xpc_connection_t con = xpc_connection_create("com.apple.security.XPCTimeStampingService", xpc_queue);

    xpc_connection_set_event_handler(con, ^(xpc_object_t event) {
        xpc_type_t xtype = xpc_get_type(event);
        if (XPC_TYPE_ERROR == xtype)
        {    tsaDebug("default: connection error: %s\n", xpc_dictionary_get_string(event, XPC_ERROR_KEY_DESCRIPTION)); }
        else
         {   tsaDebug("default: unexpected connection event %p\n", event); }
    });

    xpc_connection_resume(con);

    xpc_object_t tsaReqData = xpc_data_create(tsaReq, tsaReqLength);
    const char *urlstr = (tsaURL?(const char *)tsaURL:"");
    xpc_object_t url_as_xpc_string = xpc_string_create(urlstr);

    xpc_object_t message = xpc_create_with_format("{operation: TimeStampRequest, ServerURL: %value, TimeStampRequest: %value}", url_as_xpc_string, tsaReqData);

    xpc_connection_send_message_with_reply(con, message, xpc_queue, ^(xpc_object_t reply)
    {
        tsaDebug("xpc_connection_send_message_with_reply handler called back\n");
        dispatch_retain(waitSemaphore);

    xpc_type_t xtype = xpc_get_type(reply);
        if (XPC_TYPE_ERROR == xtype)
            {   tsaDebug("message error: %s\n", xpc_dictionary_get_string(reply, XPC_ERROR_KEY_DESCRIPTION)); }
        else if (XPC_TYPE_CONNECTION == xtype)
            { tsaDebug("received connection\n"); }
        else if (XPC_TYPE_DICTIONARY == xtype)
    {
#ifndef NDEBUG
        /*
         // This is useful for debugging.
        char *debug = xpc_copy_description(reply);
        tsaDebug("DEBUG %s\n", debug);
        free(debug);
         */
#endif

        xpc_object_t xpcTimeStampReply = xpc_dictionary_get_value(reply, "TimeStampReply");
        size_t xpcTSRLength = xpc_data_get_length(xpcTimeStampReply);
        tsaDebug("xpcTSRLength: %ld bytes of response\n", xpcTSRLength);

        xpc_object_t xpcTimeStampError = xpc_dictionary_get_value(reply, "TimeStampError");
        xpc_object_t xpcTimeStampStatus = xpc_dictionary_get_value(reply, "TimeStampStatus");

        if (xpcTimeStampError || xpcTimeStampStatus)
        {
#ifndef NDEBUG
            if (xpcTimeStampError)
            {
                size_t len = xpc_string_get_length(xpcTimeStampError);
                char *buf = (char *)malloc(len);
                strlcpy(buf, xpc_string_get_string_ptr(xpcTimeStampError), len+1);
                tsaDebug("xpcTimeStampError: %s\n", buf);
                if (buf)
                    free(buf);
            }
#endif
            if (xpcTimeStampStatus)
            {
                result = (OSStatus)xpc_int64_get_value(xpcTimeStampStatus);
                tsaDebug("xpcTimeStampStatus: %d\n", (int)result);
            }
        }

        result = remapHTTPErrorCodes(result);

        if ((result == noErr) && tsaResp && tsaRespLength)
        {
            *tsaRespLength = xpcTSRLength;
            *tsaResp = (unsigned char *)malloc(xpcTSRLength);

            size_t bytesCopied = xpc_data_get_bytes(xpcTimeStampReply, *tsaResp, 0, xpcTSRLength);
            if (bytesCopied != xpcTSRLength)
            {    tsaDebug("length mismatch: copied: %ld, xpc: %ld\n", bytesCopied, xpcTSRLength); }
            else
            if ((result = checkForNonDERResponse(*tsaResp,bytesCopied)))
            {
                tsaDebug("received non-DER response from timestamp server\n");
            }
            else
            {
                result = noErr;
                tsaDebug("copied: %ld bytes of response\n", bytesCopied);
            }
        }
        tsaDebug("releasing connection\n");
        xpc_release(con);
    }
    else
        { tsaDebug("unexpected message reply type %p\n", xtype); }

        dispatch_semaphore_signal(waitSemaphore);
        dispatch_release(waitSemaphore);
    });

    { tsaDebug("waiting up to %d seconds for response from XPC\n", timeoutInSeconds); }
	dispatch_semaphore_wait(waitSemaphore, finishTime);

	dispatch_release(waitSemaphore);
    xpc_release(tsaReqData);
    xpc_release(message);

    { tsaDebug("sendTSARequestWithXPC exit\n"); }

    return result;
}

#pragma mark ----- TimeStamp request -----

#include "tsaTemplates.h"
#include <security_asn1/SecAsn1Coder.h>
#include <Security/oidsalg.h>
#include <AssertMacros.h>
#include <libkern/OSByteOrder.h>

extern const SecAsn1Template kSecAsn1TSATimeStampReqTemplate;
extern const SecAsn1Template kSecAsn1TSATimeStampRespTemplateDER;

CFMutableDictionaryRef SecCmsTSAGetDefaultContext(CFErrorRef *error)
{
    // Caller responsible for retain/release
    // <rdar://problem/11077440> Update SecCmsTSAGetDefaultContext with actual URL for Apple Timestamp server
    //      URL will be in TimeStampingPrefs.plist

    CFBundleRef secFWbundle = NULL;
    CFURLRef resourceURL = NULL;
    CFDataRef resourceData = NULL;
    CFPropertyListRef prefs = NULL;
    CFMutableDictionaryRef contextDict = NULL;
    SInt32 errorCode = 0;
    CFOptionFlags options = 0;
    CFPropertyListFormat format = 0;
    OSStatus status = noErr;

    require_action(secFWbundle = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.security")), xit, status = errSecInternalError);
    require_action(resourceURL = CFBundleCopyResourceURL(secFWbundle, CFSTR("TimeStampingPrefs"), CFSTR("plist"), NULL),
        xit, status = errSecInvalidPrefsDomain);

    require(CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault, resourceURL, &resourceData,
        NULL, NULL, &errorCode), xit);
    require_action(resourceData, xit, status = errSecDataNotAvailable);

    prefs = CFPropertyListCreateWithData(kCFAllocatorDefault, resourceData, options, &format, error);
    require_action(prefs && (CFGetTypeID(prefs)==CFDictionaryGetTypeID()), xit, status = errSecInvalidPrefsDomain);

    contextDict = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, prefs);

    if (error)
        *error = NULL;
xit:
    if (errorCode)
        status = errorCode;
    if (error && status)
        *error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus, status, NULL);
    if (secFWbundle)
        CFRelease(secFWbundle);
    if (resourceURL)
        CFRelease(resourceURL);
    if (resourceData)
        CFRelease(resourceData);
    if (prefs)
        CFRelease(prefs);

    return contextDict;
}

static CFDataRef _SecTSARequestCopyDEREncoding(SecAsn1TSAMessageImprint *messageImprint, bool noCerts, uint64_t nonce)
{
    // Returns DER encoded TimeStampReq
    // Modeled on _SecOCSPRequestCopyDEREncoding
    // The Timestamp Authority supports 64 bit nonces (or more possibly)

    SecAsn1CoderRef             coder = NULL;
    uint8_t                     version = 1;
    SecAsn1Item                 vers = {1, &version};
    uint8_t                     creq = noCerts?0:1;
    SecAsn1Item                 certReq = {1, &creq};   //jch - to request or not?
    SecAsn1TSATimeStampReq      tsreq = {};
    CFDataRef                   der = NULL;
    uint64_t                    nonceVal = OSSwapHostToBigConstInt64(nonce);
    SecAsn1Item                 nonceItem = {sizeof(uint64_t), (unsigned char *)&nonceVal};

    uint8_t OID_FakePolicy_Data[] = { 0x2A, 0x03, 0x04, 0x05, 0x06};
    const CSSM_OID fakePolicyOID = {sizeof(OID_FakePolicy_Data),OID_FakePolicy_Data};

    tsreq.version = vers;

    tsreq.messageImprint = *messageImprint;
    tsreq.certReq = certReq;

    // skip reqPolicy, extensions for now - FAKES - jch
    tsreq.reqPolicy = fakePolicyOID;    //policyID;

    tsreq.nonce = nonceItem;

    // Encode the request
    require_noerr(SecAsn1CoderCreate(&coder), errOut);

    SecAsn1Item encoded;
    require_noerr(SecAsn1EncodeItem(coder, &tsreq,
        &kSecAsn1TSATimeStampReqTemplate, &encoded), errOut);
    der = CFDataCreate(kCFAllocatorDefault, encoded.Data,
        encoded.Length);

errOut:
    if (coder)
        SecAsn1CoderRelease(coder);

    return der;
}

OSStatus SecTSAResponseCopyDEREncoding(SecAsn1CoderRef coder, const CSSM_DATA *tsaResponse, SecAsn1TimeStampRespDER *respDER)
{
    // Partially decode the response
    OSStatus status = paramErr;

    require(tsaResponse && respDER, errOut);
    require_noerr(SecAsn1DecodeData(coder, tsaResponse,
        &kSecAsn1TSATimeStampRespTemplateDER, respDER), errOut);
    status = noErr;

errOut:

    return status;
}

#pragma mark ----- TS Callback -----

OSStatus SecCmsTSADefaultCallback(CFTypeRef context, void *messageImprintV, uint64_t nonce, CSSM_DATA *signedDERBlob)
{
    OSStatus result = paramErr;
    const unsigned char *tsaReq = NULL;
    size_t tsaReqLength = 0;
    CFDataRef cfreq = NULL;
    unsigned char *tsaURL = NULL;
    bool noCerts = false;

    if (!context || CFGetTypeID(context)!=CFDictionaryGetTypeID())
        return paramErr;

    SecAsn1TSAMessageImprint *messageImprint = (SecAsn1TSAMessageImprint *)messageImprintV;
    if (!messageImprint || !signedDERBlob)
        return paramErr;

    CFBooleanRef cfnocerts = (CFBooleanRef)CFDictionaryGetValue((CFDictionaryRef)context, kTSAContextKeyNoCerts);
    if (cfnocerts)
    {
        tsaDebug("[TSA] Request noCerts\n");
        noCerts = CFBooleanGetValue(cfnocerts);
    }

    // We must spoof the nonce here, before sending the request.
    // If we tried to alter the reply, then the signature would break instead.
    CFBooleanRef cfBadNonce = (CFBooleanRef)CFDictionaryGetValue((CFDictionaryRef)context, kTSADebugContextKeyBadNonce);
    if (cfBadNonce && CFBooleanGetValue(cfBadNonce))
    {
        tsaDebug("[TSA] Forcing bad TS Request by changing nonce\n");
        nonce++;
    }

    printDataAsHex("[TSA] hashToTimeStamp:", &messageImprint->hashedMessage,128);
    cfreq = _SecTSARequestCopyDEREncoding(messageImprint, noCerts, nonce);
    if (cfreq)
    {
        tsaReq = CFDataGetBytePtr(cfreq);
        tsaReqLength = CFDataGetLength(cfreq);

#ifndef NDEBUG
        CFShow(cfreq);
        tsaWriteFileX("/tmp/tsareq.req", tsaReq, tsaReqLength);
#endif
    }

    CFStringRef url = (CFStringRef)CFDictionaryGetValue((CFDictionaryRef)context, kTSAContextKeyURL);
    if (!url)
    {
        tsaDebug("[TSA] missing URL for TSA (key: %s)\n", "kTSAContextKeyURL");
        goto xit;
    }

    /*
        If debugging, look at special values in the context to mess things up
    */

    CFBooleanRef cfBadReq = (CFBooleanRef)CFDictionaryGetValue((CFDictionaryRef)context, kTSADebugContextKeyBadReq);
    if (cfBadReq && CFBooleanGetValue(cfBadReq))
    {
        tsaDebug("[TSA] Forcing bad TS Request by truncating length from %ld to %ld\n", tsaReqLength, (tsaReqLength-4));
        tsaReqLength -= 4;
    }

    // need to extract into buffer
    CFIndex length = CFStringGetLength(url);        // in 16-bit character units
    tsaURL = malloc(6 * length + 1);                // pessimistic
    if (!CFStringGetCString(url, (char *)tsaURL, 6 * length + 1, kCFStringEncodingUTF8))
        goto xit;

    tsaDebug("[TSA] URL for timestamp server: %s\n", tsaURL);

    unsigned char *tsaResp = NULL;
    size_t tsaRespLength = 0;
    tsaDebug("calling sendTSARequestWithXPC with %ld bytes of request\n", tsaReqLength);

    require_noerr(result = sendTSARequestWithXPC(tsaReq, tsaReqLength, tsaURL, &tsaResp, &tsaRespLength), xit);

    tsaDebug("sendTSARequestWithXPC copied: %ld bytes of response\n", tsaRespLength);

    signedDERBlob->Data = tsaResp;
    signedDERBlob->Length = tsaRespLength;

    result = noErr;

xit:
    if (tsaURL)
        free((void *)tsaURL);
    if (cfreq)
        CFRelease(cfreq);

    return result;
}

#pragma mark ----- TimeStamp Verification -----

static OSStatus convertGeneralizedTimeToCFAbsoluteTime(const char *timeStr, CFAbsoluteTime *ptime)
{
    /*
        See http://userguide.icu-project.org/formatparse/datetime for date/time format.
        The "Z" signal a GMT time, but CFDateFormatterGetAbsoluteTimeFromString returns
        values based on local time.
    */

    OSStatus result = noErr;
    CFDateFormatterRef formatter = NULL;
    CFStringRef time_string = NULL;
    CFTimeZoneRef gmt = NULL;
    CFLocaleRef locale = NULL;
    CFRange *rangep = NULL;

    require(timeStr && timeStr[0] && ptime, xit);
    require(formatter = CFDateFormatterCreate(kCFAllocatorDefault, locale, kCFDateFormatterNoStyle, kCFDateFormatterNoStyle), xit);
//    CFRetain(formatter);
    CFDateFormatterSetFormat(formatter, CFSTR("yyyyMMddHHmmss'Z'"));    // GeneralizedTime
    gmt = CFTimeZoneCreateWithTimeIntervalFromGMT(NULL, 0);
	CFDateFormatterSetProperty(formatter, kCFDateFormatterTimeZone, gmt);

    time_string = CFStringCreateWithCString(kCFAllocatorDefault, timeStr, kCFStringEncodingUTF8);
    if (!time_string || !CFDateFormatterGetAbsoluteTimeFromString(formatter, time_string, rangep, ptime))
    {
        dtprintf("%s is not a valid date\n", timeStr);
        result = 1;
    }

xit:
    if (formatter)
        CFRelease(formatter);
    if (time_string)
        CFRelease(time_string);
    if (gmt)
        CFRelease(gmt);

    return result;
}

static OSStatus SecTSAValidateTimestamp(const SecAsn1TSATSTInfo *tstInfo, CSSM_DATA **signingCerts, CFAbsoluteTime *timestampTime)
{
    // See <rdar://problem/11077708> Properly handle revocation information of timestamping certificate
    OSStatus result = paramErr;
    CFAbsoluteTime genTime = 0;
    char timeStr[32] = {0,};
    SecCertificateRef signingCertificate = NULL;

    require(tstInfo && signingCerts && (tstInfo->genTime.Length < 16), xit);

    // Find the leaf signingCert
    require_noerr(result = SecCertificateCreateFromData(*signingCerts,
        CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_DER, &signingCertificate), xit);

    memcpy(timeStr, tstInfo->genTime.Data, tstInfo->genTime.Length);
    timeStr[tstInfo->genTime.Length] = 0;
    require_noerr(convertGeneralizedTimeToCFAbsoluteTime(timeStr, &genTime), xit);
    if (SecCertificateIsValidX(signingCertificate, genTime)) // iOS?
        result = noErr;
    else
        result = errSecTimestampInvalid;
    if (timestampTime)
        *timestampTime = genTime;
xit:
    return result;
}

static OSStatus verifyTSTInfo(const CSSM_DATA_PTR content, CSSM_DATA **signingCerts, SecAsn1TSATSTInfo *tstInfo, CFAbsoluteTime *timestampTime, uint64_t expectedNonce)
{
    OSStatus status = paramErr;
    SecAsn1CoderRef coder = NULL;

    if (!tstInfo)
        return SECFailure;

    require_noerr(SecAsn1CoderCreate(&coder), xit);
    require_noerr(SecAsn1Decode(coder, content->Data, content->Length,
		kSecAsn1TSATSTInfoTemplate, tstInfo), xit);
    displayTSTInfo(tstInfo);

    // Check the nonce
    if (tstInfo->nonce.Data && expectedNonce!=0)
    {
        uint64_t nonce = tsaDER_ToInt(&tstInfo->nonce);
     //   if (expectedNonce!=nonce)
            dtprintf("verifyTSTInfo nonce: actual: %lld, expected: %lld\n", nonce, expectedNonce);
        require_action(expectedNonce==nonce, xit, status = errSecTimestampRejection);
    }

    status = SecTSAValidateTimestamp(tstInfo, signingCerts, timestampTime);
    dtprintf("SecTSAValidateTimestamp result: %ld\n", (long)status);

xit:
    if (coder)
        SecAsn1CoderRelease(coder);
    return status;
}

static void debugShowExtendedTrustResult(int index, CFDictionaryRef extendedResult)
{
#ifndef NDEBUG
    if (extendedResult)
    {
        CFStringRef xresStr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
        CFSTR("Extended trust result for signer #%d : %@"), index, extendedResult);
        if (xresStr)
        {
            CFShow(xresStr);
            CFRelease(xresStr);
        }
    }
#endif
}

#ifndef NDEBUG
extern const char *cssmErrorString(CSSM_RETURN error);

static void statusBitTest(CSSM_TP_APPLE_CERT_STATUS certStatus, uint32 bit, const char *str)
{
	if (certStatus & bit)
		dtprintf("%s  ", str);
}
#endif

static void debugShowCertEvidenceInfo(uint16_t certCount, const CSSM_TP_APPLE_EVIDENCE_INFO *info)
{
#ifndef NDEBUG
	CSSM_TP_APPLE_CERT_STATUS cs;
//	const CSSM_TP_APPLE_EVIDENCE_INFO *pinfo = info;
    uint16_t ix;
	for (ix=0; info && (ix<certCount); ix++, ++info)
    {
		cs = info->StatusBits;
		dtprintf("   cert %u:\n", ix);
		dtprintf("      StatusBits     : 0x%x", (unsigned)cs);
		if (cs)
        {
			dtprintf(" ( ");
			statusBitTest(cs, CSSM_CERT_STATUS_EXPIRED, "EXPIRED");
			statusBitTest(cs, CSSM_CERT_STATUS_NOT_VALID_YET,
				"NOT_VALID_YET");
			statusBitTest(cs, CSSM_CERT_STATUS_IS_IN_INPUT_CERTS,
				"IS_IN_INPUT_CERTS");
			statusBitTest(cs, CSSM_CERT_STATUS_IS_IN_ANCHORS,
				"IS_IN_ANCHORS");
			statusBitTest(cs, CSSM_CERT_STATUS_IS_ROOT, "IS_ROOT");
			statusBitTest(cs, CSSM_CERT_STATUS_IS_FROM_NET, "IS_FROM_NET");
			dtprintf(")\n");
		}
		else
            dtprintf("\n");

		dtprintf("      NumStatusCodes : %u ", info->NumStatusCodes);
        CSSM_RETURN *pstatuscode = info->StatusCodes;
		uint16_t jx;
        for (jx=0; pstatuscode && (jx<info->NumStatusCodes); jx++, ++pstatuscode)
            dtprintf("%s  ", cssmErrorString(*pstatuscode));

		dtprintf("\n");
		dtprintf("      Index: %u\n", info->Index);
	}

#endif
}

#ifndef NDEBUG
static const char *trustResultTypeString(SecTrustResultType trustResultType)
{
    switch (trustResultType)
    {
    case kSecTrustResultProceed:                    return "TrustResultProceed";
    case kSecTrustResultUnspecified:                return "TrustResultUnspecified";
    case kSecTrustResultDeny:                       return "TrustResultDeny";   // user reject
    case kSecTrustResultInvalid:                    return "TrustResultInvalid";
    case kSecTrustResultConfirm:                    return "TrustResultConfirm";
    case kSecTrustResultRecoverableTrustFailure:    return "TrustResultRecoverableTrustFailure";
    case kSecTrustResultFatalTrustFailure:          return "TrustResultUnspecified";
    case kSecTrustResultOtherError:                 return "TrustResultOtherError";
    default:                                        return "TrustResultUnknown";
    }
    return "";
}
#endif

static OSStatus verifySigners(SecCmsSignedDataRef signedData, int numberOfSigners)
{
    // See <rdar://problem/11077588> Bubble up SecTrustEvaluate of timestamp response to high level callers
    // Also <rdar://problem/11077708> Properly handle revocation information of timestamping certificate

    SecPolicyRef policy = NULL;
    int result=errSecInternalError;
    int rx;

    require(policy = SecPolicyCreateWithOID(kSecPolicyAppleTimeStamping), xit);
    int jx;
    for (jx = 0; jx < numberOfSigners; ++jx)
    {
        SecTrustResultType trustResultType;
        SecTrustRef trustRef = NULL;
        CFDictionaryRef extendedResult = NULL;
        CFArrayRef certChain = NULL;
        uint16_t certCount = 0;

        CSSM_TP_APPLE_EVIDENCE_INFO *statusChain = NULL;

        // SecCmsSignedDataVerifySignerInfo returns trustRef, which we can call SecTrustEvaluate on
        // usually (always?) if result is noErr, the SecTrust*Result calls will return errSecTrustNotAvailable
        result = SecCmsSignedDataVerifySignerInfo (signedData, jx, NULL, policy, &trustRef);
        dtprintf("[%s] SecCmsSignedDataVerifySignerInfo: result: %d, signer: %d\n",
            __FUNCTION__, result, jx);
         require_noerr(result, xit);

        result = SecTrustEvaluate (trustRef, &trustResultType);
        dtprintf("[%s] SecTrustEvaluate: result: %d, trustResult: %s (%d)\n",
            __FUNCTION__, result, trustResultTypeString(trustResultType), trustResultType);
        if (result)
            goto xit;
        switch (trustResultType)
        {
		case kSecTrustResultProceed:
		case kSecTrustResultUnspecified:
			break;                                  // success
		case kSecTrustResultDeny:                   // user reject
			result = errSecTimestampNotTrusted;     // SecCmsVSTimestampNotTrusted ?
            break;
		case kSecTrustResultInvalid:
			assert(false);                          // should never happen
			result = errSecTimestampNotTrusted;     // SecCmsVSTimestampNotTrusted ?
            break;
        case kSecTrustResultConfirm:
        case kSecTrustResultRecoverableTrustFailure:
        case kSecTrustResultFatalTrustFailure:
        case kSecTrustResultOtherError:
		default:
			{
                /*
                    There are two "errors" that need to be resolved externally:
                    CSSMERR_TP_CERT_EXPIRED can be OK if the timestamp was made
                    before the TSA chain expired; CSSMERR_TP_CERT_NOT_VALID_YET
                    can happen in the case where the user's clock was set to 0.
                    We don't want to prevent them using apps automatically, so
                    return noErr and let codesign or whover decide.
                */
				OSStatus resultCode;
				require_action(SecTrustGetCssmResultCode(trustRef, &resultCode)==noErr, xit, result = errSecTimestampNotTrusted);
				result = (resultCode == CSSMERR_TP_CERT_EXPIRED || resultCode == CSSMERR_TP_CERT_NOT_VALID_YET)?noErr:errSecTimestampNotTrusted;
			}
            break;
		}

        rx = SecTrustGetResult(trustRef, &trustResultType, &certChain, &statusChain);
        dtprintf("[%s] SecTrustGetResult: result: %d, type: %d\n", __FUNCTION__,rx, trustResultType);
        certCount = certChain?CFArrayGetCount(certChain):0;
        debugShowCertEvidenceInfo(certCount, statusChain);

        rx = SecTrustCopyExtendedResult(trustRef, &extendedResult);
        dtprintf("[%s] SecTrustCopyExtendedResult: result: %d\n", __FUNCTION__, rx);
        if (extendedResult)
        {
            debugShowExtendedTrustResult(jx, extendedResult);
            CFRelease(extendedResult);
        }

        if (trustRef)
            CFRelease (trustRef);
     }

xit:
    if (policy)
        CFRelease (policy);
    return result;
}

static OSStatus impExpImportCertUnCommon(
	const CSSM_DATA		*cdata,
	SecKeychainRef		importKeychain, // optional
	CFMutableArrayRef	outArray)		// optional, append here
{
    // The only difference between this and impExpImportCertCommon is that we append to outArray
    // before attempting to add to the keychain
	OSStatus status = noErr;
	SecCertificateRef certRef = NULL;

    require_action(cdata, xit, status = errSecUnsupportedFormat);

	/* Pass kCFAllocatorNull as bytesDeallocator to assure the bytes aren't freed */
	CFDataRef data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const UInt8 *)cdata->Data, (CFIndex)cdata->Length, kCFAllocatorNull);
    require_action(data, xit, status = errSecUnsupportedFormat);

	certRef = SecCertificateCreateWithData(kCFAllocatorDefault, data);
	CFRelease(data); /* certRef has its own copy of the data now */
	if(!certRef) {
		dtprintf("impExpHandleCert error\n");
		return errSecUnsupportedFormat;
	}

	if (outArray)
		CFArrayAppendValue(outArray, certRef);

	if (importKeychain)
    {
		status = SecCertificateAddToKeychain(certRef, importKeychain);
		if (status!=noErr && status!=errSecDuplicateItem)
            { dtprintf("SecCertificateAddToKeychain error: %ld\n", (long)status); }
	}

xit:
    if (certRef)
        CFRelease(certRef);
	return status;
}

static void saveTSACertificates(CSSM_DATA **signingCerts, CFMutableArrayRef	outArray)
{
    SecKeychainRef defaultKeychain = NULL;
    // Don't save certificates in keychain to avoid securityd issues
//  if (SecKeychainCopyDefault(&defaultKeychain))
//     defaultKeychain = NULL;

    unsigned certCount = SecCmsArrayCount((void **)signingCerts);
    unsigned dex;
    for (dex=0; dex<certCount; dex++)
    {
        OSStatus rx = impExpImportCertUnCommon(signingCerts[dex], defaultKeychain, outArray);
        if (rx!=noErr && rx!=errSecDuplicateItem)
            dtprintf("impExpImportCertCommon failed: %ld\n", (long)rx);
    }
    if (defaultKeychain)
        CFRelease(defaultKeychain);
}

static const char *cfabsoluteTimeToString(CFAbsoluteTime abstime)
{
    CFGregorianDate greg = CFAbsoluteTimeGetGregorianDate(abstime, NULL);
    char str[20];
    if (19 != snprintf(str, 20, "%4.4d-%2.2d-%2.2d_%2.2d:%2.2d:%2.2d",
        (int)greg.year, greg.month, greg.day, greg.hour, greg.minute, (int)greg.second))
        str[0]=0;
    char *data = (char *)malloc(20);
    strncpy(data, str, 20);
    return data;
}

static OSStatus setTSALeafValidityDates(SecCmsSignerInfoRef signerinfo)
{
    OSStatus status = noErr;

    if (!signerinfo->timestampCertList || (CFArrayGetCount(signerinfo->timestampCertList) == 0))
        return SecCmsVSSigningCertNotFound;

    SecCertificateRef tsaLeaf = (SecCertificateRef)CFArrayGetValueAtIndex(signerinfo->timestampCertList, 0);
    require_action(tsaLeaf, xit, status = errSecCertificateCannotOperate);

    signerinfo->tsaLeafNotBefore = SecCertificateNotValidBefore(tsaLeaf); /* Start date for Timestamp Authority leaf */
    signerinfo->tsaLeafNotAfter = SecCertificateNotValidAfter(tsaLeaf);   /* Expiration date for Timestamp Authority leaf */

    const char *nbefore = cfabsoluteTimeToString(signerinfo->tsaLeafNotBefore);
    const char *nafter = cfabsoluteTimeToString(signerinfo->tsaLeafNotAfter);
    if (nbefore && nafter)
    {
        dtprintf("Timestamp Authority leaf valid from %s to %s\n", nbefore, nafter);
        free((void *)nbefore);free((void *)nafter);
    }

/*
		if(at < nb)
			status = errSecCertificateNotValidYet;
		else if (at > na)
			status = errSecCertificateExpired;
*/

xit:
    return status;
}

/*
    From RFC 3161: Time-Stamp Protocol (TSP),August 2001, APPENDIX B:

    B) The validity of the digital signature may then be verified in the
        following way:

        1)  The time-stamp token itself MUST be verified and it MUST be
            verified that it applies to the signature of the signer.

        2)  The date/time indicated by the TSA in the TimeStampToken
            MUST be retrieved.

        3)  The certificate used by the signer MUST be identified and
            retrieved.

        4)  The date/time indicated by the TSA MUST be within the
            validity period of the signer's certificate.

        5)  The revocation information about that certificate, at the
            date/time of the Time-Stamping operation, MUST be retrieved.

        6)  Should the certificate be revoked, then the date/time of
            revocation shall be later than the date/time indicated by
            the TSA.

    If all these conditions are successful, then the digital signature
    shall be declared as valid.

*/

OSStatus decodeTimeStampToken(SecCmsSignerInfoRef signerinfo, CSSM_DATA_PTR inData, CSSM_DATA_PTR encDigest, uint64_t expectedNonce)
{
    /*
        We update signerinfo with timestamp and tsa certificate chain.
        encDigest is the original signed blob, which we must hash and compare.
        inData comes from the unAuthAttr section of the CMS message

        These are set in signerinfo as side effects:
            timestampTime -
            timestampCertList
    */

    SecCmsDecoderRef        decoderContext = NULL;
    SecCmsMessageRef        cmsMessage = NULL;
    SecCmsContentInfoRef    contentInfo;
    SecCmsSignedDataRef     signedData;
    SECOidTag               contentTypeTag;
    int                     contentLevelCount;
    int                     ix;
    OSStatus                result = errSecUnknownFormat;
    CSSM_DATA               **signingCerts = NULL;

    dtprintf("decodeTimeStampToken top: PORT_GetError() %d -----\n", PORT_GetError());
    PORT_SetError(0);

    /* decode the message */
    require_noerr(result = SecCmsDecoderCreate (NULL, NULL, NULL, NULL, NULL, NULL, NULL, &decoderContext), xit);
    result = SecCmsDecoderUpdate(decoderContext, inData->Data, inData->Length);
	if (result)
    {
        result = errSecTimestampInvalid;
        SecCmsDecoderDestroy(decoderContext);
        goto xit;
	}

    require_noerr(result = SecCmsDecoderFinish(decoderContext, &cmsMessage), xit);

    // process the results
    contentLevelCount = SecCmsMessageContentLevelCount(cmsMessage);

    if (encDigest)
        printDataAsHex("encDigest",encDigest, 0);

    for (ix = 0; ix < contentLevelCount; ++ix)
    {
        dtprintf("\n----- Content Level %d -----\n", ix);
        // get content information
        contentInfo = SecCmsMessageContentLevel (cmsMessage, ix);
        contentTypeTag = SecCmsContentInfoGetContentTypeTag (contentInfo);

        // After 2nd round, contentInfo.content.data is the TSTInfo

        debugShowContentTypeOID(contentInfo);

        switch (contentTypeTag)
        {
        case SEC_OID_PKCS7_SIGNED_DATA:
        {
            require((signedData = (SecCmsSignedDataRef)SecCmsContentInfoGetContent(contentInfo)) != NULL, xit);

            debugShowSignerInfo(signedData);

            SECAlgorithmID **digestAlgorithms = SecCmsSignedDataGetDigestAlgs(signedData);
            unsigned digestAlgCount = SecCmsArrayCount((void **)digestAlgorithms);
            dtprintf("digestAlgCount: %d\n", digestAlgCount);
            if (signedData->digests)
            {
                int jx;
                char buffer[128];
                for (jx=0;jx < digestAlgCount;jx++)
                {
                    sprintf(buffer, " digest[%u]", jx);
                    printDataAsHex(buffer,signedData->digests[jx], 0);
                }
            }
            else
            {
                dtprintf("No digests\n");
                CSSM_DATA_PTR innerContent = SecCmsContentInfoGetInnerContent(contentInfo);
                if (innerContent)
                {
                    dtprintf("inner content length: %ld\n", innerContent->Length);
                    SecAsn1TSAMessageImprint fakeMessageImprint = {{{0}},};
                    OSStatus status = createTSAMessageImprint(signedData, innerContent, &fakeMessageImprint);
                    if (status)
                        {    dtprintf("createTSAMessageImprint status: %d\n", (int)status); }
                    printDataAsHex("inner content hash",&fakeMessageImprint.hashedMessage, 0);
                    CSSM_DATA_PTR digestdata = &fakeMessageImprint.hashedMessage;
                    CSSM_DATA_PTR digests[2] = {digestdata, NULL};
                    SecCmsSignedDataSetDigests(signedData, digestAlgorithms, (CSSM_DATA_PTR *)&digests);
                }
                else
                    dtprintf("no inner content\n");
            }

            /*
                Import the certificates. We leave this as a warning, since
                there are configurations where the certificates are not returned.
            */
            signingCerts = SecCmsSignedDataGetCertificateList(signedData);
            if (signingCerts == NULL)
            {    dtprintf("SecCmsSignedDataGetCertificateList returned NULL\n"); }
            else
            {
                if (!signerinfo->timestampCertList)
                    signerinfo->timestampCertList = CFArrayCreateMutable(kCFAllocatorDefault, 10, &kCFTypeArrayCallBacks);
                saveTSACertificates(signingCerts, signerinfo->timestampCertList);
                require_noerr(result = setTSALeafValidityDates(signerinfo), xit);
                debugSaveCertificates(signingCerts);
            }

            int numberOfSigners = SecCmsSignedDataSignerInfoCount (signedData);

            result = verifySigners(signedData, numberOfSigners);
            if (result)
                dtprintf("verifySigners failed: %ld\n", (long)result);   // warning


            if (result)     // remap to SecCmsVSTimestampNotTrusted ?
                goto xit;

            break;
        }
        case SEC_OID_PKCS9_SIGNING_CERTIFICATE:
        {
            dtprintf("SEC_OID_PKCS9_SIGNING_CERTIFICATE seen\n");
            break;
        }

        case SEC_OID_PKCS9_ID_CT_TSTInfo:
        {
            SecAsn1TSATSTInfo tstInfo = {{0},};
            result = verifyTSTInfo(contentInfo->rawContent, signingCerts, &tstInfo, &signerinfo->timestampTime, expectedNonce);
            if (signerinfo->timestampTime)
            {
                const char *tstamp = cfabsoluteTimeToString(signerinfo->timestampTime);
                if (tstamp)
                {
                    dtprintf("Timestamp Authority timestamp: %s\n", tstamp);
                    free((void *)tstamp);
                }
            }
            break;
        }
        case SEC_OID_OTHER:
        {
            dtprintf("otherContent : %p\n", (char *)SecCmsContentInfoGetContent (contentInfo));
            break;
        }
        default:
            dtprintf("ContentTypeTag : %x\n", contentTypeTag);
            break;
        }
    }
xit:
	if (cmsMessage)
		SecCmsMessageDestroy(cmsMessage);

    return result;
}


