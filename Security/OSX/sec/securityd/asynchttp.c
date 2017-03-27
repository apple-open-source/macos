/*
 * Copyright (c) 2009-2010,2012-2015 Apple Inc. All Rights Reserved.
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

/*
 * asynchttp.c - asynchronous http get/post engine.
 */

#include "asynchttp.h"

#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFStream.h>
#include <CFNetwork/CFProxySupport.h>     /* CFNetworkCopySystemProxySettings */
#include <CFNetwork/CFSocketStreamPriv.h> /* kCFStreamPropertySourceApplication */
#include <Security/SecInternal.h>
#include "SecBase64.h"
#include <AssertMacros.h>
#include <utilities/debugging.h>
#include <utilities/SecDispatchRelease.h>
#include <asl.h>
#include <string.h>

#include <inttypes.h>

#if __LP64__
#define PRIstatus "d"
#else
#define PRIstatus "ld"
#endif

/* POST method has Content-Type header line equal to
   "application/ocsp-request" */
static CFStringRef kContentType		= CFSTR("Content-Type");
static CFStringRef kAppOcspRequest	= CFSTR("application/ocsp-request");

/* SPI to specify timeout on CFReadStream */
#define _kCFStreamPropertyReadTimeout   CFSTR("_kCFStreamPropertyReadTimeout")
#define _kCFStreamPropertyWriteTimeout   CFSTR("_kCFStreamPropertyWriteTimeout")

/* The timeout we set - 7 seconds */
#define STREAM_TIMEOUT		(7 * NSEC_PER_SEC)

#define POST_BUFSIZE   2048

/* There has got to be an easier way to do this.  For now we based this code
   on CFNetwork/Connection/URLResponse.cpp. */
static CFStringRef copyParseMaxAge(CFStringRef cacheControlHeader) {
    /* The format of the cache control header is a comma-separated list, but
       each list element could be a key-value pair, with the value quoted and
       possibly containing a comma. */
    CFStringInlineBuffer inlineBuf = {};
    CFRange componentRange;
    CFIndex length = CFStringGetLength(cacheControlHeader);
    bool done = false;
    CFCharacterSetRef whitespaceSet = CFCharacterSetGetPredefined(kCFCharacterSetWhitespace);
    CFStringRef maxAgeValue = NULL;

    CFStringInitInlineBuffer(cacheControlHeader, &inlineBuf, CFRangeMake(0, length));
    componentRange.location = 0;

    while (!done) {
        bool inQuotes = false;
        bool foundComponentStart = false;
        CFIndex charIndex = componentRange.location;
        CFIndex componentEnd = -1;
        CFRange maxAgeRg;
        componentRange.length = 0;

        while (charIndex < length) {
            UniChar ch = CFStringGetCharacterFromInlineBuffer(&inlineBuf, charIndex);
            if (!inQuotes && ch == ',') {
                componentRange.length = charIndex - componentRange.location;
                break;
            }
            if (!CFCharacterSetIsCharacterMember(whitespaceSet, ch)) {
                if (!foundComponentStart) {
                    foundComponentStart = true;
                    componentRange.location = charIndex;
                } else {
                    componentEnd = charIndex;
                }
                if (ch == '\"') {
                    inQuotes = (inQuotes == false);
                }
            }
            charIndex ++;
        }

        if (componentEnd == -1) {
            componentRange.length = charIndex - componentRange.location;
        } else {
            componentRange.length = componentEnd - componentRange.location + 1;
        }

        if (charIndex == length) {
            /* Fell off the end; this is the last component. */
            done = true;
        }

        /* componentRange should now contain the range of the current
           component; trimmed of any whitespace. */

        /* We want to look for a max-age value. */
        if (!maxAgeValue && CFStringFindWithOptions(cacheControlHeader, CFSTR("max-age"), componentRange, kCFCompareCaseInsensitive | kCFCompareAnchored, &maxAgeRg)) {
            CFIndex equalIdx;
            CFIndex maxCompRg = componentRange.location + componentRange.length;
            for (equalIdx = maxAgeRg.location + maxAgeRg.length; equalIdx < maxCompRg; equalIdx ++) {
                UniChar equalCh = CFStringGetCharacterFromInlineBuffer(&inlineBuf, equalIdx);
                if (equalCh == '=') {
                    // Parse out max-age value
                    equalIdx ++;
                    while (equalIdx < maxCompRg && CFCharacterSetIsCharacterMember(whitespaceSet, CFStringGetCharacterAtIndex(cacheControlHeader, equalIdx))) {
                        equalIdx ++;
                    }
                    if (equalIdx < maxCompRg) {
                        CFReleaseNull(maxAgeValue);
                        maxAgeValue = CFStringCreateWithSubstring(kCFAllocatorDefault, cacheControlHeader, CFRangeMake(equalIdx, maxCompRg-equalIdx));
                    }
                } else if (!CFCharacterSetIsCharacterMember(whitespaceSet, equalCh)) {
                    // Not a valid max-age header; break out doing nothing
                    break;
                }
            }
        }

        if (!done && maxAgeValue) {
            done = true;
        }
        if (!done) {
            /* Advance to the next component; + 1 to get past the comma. */
            componentRange.location = charIndex + 1;
        }
    }

    return maxAgeValue;
}

