/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
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
#import <syslog.h>

#import <Heimdal/HeimdalSystemConfiguration.h>

#import <Foundation/Foundation.h>
#import <SystemConfiguration/SystemConfiguration.h>
#import <krb5.h>
#import <locate_plugin.h>

/**
 * Reachablity plugin reads System Configuration to pick up the realm
 * configuration from OpenDirectory plugins, both OD and AD.
 *
 * The keys published is:
 *
 * Kerberos:REALM = {
 *   kadmin = [ { host = "hostname", port = "port-number" } ]
 *   kdc = [ .. ]
 *   kpasswd = [ ]
 * }
 *
 * port is optional
 *
 * The following behaivor is expected:
 *
 * 1. Not joined to a domain
 *      no entry published
 * 2. Joined to a domain and replica AVAILABLE:
 *       entry pushlished with content
 * 3. Joined to a domain and replica UNAVAILABLE
 *       entry pushlished, but no content
 *
 */

static krb5_error_code
reachability_init(krb5_context context, void **ctx)
{
    *ctx = NULL;
    return 0;
}

static void
reachability_fini(void *ctx)
{
}

static krb5_error_code
reachability_lookup(void *ctx,
		    unsigned long flags,
		    enum locate_service_type service,
		    const char *realm,
		    int domain,
		    int type,
		    int (*addfunc)(void *,int,struct sockaddr *),
		    void *addctx)
{
    krb5_error_code ret;
    NSAutoreleasePool *pool;
    NSString *svc, *sckey, *host, *port;
    struct addrinfo hints, *ai0, *ai;
    SCDynamicStoreRef store = NULL;
    NSDictionary *top = NULL;
    NSArray *vals;
    NSString *defport;
    int found_entry = 0;
    id rp;
    
    @try {
	pool = [[NSAutoreleasePool alloc] init];

	switch(service) {
	case locate_service_kdc:
	case locate_service_master_kdc:
	case locate_service_krb524:
	    svc = (NSString *)HEIMDAL_SC_LOCATE_TYPE_KDC;
	    defport = @"88";
	    break;
	case locate_service_kpasswd:
	    svc = (NSString *)HEIMDAL_SC_LOCATE_TYPE_KPASSWD;
	    defport = @"464";
	    break;
	case locate_service_kadmin:
	    svc = (NSString *)HEIMDAL_SC_LOCATE_TYPE_ADMIN;
	    defport = @"749";
	    break;
	default:
	    ret = KRB5_PLUGIN_NO_HANDLE;
	    goto out;
	}
	
	store = SCDynamicStoreCreate(kCFAllocatorDefault, CFSTR("Kerberos"), NULL, NULL);
	sckey = [NSString stringWithFormat:@"%@%s",
			  (NSString *)HEIMDAL_SC_LOCATE_REALM_PREFIX, realm];
	top = (NSDictionary *)SCDynamicStoreCopyValue(store, (CFStringRef)sckey);
	if (top == NULL) {
	    ret = KRB5_PLUGIN_NO_HANDLE;
	    goto out;
	}

	vals = [top valueForKey:svc];
	if (vals == NULL) {
	    ret = KRB5_PLUGIN_NO_HANDLE;
	    goto out;
	}

	if ([vals count] == 0)
	    syslog(LOG_WARNING,
		   "Kerberos-Reachability SystemConfiguration returned 0 entries for %s",
		   realm);
	
	for (NSDictionary *a in vals) {
	    host = [a valueForKey:(NSString *)HEIMDAL_SC_LOCATE_HOST];
	    
	    rp = [a valueForKey:(NSString *)HEIMDAL_SC_LOCATE_PORT];
	    if ([rp isKindOfClass:[NSString class]])
		port = rp;
	    else if ([rp respondsToSelector:@selector(stringValue)])
		port = [rp stringValue];
	    else
		port = defport;
	    if (port == nil)
		continue;
	    
	    memset(&hints, 0, sizeof(hints));
	    hints.ai_flags = 0;
	    hints.ai_family = type;
	    hints.ai_socktype = domain;
	    
	    if (getaddrinfo([host UTF8String], [port UTF8String], &hints, &ai0) != 0)
		continue;
	    
	    for (ai = ai0; ai != NULL; ai = ai->ai_next) {
		ret = addfunc(addctx, ai->ai_socktype, ai->ai_addr);
		if (ret == 0)
		    found_entry = 1;
	    }
	    freeaddrinfo(ai0);
	}
	
	if (!found_entry)
	    ret = KRB5_KDC_UNREACH;
	else
	    ret = 0;
     out:
	do {} while(0);
    }
    @catch (NSException *exception) { }
    @finally {

	if (top)
	    CFRelease((CFTypeRef)top);
	if (store)
	    CFRelease(store);
	[pool drain];
    }

    return ret;
}

static krb5_error_code
reachability_lookup_old(void *ctx,
			enum locate_service_type service,
			const char *realm,
			int domain,
			int type,
			int (*addfunc)(void *,int,struct sockaddr *),
			void *addctx)
{
    return reachability_lookup(ctx, KRB5_PLF_ALLOW_HOMEDIR, service,
			       realm, domain, type, addfunc, addctx);
}

krb5plugin_service_locate_ftable service_locator = {
    KRB5_PLUGIN_LOCATE_VERSION_2,
    reachability_init,
    reachability_fini,
    reachability_lookup_old,
    reachability_lookup
};
