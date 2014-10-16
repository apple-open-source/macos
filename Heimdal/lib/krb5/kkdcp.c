/*
 * Copyright (c) 2013 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "krb5_locl.h"
#include <CoreFoundation/CoreFoundation.h>
#include <CFNetwork/CFNetwork.h>

static krb5_error_code
requestToURL(krb5_context context,
	     const char *stringurl,
	     const krb5_data *outdata,
	     krb5_data *retdata)
{
    CFMutableDataRef responseBytes = NULL;
    CFReadStreamRef requestStream = NULL;
    CFHTTPMessageRef message = NULL;
    CFDataRef bodyData = NULL;
    KDC_PROXY_MESSAGE msg;
    CFIndex numBytesRead;
    krb5_error_code ret;
    CFURLRef url = NULL;
    size_t size;
    
    bodyData = CFDataCreateWithBytesNoCopy(NULL, outdata->data, outdata->length, kCFAllocatorNull);
    if (bodyData == NULL) {
	ret = ENOMEM;
	goto out;
    }
 
    url = CFURLCreateWithBytes(NULL, (const UInt8 *)stringurl, strlen(stringurl), kCFStringEncodingUTF8, NULL);
    if (url == NULL) {
	ret = ENOMEM;
	goto out;
    }

    message = CFHTTPMessageCreateRequest(kCFAllocatorDefault, CFSTR("POST"), url, kCFHTTPVersion1_1);
    if (message == NULL) {
	ret = ENOMEM;
	goto out;
    }
    CFHTTPMessageSetBody(message, bodyData);
    CFHTTPMessageSetHeaderFieldValue(message, CFSTR("Content-Type"), CFSTR("application/octet-stream"));

    requestStream = CFReadStreamCreateForHTTPRequest(NULL, message);
    if (requestStream == NULL) {
	ret = ENOMEM;
	goto out;
    }

    if (!CFReadStreamOpen(requestStream)) {
	CFErrorRef error = CFReadStreamCopyError(requestStream);
	ret = HEIM_ERR_EOF;
	_krb5_set_cf_error_message(context, ret, error, "Failed to open kkdcp stream");
	if (error)
	    CFRelease(error);
	goto out;
    }

    responseBytes = CFDataCreateMutable(NULL, 0);
    do {
	UInt8 buf[1024];
	numBytesRead = CFReadStreamRead(requestStream, buf, sizeof(buf));
	if(numBytesRead > 0)
	    CFDataAppendBytes(responseBytes, buf, numBytesRead);

    } while(numBytesRead > 0);
    if (numBytesRead < 0) {
	CFErrorRef error = CFReadStreamCopyError(requestStream);
	ret = HEIM_ERR_EOF;
	_krb5_set_cf_error_message(context, ret, error, "Failed to reading kkdcp stream");
	if (error)
	    CFRelease(error);
	goto out;
    }
    CFReadStreamClose(requestStream);

    ret = decode_KDC_PROXY_MESSAGE(CFDataGetBytePtr(responseBytes), CFDataGetLength(responseBytes), &msg, &size);
    if (ret) {
	krb5_set_error_message(context, ret, "failed to decode KDC_PROXY_MESSAGE");
	goto out;
    }
    
    ret = krb5_data_copy(retdata, msg.kerb_message.data, msg.kerb_message.length);
    free_KDC_PROXY_MESSAGE(&msg);
    if (ret)
	goto out;
    
    ret = 0;
 out:
    if (ret)
	_krb5_debug(context, 10, ret, "kkdcp to url (%s) failed", stringurl);

    if (bodyData)
	CFRelease(bodyData);
    if (url)
	CFRelease(url);
    if (message)
	CFRelease(message);
    if (requestStream)
	CFRelease(requestStream);
    if (responseBytes)
	CFRelease(responseBytes);

    return ret;
}

krb5_error_code
_krb5_kkdcp_request(krb5_context context,
		    const char *realm,
		    const char *url,
		    const krb5_data *data,
		    krb5_data *retdata)
{
    KDC_PROXY_MESSAGE msg;
    krb5_data msgdata;
    krb5_error_code ret;
    size_t size = 0;

    memset(&msg, 0, sizeof(msg));
    
    msg.kerb_message = *data;
    msg.target_domain = (Realm *)&realm;
    msg.dclocator_hint = NULL;
	
    ASN1_MALLOC_ENCODE(KDC_PROXY_MESSAGE, msgdata.data, msgdata.length, &msg, &size, ret);
    if (ret)
	return ret;
    heim_assert(msgdata.length == size, "internal asn1. encoder error");
	
    ret = requestToURL(context, url, &msgdata, retdata);
    krb5_data_free(&msgdata);
	
    return ret;
}