static void asynchttp_complete(asynchttp_t *http) {
    secdebug("http", "http: %p", http);
    /* Shutdown streams and timer, we're about to invoke our client callback. */
    if (http->stream) {
        CFReadStreamSetClient(http->stream, kCFStreamEventNone, NULL, NULL);
        CFReadStreamSetDispatchQueue(http->stream, NULL);
        CFReadStreamClose(http->stream);
        CFReleaseNull(http->stream);
    }
    if (http->timer) {
        dispatch_source_cancel(http->timer);
        dispatch_release_null(http->timer);
    }

    if (http->completed) {
        /* This should probably move to our clients. */
        CFTimeInterval maxAge = NULL_TIME;
        if (http->response) {
            CFStringRef cacheControl = CFHTTPMessageCopyHeaderFieldValue(
                http->response, CFSTR("cache-control"));
            if (cacheControl) {
                CFStringRef maxAgeValue = copyParseMaxAge(cacheControl);
                CFRelease(cacheControl);
                if (maxAgeValue) {
                    secdebug("http", "http header max-age: %@", maxAgeValue);
                    maxAge = CFStringGetDoubleValue(maxAgeValue);
                    CFRelease(maxAgeValue);
                }
            }
        }
        http->completed(http, maxAge);
    }
}

static void handle_server_response(CFReadStreamRef stream,
    CFStreamEventType type, void *info) {
    asynchttp_t *http = (asynchttp_t *)info;
    if (!http->stream) {
        secerror("Avoiding crash due to CFReadStream invoking us after we called CFReadStreamSetDispatchQueue(stream, NULL) on a different block on our serial queue");
        return;
    }

    switch (type) {
    case kCFStreamEventHasBytesAvailable:
    {
        UInt8 buffer[POST_BUFSIZE];
        CFIndex length;
        do {
#if 1
            length = CFReadStreamRead(stream, buffer, sizeof(buffer));
#else
            const UInt8 *buffer = CFReadStreamGetBuffer(stream, -1, &length);
#endif
            secdebug("http",
                "stream: %@ kCFStreamEventHasBytesAvailable read: %lu bytes",
                stream, length);
            if (length < 0) {
                /* Negative length == error */
                asynchttp_complete(http);
                break;
            } else if (length > 0) {
                //CFHTTPMessageAppendBytes(http->response, buffer, length);
                CFDataAppendBytes(http->data, buffer, length);
            } else {
                /* Read 0 bytes. This is a no-op, but we need to keep
                   reading until CFReadStreamHasBytesAvailable is false.
                 */
            }
        } while (CFReadStreamHasBytesAvailable(stream));
        break;
    }
    case kCFStreamEventErrorOccurred:
    {
        CFStreamError error = CFReadStreamGetError(stream);

        secdebug("http",
            "stream: %@ kCFStreamEventErrorOccurred domain: %ld error: %ld",
            stream, error.domain, (long) error.error);

        if (error.domain == kCFStreamErrorDomainPOSIX) {
            secerror("CFReadStream posix: %s", strerror(error.error));
        } else if (error.domain == kCFStreamErrorDomainMacOSStatus) {
            secerror("CFReadStream osstatus: %"PRIstatus, error.error);
        } else {
            secerror("CFReadStream domain: %ld error: %"PRIstatus,
                error.domain, error.error);
        }
        asynchttp_complete(http);
        break;
    }
    case kCFStreamEventEndEncountered:
    {
        http->response = (CFHTTPMessageRef)CFReadStreamCopyProperty(
            stream, kCFStreamPropertyHTTPResponseHeader);
        secdebug("http", "stream: %@ kCFStreamEventEndEncountered hdr: %@",
            stream, http->response);
        CFHTTPMessageSetBody(http->response, http->data);
        asynchttp_complete(http);
        break;
    }
    default:
        secerror("handle_server_response unexpected event type: %lu",
            type);
        break;
    }
}

