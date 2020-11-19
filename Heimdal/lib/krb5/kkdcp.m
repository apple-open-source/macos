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
#include <Foundation/Foundation.h>

static krb5_error_code
requestToURL(krb5_context context,
	     const char *stringurl,
	     const krb5_data *outdata,
	     krb5_data *retdata)
{
    KDC_PROXY_MESSAGE msg;
    __block krb5_error_code ret;
    size_t size;
    
    @autoreleasepool {
	__block NSData *responseBytes = nil;
	NSMutableURLRequest *request = nil;
	NSURLSessionDataTask *task = nil;
	
	NSString *urlString = [NSString stringWithUTF8String:stringurl];
	_krb5_debugx(context, 5, "kkdcp request to url: %s", [urlString UTF8String]);
	
	NSURL *url = [NSURL URLWithString:urlString];
	if (url==nil)
	{
	    ret = ENOMEM;
	    goto out;
	}
	NSData *bodyData = [[NSData alloc] initWithBytesNoCopy:outdata->data length:outdata->length];
		
	request = [NSMutableURLRequest requestWithURL:url];
	[request setHTTPMethod:@"POST"];
	[request setHTTPBody:bodyData];
	[request addValue:@"application/octet-stream" forHTTPHeaderField:@"Content-Type"];
	[request addValue:@(PACKAGE_STRING) forHTTPHeaderField:@"X-Kerberos-Client"];
	
	dispatch_semaphore_t sem = dispatch_semaphore_create(0);
	NSURLSession *session = [NSURLSession sessionWithConfiguration:NSURLSessionConfiguration.ephemeralSessionConfiguration];
	task = [session dataTaskWithRequest:request completionHandler:^(NSData * _Nullable data, NSURLResponse * _Nullable response, NSError * _Nullable error) {
		if (error) {
		_krb5_debugx(context, 5, "kkdcp response error: %s", [[error localizedDescription] UTF8String]);
		ret = HEIM_ERR_EOF;
		_krb5_set_cf_error_message(context, ret, (__bridge CFErrorRef) error, "Failure during kkdcp stream");
		}
		if (data) {
		responseBytes = [data copy];
		_krb5_debugx(context, 5, "kkdcp response received: %lu", (unsigned long)data.length);
		}
		dispatch_semaphore_signal(sem);
		}];
	[task resume];
	dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
	
	ret = decode_KDC_PROXY_MESSAGE(responseBytes.bytes, responseBytes.length, &msg, &size);
	if (ret) {
	    krb5_set_error_message(context, ret, "failed to decode KDC_PROXY_MESSAGE");
	    goto out;
	}
    }
    
    ret = krb5_data_copy(retdata, msg.kerb_message.data, msg.kerb_message.length);
    free_KDC_PROXY_MESSAGE(&msg);
    if (ret)
	goto out;
    
    ret = 0;
 out:
    if (ret)
	_krb5_debug(context, 10, ret, "kkdcp to url (%s) failed", stringurl);

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
