/*-
 * Copyright (c) 2013 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#import "GSSCredHelper.h"
#import <CoreFoundation/CFXPCBridge.h>
#import "common.h"
#import "krb5.h"

os_log_t GSSHelperOSLog(void)
{
    static dispatch_once_t once;
    static os_log_t log;
    dispatch_once(&once, ^{ log = os_log_create("com.apple.Heimdal", "GSSCredHelper"); });
    return log;
};

@implementation GSSHelperPeer
@synthesize conn;
@synthesize bundleIdentifier;
@synthesize session;
@end

@implementation GSSCredHelper

+ (void)do_Acquire:(GSSHelperPeer *)peer request:(xpc_object_t) request reply:(xpc_object_t) reply
{
    os_log_debug(GSSHelperOSLog(), "do_Acquire %@", peer.bundleIdentifier);
    
    krb5_error_code ret = 0;
    krb5_ccache  ccache = NULL;
    krb5_creds cred;
    krb5_const_realm realm;
    krb5_get_init_creds_opt *opt = NULL;
    char *in_tkt_service = NULL;
    krb5_uuid cacheUUID;
    krb5_principal principal = NULL;
    krb5_context context = NULL;
    
    krb5_set_home_dir_access(NULL, FALSE);
    memset(&cred, 0, sizeof(cred));
    ret = krb5_init_context(&context);
    
    NSMutableDictionary *attributesToPrint = nil;
    NSString *clientName = nil;
    NSData *passwordData = nil;
    NSString *passwordString = nil;
    
    xpc_object_t xpcattrs = xpc_dictionary_get_value(request, "attributes");
    NSDictionary *attributes = CFBridgingRelease(_CFXPCCreateCFObjectFromXPCObject(xpcattrs));
    if (attributes==nil)
    {
	os_log_error(GSSHelperOSLog(), "unable to acquire credential without attributes %d", peer.session);
	ret = KRB5_FCC_INTERNAL;
	goto out;
    }
    
    // print all the values except the kHEIMAttrData entry for debugging.  Printing it can expose the password length.
    attributesToPrint = [attributes mutableCopy];
    [attributesToPrint removeObjectForKey:kHEIMAttrData];
    os_log_debug(GSSHelperOSLog(), "attributes for acquire: %@", attributesToPrint);
    
    CFUUIDRef cacheID = CFBridgingRetain(attributes[(__bridge id _Nonnull)kHEIMAttrParentCredential]);
    if (cacheID == NULL) {
	os_log_error(GSSHelperOSLog(), "unable to acquire credential without cache uuid %d", peer.session);
	ret = KRB5_FCC_INTERNAL;
	goto out;
    } else {
	CFUUIDBytes bytes = CFUUIDGetUUIDBytes(cacheID);
	memcpy(cacheUUID, &bytes, sizeof(krb5_uuid));
	os_log_debug(GSSHelperOSLog(), "using cache: %@", CFBridgingRelease(CFUUIDCreateString(NULL, cacheID)));
	CFRelease(cacheID);
    }
    
    clientName = attributes[(__bridge id _Nonnull)kHEIMAttrClientName];
    if (clientName==nil) {
	os_log_error(GSSHelperOSLog(), "unable to acquire credential without principal %d", peer.session);
	ret = KRB5_FCC_INTERNAL;
	goto out;
    }
    
    passwordData = attributes[(__bridge id _Nonnull)kHEIMAttrData];
    if (passwordData==nil) {
	os_log_error(GSSHelperOSLog(), "unable to acquire credential without password %d", peer.session);
	ret = KRB5_FCC_INTERNAL;
	goto out;
    }
    
    passwordString = [[NSString alloc] initWithData:passwordData encoding:NSUTF8StringEncoding];
    
    ret = krb5_cc_resolve_by_uuid(context, "XCACHE", &ccache, cacheUUID);
    if (ret) {
	os_log_error(GSSHelperOSLog(), "unable to find cache %d, %d", peer.session, ret);
	goto out;
    }
    
    ret = krb5_cc_get_principal(context, ccache, &principal);
    if (ret) {
	os_log_error(GSSHelperOSLog(), "unable to find cache %d, %d", peer.session, ret);
	goto out;
    }
    
    const char *princ_realm = krb5_principal_get_realm(context, principal);
    if (ret || !princ_realm) {
	krb5_free_principal(context, principal);
	ret = krb5_make_principal(context, &principal, [clientName UTF8String],
				  KRB5_WELLKNOWN_NAME, KRB5_ANON_NAME,
				  NULL);
	if (ret) {
	    os_log_debug(GSSHelperOSLog(), "Failed to parse principal %d: %@", peer.session, clientName);
	    goto out;
	}
    }
    
    ret = krb5_get_init_creds_opt_alloc(context, &opt);
    if (ret)
	goto out;
    
    realm = krb5_principal_get_realm(context, principal);
    krb5_get_init_creds_opt_set_default_flags(context, "gsscred", realm, opt);
    krb5_get_init_creds_opt_set_forwardable(opt, 1);
    krb5_get_init_creds_opt_set_proxiable(opt, 1);
    krb5_get_init_creds_opt_set_canonicalize(context, opt, TRUE);
    krb5_get_init_creds_opt_set_win2k(context, opt, TRUE);
    
    ret = krb5_get_init_creds_password(context,
				       &cred,
				       principal,
				       [passwordString UTF8String],
				       NULL,
				       NULL,
				       0,
				       in_tkt_service,
				       opt);
    
    passwordString = nil;
    
    if (ret) {
	const char *msg = krb5_get_error_message(context, ret);
	os_log_error(GSSHelperOSLog(), "Failed to acquire credentials for cache: %s", msg);
	krb5_free_error_message(context, msg);
	if (in_tkt_service != NULL)
	    free(in_tkt_service);
	goto out;
    }
    
    if (in_tkt_service != NULL)
	free(in_tkt_service);
    
    ret = krb5_cc_store_cred(context, ccache, &cred);
    if (ret) {
	const char *msg = krb5_get_error_message(context, ret);
	os_log_error(GSSHelperOSLog(), "Failed to store credentials for cache %d: %s", peer.session, msg);
	krb5_free_error_message(context, msg);
	krb5_free_cred_contents(context, &cred);
	goto out;
    }
    
    out:
    if (opt)
	krb5_get_init_creds_opt_free(context, opt);
    
    NSDictionary *replydict = @{
	@"status":[NSNumber numberWithInt:ret],
	@"expire":[NSNumber numberWithLong:cred.times.endtime],
    };
    os_log_debug(GSSHelperOSLog(), "do_Acquire results: %@", replydict);
    xpc_object_t xpcreplyattrs = _CFXPCCreateXPCObjectFromCFObject((__bridge CFTypeRef)replydict);
    
    xpc_dictionary_set_value(reply, "result", xpcreplyattrs);
    
    krb5_cc_close(context, ccache);
    krb5_free_cred_contents(context, &cred);
    krb5_free_principal(context, principal);
    krb5_free_context(context);
    
}

+ (void)do_Refresh:(GSSHelperPeer *)peer request:(xpc_object_t) request reply:(xpc_object_t) reply
{
    os_log_debug(GSSHelperOSLog(), "do_Refresh %@", peer.bundleIdentifier);
    
    krb5_error_code ret;
    krb5_creds in, *out = NULL;
    krb5_creds kcred;
    krb5_kdc_flags flags;
    krb5_const_realm realm = NULL;
    krb5_uuid cacheUUID;
    krb5_ccache ccache = NULL;
    krb5_ccache tempcache = NULL;
    krb5_context context = NULL;
    krb5_get_init_creds_opt *opt = NULL;
    
    NSString *clientName = nil;
    NSString *serverName = nil;
    NSMutableDictionary *attributesToPrint = nil;
    
    time_t expire = 0;
    
    krb5_set_home_dir_access(NULL, FALSE);
    
    ret = krb5_init_context(&context);

    memset(&in, 0, sizeof(in));
    
    xpc_object_t xpcattrs = xpc_dictionary_get_value(request, "attributes");
    NSDictionary *attributes = CFBridgingRelease(_CFXPCCreateCFObjectFromXPCObject(xpcattrs));
    if (attributes==nil)
    {
	os_log_error(GSSHelperOSLog(), "unable to acquire credential without attributes: %d", peer.session);
	ret = KRB5_FCC_INTERNAL;
	goto out;
    }
    
    // print all the values except the kHEIMAttrData entry for debugging.  Printing it can expose the data length.
    attributesToPrint = [attributes mutableCopy];
    [attributesToPrint removeObjectForKey:kHEIMAttrData];
    os_log_debug(GSSHelperOSLog(), "attributes for refresh: %@", attributesToPrint);
    
    CFUUIDRef cacheID = CFBridgingRetain(attributes[(__bridge id _Nonnull)kHEIMAttrParentCredential]);
    if (cacheID == NULL) {
	os_log_error(GSSHelperOSLog(), "unable to acquire credential without cache uuid %d", peer.session);
	ret = KRB5_FCC_INTERNAL;
	goto out;
    } else {
	CFUUIDBytes bytes = CFUUIDGetUUIDBytes(cacheID);
	memcpy(cacheUUID, &bytes, sizeof(krb5_uuid));
	os_log_debug(GSSHelperOSLog(), "using cache: %@", CFBridgingRelease(CFUUIDCreateString(NULL, cacheID)));
	CFRelease(cacheID);
    }
    
    clientName = attributes[(__bridge id _Nonnull)kHEIMAttrClientName];
    if (clientName==nil) {
	os_log_error(GSSHelperOSLog(), "unable to acquire credential without principal: %d", peer.session);
	ret = KRB5_FCC_INTERNAL;
	goto out;
    } else {
	os_log_debug(GSSHelperOSLog(), "using clientName: %@", clientName);
    }
    
    //make a temp cache to store the cred.  if it is successful, then move it.
    krb5_cc_new_unique(context, "XCACHE", [clientName UTF8String], &tempcache);
    
    /* Find principal */
    ret = krb5_parse_name(context, [clientName UTF8String], &in.client);
    if (ret) {
	os_log_error(GSSHelperOSLog(),  "Failed to parse clientname");
	goto out;
    }

    serverName = attributes[(__bridge id _Nonnull)kHEIMAttrServerName];
    if (serverName!=nil) {
	os_log_debug(GSSHelperOSLog(), "using serverName: %@", serverName);
	ret = krb5_parse_name(context, [serverName UTF8String],  &in.server);
	if (ret) {
	    os_log_error(GSSHelperOSLog(), "Failed to copy service principal: %@", serverName);
	    goto out;
	}
    } else {
	realm = krb5_principal_get_realm(context, in.client);
	krb5_free_principal(context, in.server);
	ret = krb5_make_principal(context, &in.server, realm,
				  KRB5_TGS_NAME, realm, NULL);
	if (ret) {
	    os_log_error(GSSHelperOSLog(), "Failed to make TGS principal for realm: %s", realm);
	    goto out;
	}
    }
    
    // we dont have access to the user's original ticket life and renew till settings.  We will check for any gsscred settings or use defaults.
    ret = krb5_get_init_creds_opt_alloc(context, &opt);
    if (ret)
	goto out;
    
    krb5_get_init_creds_opt_set_default_flags(context, "gsscred", realm, opt);
    if (opt->flags & KRB5_GET_INIT_CREDS_OPT_TKT_LIFE)
	in.times.endtime = time(NULL) + opt->tkt_life;
    else
	in.times.endtime = time(NULL) + 10 * 60 * 60;  //default to 10 hours
    
    if (opt->flags & KRB5_GET_INIT_CREDS_OPT_RENEW_LIFE)
	in.times.renew_till = time(NULL) + opt->renew_life;
    else
	in.times.renew_till = time(NULL) + 3600 * 24 * 7;  //default to 1 week
    
    flags.i = 0;
    flags.b.renewable = TRUE;
    flags.b.renew = TRUE;
    
    /*
     * Capture the forwardable/proxyable bit from previous matching
     * service ticket, we can't use initial since that get lost in the
     * first renewal.
     */
   
    ret = krb5_cc_resolve_by_uuid(context, "XCACHE", &ccache, cacheUUID);
    if (ret) {
	os_log_error(GSSHelperOSLog(),  "Failed to resolve cache using uuid");
	goto out;
    }
    krb5_free_principal(context, in.client);
    in.client = NULL;

    ret = krb5_cc_get_principal(context, ccache, &in.client);
    if (ret) {
	os_log_error(GSSHelperOSLog(),  "Failed to retrieve principal from cache");
	goto out;
    }

    if (in.client == NULL) {
	ret = KRB5_FCC_INTERNAL;
	os_log_error(GSSHelperOSLog(),  "Principal can not be NULL");
	goto out;
    }

    krb5_creds mcreds;
    
    krb5_cc_clear_mcred(&mcreds);
    mcreds.server = in.server;
    mcreds.client = in.client;
    ret = krb5_cc_retrieve_cred(context, ccache, 0, &mcreds, &kcred);
    if(ret == 0) {
	os_log_debug(GSSHelperOSLog(), "found previous ticket");
	flags.b.forwardable = kcred.flags.b.forwardable;
	flags.b.proxiable = kcred.flags.b.proxiable;
	krb5_free_cred_contents(context, &kcred);
    }
    krb5_cc_clear_mcred(&mcreds);
    
    ret = krb5_get_kdc_cred(context,
			    ccache,
			    flags,
			    NULL,
			    NULL,
			    &in,
			    &out);
    if (ret) {
	os_log_error(GSSHelperOSLog(),  "Failed to renew credentials for cache: %@",
		     clientName);
	goto out;
    }
      
    ret = krb5_cc_initialize(context, tempcache, in.client);
    if(ret) {
	os_log_error(GSSHelperOSLog(), "error in krb5_cc_initialize: %d", ret);
	goto out;
    }

    ret = krb5_cc_store_cred(context, tempcache, out);
    if(ret) {
	krb5_warn(context, ret, "krb5_cc_initialize");
	os_log_error(GSSHelperOSLog(), "error in krb5_cc_store_cred: %d", ret);
	goto out;
    }

    ret = krb5_cc_move(context, tempcache, ccache);
    if (ret) {
	os_log_error(GSSHelperOSLog(), "unable to move cache: %d, %d", peer.session, ret);
    } else {
	tempcache = NULL;  //this is set to NULL because move will destroy and free it
    }
    
    expire = out->times.endtime;

    out:

    if (out)
	krb5_free_creds(context, out);
    if (opt)
	krb5_get_init_creds_opt_free(context, opt);
    if (ccache)
	krb5_cc_close(context, ccache);
    if (tempcache)
	krb5_cc_close(context, tempcache);
    
    krb5_free_cred_contents(context, &in);
    krb5_free_context(context);
        
    NSDictionary *replydict = @{
	@"status":[NSNumber numberWithInt:ret],
	@"expire":[NSNumber numberWithLong:expire],
    };
    os_log_debug(GSSHelperOSLog(), "do_Refresh results: %@", replydict);
    xpc_object_t xpcreplyattrs = _CFXPCCreateXPCObjectFromCFObject((__bridge CFTypeRef)replydict);
    
    xpc_dictionary_set_value(reply, "result", xpcreplyattrs);
}

@end