/* Create a URI suitable for use in an http GET request, will return NULL if
   the length would exceed 255 bytes. */
static CFURLRef createGetURL(CFURLRef responder, CFDataRef request) {
    CFURLRef getURL = NULL;
    CFMutableDataRef base64Request = NULL;
    CFStringRef base64RequestString = NULL;
    CFStringRef peRequest = NULL;
    CFIndex base64Len;

    base64Len = SecBase64Encode(NULL, CFDataGetLength(request), NULL, 0);
    /* Don't bother doing all the work below if we know the end result will
       exceed 255 bytes (minus one for the '/' separator makes 254). */
    if (base64Len + CFURLGetBytes(responder, NULL, 0) > 254)
        return NULL;

    require(base64Request = CFDataCreateMutable(kCFAllocatorDefault,
        base64Len), errOut);
    CFDataSetLength(base64Request, base64Len);
    SecBase64Encode(CFDataGetBytePtr(request), CFDataGetLength(request),
        (char *)CFDataGetMutableBytePtr(base64Request), base64Len);
    require(base64RequestString = CFStringCreateWithBytes(kCFAllocatorDefault,
        CFDataGetBytePtr(base64Request), base64Len, kCFStringEncodingUTF8,
        false), errOut);
    /* percent-encode all reserved characters from RFC 3986 [2.2] */
    require(peRequest = CFURLCreateStringByAddingPercentEscapes(
        kCFAllocatorDefault, base64RequestString, NULL,
        CFSTR(":/?#[]@!$&'()*+,;="), kCFStringEncodingUTF8), errOut);
#if 1
    CFStringRef urlString = CFURLGetString(responder);
    CFStringRef fullURL;
    if (CFStringHasSuffix(urlString, CFSTR("/"))) {
        fullURL = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
            CFSTR("%@%@"), urlString, peRequest);
    } else {
        fullURL = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
            CFSTR("%@/%@"), urlString, peRequest);
    }
    getURL = CFURLCreateWithString(kCFAllocatorDefault, fullURL, NULL);
    CFRelease(fullURL);
#else
    getURL = CFURLCreateWithString(kCFAllocatorDefault, peRequest, responder);
#endif

errOut:
    CFReleaseSafe(base64Request);
    CFReleaseSafe(base64RequestString);
    CFReleaseSafe(peRequest);

    return getURL;
}

bool asyncHttpPost(CFURLRef responder, CFDataRef requestData /* , bool force_nocache */ ,
    uint64_t timeout, asynchttp_t *http) {
    bool result = true; /* True, we didn't schedule any work. */
	/* resources to release on exit */
    CFURLRef getURL = NULL;

/* Interesting tidbit from rfc5019
   When sending requests that are less than or equal to 255 bytes in
   total (after encoding) including the scheme and delimiters (http://),
   server name and base64-encoded OCSPRequest structure, clients MUST
   use the GET method (to enable OCSP response caching).  OCSP requests
   larger than 255 bytes SHOULD be submitted using the POST method.

   Interesting tidbit from rfc2616:
   Note: Servers ought to be cautious about depending on URI lengths
   above 255 bytes, because some older client or proxy
   implementations might not properly support these lengths.

   Given the second note I'm assuming that the note in rfc5019 is about the
   length of the URI, not the length of the entire HTTP request.

   If we need to consider the entire request we need to have 17 bytes less, or
   17 + 25 = 42 if we are appending a "Cache-Control: no-cache CRLF" header
   field.

   The 17 and 42 above are based on the request encoding from rfc2616
   Method SP Request-URI SP HTTP-Version CRLF (header CRLF)* CRLF
   so in our case it's:
   GET SP URI SP HTTP/1.1 CRLF CRLF
   17 + len(URI) bytes
   or
   GET SP URI SP HTTP/1.1 CRLF Cache-Control: SP no-cache CRLF CRLF
   42 + len(URI) bytes
 */

    /* First let's try creating a GET request. */
    getURL = createGetURL(responder, requestData);
    if (getURL && CFURLGetBytes(getURL, NULL, 0) < 256) {
        /* Get URI is less than 256 bytes encoded, making it safe even for
           older proxy or caching servers, so let's use HTTP GET. */
        secdebug("http", "GET[%ld] %@", CFURLGetBytes(getURL, NULL, 0), getURL);
        require_quiet(http->request = CFHTTPMessageCreateRequest(kCFAllocatorDefault,
            CFSTR("GET"), getURL, kCFHTTPVersion1_1), errOut);
    } else {
        /* GET Request too big to ensure error free transmission, let's
           create a HTTP POST http->request instead. */
        secdebug("http", "POST %@ CRLF body", responder);
        require_quiet(http->request = CFHTTPMessageCreateRequest(kCFAllocatorDefault,
            CFSTR("POST"), responder, kCFHTTPVersion1_1), errOut);
        /* Set the body and required header fields. */
        CFHTTPMessageSetBody(http->request, requestData);
        CFHTTPMessageSetHeaderFieldValue(http->request, kContentType,
            kAppOcspRequest);
    }

#if 0
    if (force_nocache) {
        CFHTTPMessageSetHeaderFieldValue(http->request, CFSTR("Cache-Control"),
            CFSTR("no-cache"));
    }
#endif

    result = asynchttp_request(NULL, timeout, http);

errOut:
    CFReleaseSafe(getURL);

    return result;
}


