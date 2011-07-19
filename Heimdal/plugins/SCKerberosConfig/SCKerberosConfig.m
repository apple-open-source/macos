/*
 * Copyright (c) 2006, 2010 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2010 Apple Inc. All rights reserved.
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

#import <err.h>
#import <stdio.h>
#import <arpa/inet.h>
#import <netdb.h>
#import <sys/param.h>
#import <sys/socket.h>

#import <Foundation/Foundation.h>
#import <SystemConfiguration/SystemConfiguration.h>
#import <Heimdal/HeimdalSystemConfiguration.h>
#import <krb5.h>
#import "config_plugin.h"

/**
 * Configuration plugin uses configuration in SC for Kerberos
 */

struct config_ctx {
    SCDynamicStoreRef store;
};


static krb5_error_code
get_default_realm(krb5_context context, void *ptr, void *userctx,
		  void (*add_realms)(krb5_context, void *, krb5_const_realm))
{
    krb5_error_code ret = KRB5_PLUGIN_NO_HANDLE;
    struct config_ctx *ctx = ptr;
    NSAutoreleasePool *pool;
    NSArray *vals = NULL;
    
    @try {
	pool = [[NSAutoreleasePool alloc] init];

	vals = (NSArray *)SCDynamicStoreCopyValue(ctx->store, HEIMDAL_SC_DEFAULT_REALM);
	if (vals == NULL)
	    goto out;

	for (NSString *a in vals)
	    add_realms(context, userctx, [a UTF8String]);

	ret = 0;
    out:
	do { } while(0);
    }
    @catch (NSException *exception) { }
    @finally {

	if (vals)
	    CFRelease((CFTypeRef)vals);
	[pool drain];
    }

    return ret;
}

static krb5_error_code
get_host_domain(krb5_context context, const char *hostname, void *ptr, void *userptr,
		void (*add_realms)(krb5_context, void *, krb5_const_realm))
{
    krb5_error_code ret = KRB5_PLUGIN_NO_HANDLE;
    struct config_ctx *ctx = ptr;
    NSAutoreleasePool *pool;
    NSArray *vals = NULL;
    
    @try {
	pool = [[NSAutoreleasePool alloc] init];
	NSMutableArray *res = [NSMutableArray arrayWithCapacity:0];
	NSString *host = [NSString stringWithUTF8String:hostname];
	
	vals = (NSArray *)SCDynamicStoreCopyValue(ctx->store, HEIMDAL_SC_DOMAIN_REALM_MAPPING);
	if (vals == NULL)
	    goto out;
	
	/* search dict for matches, all matches from first domain that matches */
	for (NSDictionary *a in vals) {
	    for (NSString *domain in a)
		if ([host hasSuffix:domain])
		    [res addObject:[a valueForKey:domain]];
	    
	    if ([res count])
		break;
	}
	if ([res count] == 0)
	    goto out;

	for (NSString *realm in res)
	    add_realms(context, userptr, [realm UTF8String]);
	
	ret = 0;
	out:
	do { } while(0);
    }
    @catch (NSException *exception) { }
    @finally {
	
	if (vals)
	    CFRelease((CFTypeRef)vals);
	[pool drain];
    }
    
    return ret;
}

static krb5_error_code
config_init(krb5_context context, void **ptr)
{
    struct config_ctx *ctx = calloc(1, sizeof(*ctx));

    if (ctx == NULL)
	return ENOMEM;

    ctx->store = SCDynamicStoreCreate(kCFAllocatorDefault, CFSTR("SCKerberosConfig"), NULL, NULL);
    if (ctx->store == NULL) {
	free(ctx);
	return ENOMEM;
    }
	
    *ptr = ctx;
    return 0;
}

static void
config_fini(void *ptr)
{
    struct config_ctx *ctx = ptr;

    CFRelease(ctx->store);
    free(ctx);
}


krb5plugin_config_ftable krb5_configuration = {
    KRB5_PLUGIN_CONFIGURATION_VERSION_1,
    config_init,
    config_fini,
    get_default_realm,
    get_host_domain
};