static void asynchttp_timer_proc(asynchttp_t *http CF_CONSUMED) {
    CFStringRef req_meth = http->request ? CFHTTPMessageCopyRequestMethod(http->request) : NULL;
    CFURLRef req_url = http->request ? CFHTTPMessageCopyRequestURL(http->request) : NULL;
    secnotice("http", "Timeout during %@ %@.", req_meth, req_url);
    CFReleaseSafe(req_url);
    CFReleaseSafe(req_meth);
    asynchttp_complete(http);
}


void asynchttp_free(asynchttp_t *http) {
    if (http) {
        CFReleaseNull(http->token);
        CFReleaseNull(http->request);
        CFReleaseNull(http->response);
        CFReleaseNull(http->data);
        CFReleaseNull(http->stream);
        dispatch_release_null(http->timer);
    }
}

/* Return true, iff we didn't schedule any work, return false if we did. */
bool asynchttp_request(CFHTTPMessageRef request, uint64_t timeout, asynchttp_t *http) {
    secdebug("http", "request %@", request);
    if (request) {
        http->request = request;
        CFRetain(request);
    }

    /* Create the stream for the request. */
    require_quiet(http->stream = CFReadStreamCreateForHTTPRequest(
        kCFAllocatorDefault, http->request), errOut);

    /* Set a reasonable timeout */
    require_quiet(http->timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, http->queue), errOut);
    dispatch_source_set_event_handler(http->timer, ^{
        asynchttp_timer_proc(http);
    });
    // Set the timer's fire time to now + STREAM_TIMEOUT seconds with a .5 second fuzz factor.
    uint64_t stream_timeout = timeout;
    if (timeout == 0) {
        stream_timeout = STREAM_TIMEOUT;
    }
    dispatch_source_set_timer(http->timer, dispatch_time(DISPATCH_TIME_NOW, stream_timeout),
                              DISPATCH_TIME_FOREVER, (int64_t)(500 * NSEC_PER_MSEC));
    dispatch_resume(http->timer);

    /* Set up possible proxy info */
    CFDictionaryRef proxyDict = CFNetworkCopySystemProxySettings();
    if (proxyDict) {
        CFReadStreamSetProperty(http->stream, kCFStreamPropertyHTTPProxy, proxyDict);
        CFRelease(proxyDict);
    }

    /* Set source application property info */
    if (http->token) {
        CFReadStreamSetProperty(http->stream, kCFStreamPropertySourceApplication, http->token);
    }

    http->data = CFDataCreateMutable(kCFAllocatorDefault, 0);

    CFStreamClientContext stream_context = { .info = http };
    CFReadStreamSetClient(http->stream,
        (kCFStreamEventHasBytesAvailable
         | kCFStreamEventErrorOccurred
         | kCFStreamEventEndEncountered),
        handle_server_response, &stream_context);
    CFReadStreamSetDispatchQueue(http->stream, http->queue);
    CFReadStreamOpen(http->stream);

    return false; /* false -> something was scheduled. */

errOut:
    /* Deschedule timer and free anything we might have retained so far. */
    asynchttp_free(http);
    return true;
}
