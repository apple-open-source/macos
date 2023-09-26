/*-
 * Copyright (c) 2013 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2013 Apple Inc. All rights reserved.
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

#import <TargetConditionals.h>

#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreFoundation/CFRuntime.h>
#import <CommonCrypto/CommonHMAC.h>
#import <Security/SecCFAllocator.h>
#import <os/log.h>

#import "roken.h"
#import "heimcred.h"
#import "heimbase.h"
#import "config.h"
#import "heimntlm.h"
#import "common.h"
#import "gsscred.h"
#include "heimscram.h"

#if ENABLE_NTLM

#if TARGET_OS_OSX
#import <SystemConfiguration/SystemConfiguration.h>

static dispatch_queue_t ntlmDomainQueue;
static char *ntlmDomain;

static void
update_ntlm(SCDynamicStoreRef store, CFArrayRef changedKeys, void *info)
{
    CFDictionaryRef settings;
    CFStringRef n;

    if (store == NULL)
	return;

    settings = (CFDictionaryRef)SCDynamicStoreCopyValue(store, CFSTR("com.apple.smb"));
    if (settings == NULL)
	return;

    n = CFDictionaryGetValue(settings, CFSTR("NetBIOSName"));
    if (n == NULL || CFGetTypeID(n) != CFStringGetTypeID() || CFStringGetLength(n) == 0)
	goto fin;

    if (ntlmDomain)
	free(ntlmDomain);
    ntlmDomain = rk_cfstring2cstring(n);
    strupr(ntlmDomain);

fin:
    CFRELEASE_NULL(settings);
    return;
}

static void
setup_ntlm_notification(void)
{
    SCDynamicStoreRef store = NULL;

    store = SCDynamicStoreCreate(kCFAllocatorDefault, CFSTR("gsscred-NetBIOSName"), update_ntlm, NULL);
    if (store == NULL)
	return;

    CFTypeRef key[] = {CFSTR("com.apple.smb")};
    CFArrayRef keys = CFArrayCreate(kCFAllocatorDefault, key, 1, NULL);
    SCDynamicStoreSetNotificationKeys(store, keys, NULL);
    CFRELEASE_NULL(keys);

    ntlmDomainQueue = dispatch_queue_create("gsscred-NetBIOSName", NULL);

    SCDynamicStoreSetDispatchQueue(store, ntlmDomainQueue);

    dispatch_sync(ntlmDomainQueue, ^{ update_ntlm(store, NULL, NULL); });
    CFRELEASE_NULL(store);
}

static char *
copy_netbios_name(void)
{
    __block char *domain = NULL;
    dispatch_sync(ntlmDomainQueue, ^{
	    if (ntlmDomain)
		domain = strdup(ntlmDomain);
	});
    if (domain == NULL)
	domain = strdup("workstation");
    return domain;
}
#endif // TARGET_OS_OSX

static int
ntlm_domain_is_hostname(const char *name)
{
    return (name[0] == '\\');
}

static CFTypeRef
DictionaryGetTypedValue(CFDictionaryRef dict, CFStringRef key, CFTypeID reqType)
{
	CFTypeRef value = CFDictionaryGetValue(dict, key);
	if (value == NULL || CFGetTypeID(value) != reqType)
		return NULL;
	return value;
}

static CFDictionaryRef
NTLMAuthCallback(struct peer *peer, HeimCredRef cred, CFDictionaryRef input)
{
	CFDictionaryRef result_cfdict = NULL;
	CFDataRef type1_cfdata = NULL;
	CFDataRef type2_cfdata = NULL;
	CFDataRef type3_cfdata = NULL;
	CFDataRef sessionkey_cfdata = NULL;
	CFDataRef channel_binding_cfdata = NULL;
	CFStringRef client_target_name_cfstr = NULL;
	CFStringRef username_cfstr = NULL;
	CFStringRef domain_cfstr = NULL;
	CFNumberRef	flags_cfnum = NULL;
	CFNumberRef	kcmflags_cfnum = NULL;
	CFDataRef ntlmhash_cfdata = NULL;
	CFDictionaryRef credential_cfdict = NULL;

	char *username_cstr = NULL;
	char *domain_cstr = NULL;
	char *client_target_name_cstr = NULL;
	uint32_t type1flags = 0;
	uint32_t flags = 0;
	char flagname[256];

	size_t mic_offset = 0;
	struct ntlm_buf nthash;
	struct ntlm_buf ndata;
	struct ntlm_buf type1data;
	struct ntlm_buf type2data;
	struct ntlm_buf sessionkey;
	struct ntlm_buf tidata;
	struct ntlm_buf cb; /* channel binding */
	struct ntlm_type2 type2;
	struct ntlm_type3 type3;
	struct ntlm_targetinfo ti;
	unsigned char ntlmv2[16];
	static uint8_t zeros[16] = { 0 };
	int ret;

	os_log_debug(GSSOSLog(), "NTLMAuthCallback");

#if TARGET_OS_OSX
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
	setup_ntlm_notification();
    });
#endif
    
	memset(&nthash, 0, sizeof(nthash));
	memset(&ndata, 0, sizeof(ndata));
	memset(&type1data, 0, sizeof(type1data));
	memset(&type2data, 0, sizeof(type2data));
	memset(&sessionkey, 0, sizeof(sessionkey));
	memset(&tidata, 0, sizeof(tidata));
	memset(&cb, 0, sizeof(cb));
	memset(&type2, 0, sizeof(type2));
	memset(&type3, 0, sizeof(type3));
	memset(&ti, 0, sizeof(ti));

	/* ntlm hash  data*/
	credential_cfdict = HeimCredGetAttributes(cred);
	if (!credential_cfdict)
		goto error;
	ntlmhash_cfdata = DictionaryGetTypedValue(credential_cfdict, kHEIMAttrData, CFDataGetTypeID());
	if (ntlmhash_cfdata == NULL)
		goto error;
	nthash.data = (UInt8 *)CFDataGetBytePtr(ntlmhash_cfdata);
	nthash.length = CFDataGetLength(ntlmhash_cfdata);

	/* NTLM type2 data */
	type2_cfdata = DictionaryGetTypedValue(input, kHEIMAttrNTLMType2Data, CFDataGetTypeID());
	if (type2_cfdata == NULL)
	goto error;
	type2data.data = (UInt8 *)CFDataGetBytePtr(type2_cfdata);
	type2data.length = CFDataGetLength(type2_cfdata);

	/* channel binding */
	channel_binding_cfdata = DictionaryGetTypedValue(input, kHEIMAttrNTLMChannelBinding, CFDataGetTypeID());
	if (channel_binding_cfdata == NULL)
		goto error;
	cb.data = (UInt8 *)CFDataGetBytePtr(channel_binding_cfdata);
	cb.length = CFDataGetLength(channel_binding_cfdata);

	/* NTLM type1 data */
	type1_cfdata = DictionaryGetTypedValue(input, kHEIMAttrNTLMType1Data, CFDataGetTypeID());
	if (type1_cfdata == NULL)
		goto error;
	type1data.data = (UInt8 *)CFDataGetBytePtr(type1_cfdata);
	type1data.length = CFDataGetLength(type1_cfdata);

	/* client targetname */
	client_target_name_cfstr = DictionaryGetTypedValue(input, kHEIMAttrNTLMClientTargetName, CFStringGetTypeID());
	if (client_target_name_cfstr == NULL)
		goto error;
	client_target_name_cstr = rk_cfstring2cstring(client_target_name_cfstr);
	if (client_target_name_cstr == NULL)
		goto error;

	/* type1 flags */
	flags_cfnum = DictionaryGetTypedValue(input, kHEIMAttrNTLMClientFlags, CFNumberGetTypeID());
	if (flags_cfnum == NULL) {
		    goto error;
	} else {
	    CFRetain(flags_cfnum);
	}

	if (! CFNumberGetValue(flags_cfnum, kCFNumberSInt32Type, &type1flags))
	   goto error;

	/* NTLM username */
	username_cfstr = DictionaryGetTypedValue(input, kHEIMAttrNTLMUsername, CFStringGetTypeID());
	if (username_cfstr == NULL)
		goto error;
	username_cstr = rk_cfstring2cstring(username_cfstr);
	if (username_cstr == NULL)
		goto error;

	/* NTLM domain */
	domain_cfstr = DictionaryGetTypedValue(input, kHEIMAttrNTLMDomain, CFStringGetTypeID());
	if (domain_cfstr == NULL)
		goto error;
	CFRetain(domain_cfstr);
	domain_cstr = rk_cfstring2cstring(domain_cfstr);
	if (domain_cstr == NULL)
		goto error;

	ndata.data = type2data.data;
	ndata.length = type2data.length;

	ret = heim_ntlm_decode_type2(&ndata, &type2); /* ndata(encoded type2 data) -> decoded type2 data  */
	if (ret) {
		os_log_error(GSSOSLog(), "heim_ntlm_decode_type2 (%d)\n", ret);
		goto error;
	}

	if (checkNTLMChallenge(peer, type2.challenge)) {
		os_log_error(GSSOSLog(), "NTLM Mirror attack detected");
		goto error;
	    }

	/* if service name or case matching with domain, let pick the domain */
	if (ntlm_domain_is_hostname(domain_cstr) || strcasecmp(domain_cstr, type2.targetname) == 0) {
	    free(domain_cstr);
	    CFRELEASE_NULL(domain_cfstr);
	    domain_cfstr = CFStringCreateWithCString(NULL, type2.targetname, kCFStringEncodingUTF8);
	    if (domain_cfstr == NULL) {
		goto error;
	    }
	    domain_cstr = rk_cfstring2cstring(domain_cfstr);
	    if (domain_cstr == NULL)
		goto error;
	}
    
	/* NTLM type3 data */
	type3.username = username_cstr;
	type3.flags = type2.flags;
	/* only allow what we negotiated ourself */
	type3.flags &= type1flags;
	type3.targetname = domain_cstr;
#if TARGET_OS_OSX
	type3.ws = copy_netbios_name();
#else
	type3.ws = strdup("MOBILE");
#endif
	/* verify infotarget */
	ret = heim_ntlm_decode_targetinfo(&type2.targetinfo, 1, &ti);
	if (ret)
		goto error;

	if (ti.avflags & NTLM_TI_AV_FLAG_GUEST)
		flags |= HEIMCRED_NTLM_FLAG_AV_GUEST;

	if (ti.channel_bindings.data)
		free(ti.channel_bindings.data);
	if (ti.targetname)
		free(ti.targetname);

	/*
	 * We are going to use MIC, tell server so it can reject the
	 * authenticate if the mic is missing.
	 */
	ti.avflags |= NTLM_TI_AV_FLAG_MIC;
	ti.targetname = client_target_name_cstr;

	if (cb.length == 0) {
		ti.channel_bindings.data = zeros;
		ti.channel_bindings.length = sizeof(zeros);
	} else {
		ti.channel_bindings.data = cb.data;
		ti.channel_bindings.length = cb.length;
	}

	ret = heim_ntlm_encode_targetinfo(&ti, TRUE, &tidata);
    if (ret)
	    goto error;
	ti.targetname = NULL;
	ti.channel_bindings.data = NULL;
	ti.channel_bindings.length = 0;

	heim_ntlm_free_targetinfo(&ti);
	if (ret)
		goto error;

	/*
	 * Send empty LM2 response
	 */

	type3.lm.data = malloc(24);
	if (type3.lm.data == NULL) {
		ret = ENOMEM;
	} else {
		type3.lm.length = 24;
		memset(type3.lm.data, 0, type3.lm.length);
	}

	if (ret)
		goto error;

	ret = heim_ntlm_calculate_ntlm2(nthash.data,
									nthash.length,
									type3.username,
									domain_cstr,
									type2.challenge,
									&tidata,
									ntlmv2,
									&type3.ntlm);
	if (ret)
		goto error;

	if (type3.flags & NTLM_NEG_KEYEX) {
		ret = heim_ntlm_build_ntlm2_master(ntlmv2, sizeof(ntlmv2),
										&type3.ntlm,
										&sessionkey,
										&type3.sessionkey);
	} else {
		ret = heim_ntlm_v2_base_session(ntlmv2, sizeof(ntlmv2), &type3.ntlm, &sessionkey);
	}

	if (ret)
		goto error;

	ret = heim_ntlm_encode_type3(&type3, &ndata, &mic_offset); /* type3 data -> ndata(encoded type3 data) */
	if (ret)
		goto error;
	if (ndata.length < CC_MD5_DIGEST_LENGTH) {
		ret = EINVAL;
		goto error;
	}

	if (mic_offset && mic_offset < ndata.length - CC_MD5_DIGEST_LENGTH) {
		CCHmacContext mic;
		uint8_t *p = (uint8_t *)ndata.data + mic_offset;
		CCHmacInit(&mic, kCCHmacAlgMD5, sessionkey.data, sessionkey.length);
		CCHmacUpdate(&mic, type1data.data, type1data.length);
		CCHmacUpdate(&mic, type2data.data, type2data.length);
		CCHmacUpdate(&mic, ndata.data, ndata.length);
		CCHmacFinal(&mic, p);
	}
/* REPLY */

	/* encoded type3 data */
	type3_cfdata = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)ndata.data, ndata.length );
	if (!type3_cfdata)
		goto error;
	// free ndata(encoded type3 data)

	/* kcm flags  */
	kcmflags_cfnum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &flags);
	if (kcmflags_cfnum == NULL)
		goto error;

	/*  flags */
	CFRELEASE_NULL(flags_cfnum);
	flags_cfnum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &type3.flags);
	if (flags_cfnum == NULL)
		goto error;

	/* sessionkey */
	sessionkey_cfdata = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)sessionkey.data, sessionkey.length );
	if (!sessionkey_cfdata)
		goto error;

	/* username  = username_cfstr */
	/* domain  = domain_cfstr */

	heim_ntlm_unparse_flags(type3.flags, flagname, sizeof(flagname));

	os_log_info(GSSOSLog(), "ntlm v2 request processed for %s\\%s flags: %s",
		domain_cstr, username_cstr, flagname);

	const void *add_keys[] = {
	(void *)kHEIMObjectType,
			kHEIMAttrType,
			kHEIMAttrNTLMUsername,
			kHEIMAttrNTLMDomain,
			kHEIMAttrNTLMType3Data,
			kHEIMAttrNTLMSessionKey,
			kHEIMAttrNTLMClientFlags,
			kHEIMAttrNTLMKCMFlags
	};
	const void *add_values[] = {
	(void *)kHEIMObjectNTLM,
			kHEIMTypeNTLM,
			username_cfstr,
			domain_cfstr,
			type3_cfdata,
			sessionkey_cfdata,
			flags_cfnum,
			kcmflags_cfnum
	};

	result_cfdict = CFDictionaryCreate(NULL, add_keys, add_values, sizeof(add_keys) / sizeof(add_keys[0]), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	heim_assert(result_cfdict != NULL, "Failed to create dictionary");
error:
	memset(ntlmv2, 0, sizeof(ntlmv2));

	if (type3.lm.data)
		free(type3.lm.data);
	if (type3.ntlm.data)
		free(type3.ntlm.data);
	if (type3.sessionkey.data)
		free(type3.sessionkey.data);
	if (type3.ws)
		free(type3.ws);
	heim_ntlm_free_type2(&type2);
	heim_ntlm_free_buf(&sessionkey);
	heim_ntlm_free_buf(&tidata);

	CFRELEASE_NULL(type3_cfdata);
	CFRELEASE_NULL(kcmflags_cfnum);
	CFRELEASE_NULL(flags_cfnum);
	CFRELEASE_NULL(sessionkey_cfdata);
	CFRELEASE_NULL(domain_cfstr);

	return result_cfdict;
}

#endif /* ENABLE_NTLM */

/*
 *
 */

void
_HeimCredRegisterNTLM(void)
{
#if ENABLE_NTLM
	CFMutableSetRef set = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
	CFMutableDictionaryRef schema;

	schema = _HeimCredCreateBaseSchema(kHEIMObjectNTLM);

	CFDictionarySetValue(schema, kHEIMAttrData, CFSTR("Wd"));
	CFDictionarySetValue(schema, kHEIMAttrNTLMUsername, CFSTR("Rs"));
	CFDictionarySetValue(schema, kHEIMAttrNTLMDomain, CFSTR("Rs"));
	CFDictionarySetValue(schema, kHEIMAttrTransient, CFSTR("b"));
	CFDictionarySetValue(schema, kHEIMAttrAllowedDomain, CFSTR("as"));
	CFDictionarySetValue(schema, kHEIMAttrStatus, CFSTR("n"));

	CFSetAddValue(set, schema);
	CFRELEASE_NULL(schema);

	_HeimCredRegisterMech(kHEIMTypeNTLM, set, NULL, NTLMAuthCallback, NULL, DefaultTraceCallback, false, NULL);
	CFRELEASE_NULL(set);
#endif /* ENABLE_NTLM */
}

void
_HeimCredRegisterNTLMReflection(void)
{
#if ENABLE_NTLM
    CFMutableSetRef set = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
    CFMutableDictionaryRef schema;

    schema = _HeimCredCreateBaseSchema(kHEIMObjectNTLMReflection);

    CFDictionarySetValue(schema, kHEIMAttrData, CFSTR("WRd"));

    CFSetAddValue(set, schema);
    CFRELEASE_NULL(schema);

    _HeimCredRegisterMech(kHEIMTypeNTLMRelfection, set, NULL, NULL, NULL, DefaultTraceCallback, false, NULL);
    CFRELEASE_NULL(set);
#endif /* ENABLE_NTLM */
}

#if ENABLE_SCRAM
static CFDictionaryRef
SCRAMAuthCallback(struct peer *peer, HeimCredRef cred, CFDictionaryRef input)
{
    heim_scram_data proof, server, client_key, stored, server_key, session_key;
    heim_scram_method method = HEIM_SCRAM_DIGEST_SHA1;
    krb5_data salt, c1, s1, c2noproof;
    krb5_error_code ret;
    uint32_t iterations;
    unsigned char *p, *q;
    size_t n;

    memset(&proof, 0, sizeof(proof));
    memset(&server, 0, sizeof(server));
    memset(&client_key, 0, sizeof(client_key));
    memset(&stored, 0, sizeof(stored));
    memset(&server_key, 0, sizeof(server_key));
    memset(&session_key, 0, sizeof(session_key));
    krb5_data_zero(&salt);
    krb5_data_zero(&c1);
    krb5_data_zero(&s1);
    krb5_data_zero(&c2noproof);

    CFNumberRef iterations_number = NULL;
    CFDataRef salt_data = NULL;
    CFDataRef c1_data = NULL;
    CFDataRef s1_data = NULL;
    CFDataRef c2noproof_data = NULL;
    CFDataRef password_data = NULL;
    CFStringRef pw_string = NULL;

    CFDataRef proof_data = NULL;
    CFDataRef server_data = NULL;
    CFDataRef sessionkey_data = NULL;

    CFDictionaryRef attr_cfdict = NULL;

    iterations_number = CFDictionaryGetValue(input, kHEIMAttrSCRAMIterations);
    if (iterations_number != NULL) {
	CFNumberGetValue(iterations_number, kCFNumberIntType, &iterations);
	if (iterations <= 0) {
	    goto out;
	}
    } else {
	goto out;
    }

    salt_data = CFDictionaryGetValue(input, kHEIMAttrSCRAMSalt);
    if (salt_data) {
	salt.length = CFDataGetLength(salt_data);
	salt.data = calloc(1, salt.length);
	if (salt.data == NULL) {
	    ret = ENOMEM;
	    goto out;
	}
	CFDataGetBytes(salt_data, CFRangeMake(0, CFDataGetLength(salt_data)), salt.data);
    }

    c1_data = CFDictionaryGetValue(input, kHEIMAttrSCRAMC1);
    if (c1_data) {
	c1.length = CFDataGetLength(c1_data);
	c1.data = calloc(1, c1.length);
	if (c1.data == NULL) {
	    ret = ENOMEM;
	    goto out;
	}
	CFDataGetBytes(c1_data, CFRangeMake(0, CFDataGetLength(c1_data)), c1.data);
    }

    s1_data = CFDictionaryGetValue(input, kHEIMAttrSCRAMS1);
    if (s1_data) {
	s1.length = CFDataGetLength(s1_data);
	s1.data = calloc(1, s1.length);
	if (s1.data == NULL) {
	    ret = ENOMEM;
	    goto out;
	}
	CFDataGetBytes(s1_data, CFRangeMake(0, CFDataGetLength(s1_data)), s1.data);
    }

    c2noproof_data = CFDictionaryGetValue(input, kHEIMAttrSCRAMC2NoProof);
    if (c2noproof_data) {
	c2noproof.length = CFDataGetLength(c2noproof_data);
	c2noproof.data = calloc(1, c2noproof.length);
	if (c2noproof.data == NULL) {
	    ret = ENOMEM;
	    goto out;
	}
	CFDataGetBytes(c2noproof_data, CFRangeMake(0, CFDataGetLength(c2noproof_data)), c2noproof.data);
    }

    password_data = CFDictionaryGetValue(cred->attributes, kHEIMAttrData);
    if (!password_data) {
	goto out;
    }

    pw_string = CFStringCreateWithBytes(SecCFAllocatorZeroize(), CFDataGetBytePtr(password_data), CFDataGetLength(password_data), kCFStringEncodingUTF8, false);

    ret = heim_scram_stored_key(method, [(__bridge NSString *)pw_string UTF8String], iterations, &salt, &client_key, &stored, &server_key);
    if (ret) {
	goto out;
    }

    ret = heim_scram_generate(method, &stored, &server_key,
			      &c1, &s1, &c2noproof, &proof, &server);
    if (ret) {
	goto out;
    }


    ret = heim_scram_session_key(method, &stored, &client_key,
				 &c1, &s1, &c2noproof,
				 &session_key);
    if (ret) {
	goto out;
    }

    /*
     * Now client_key XOR proof
     */
    p = proof.data;
    q = client_key.data;

    for (n = 0 ; n < client_key.length; n++) {
	p[n] = p[n] ^ q[n];
    }

    proof_data = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)proof.data, proof.length );
    if (!proof_data) {
	ret = KRB5_CC_IO;
	goto out;
    }

    server_data = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)server.data, server.length );
    if (!server_data) {
	ret = KRB5_CC_IO;
	goto out;
    }

    sessionkey_data = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)session_key.data, session_key.length );
    if (!sessionkey_data) {
	ret = KRB5_CC_IO;
	goto out;
    }

    //add to attributes
    const void *attr_keys[] = {
	kHEIMAttrSCRAMProof,
	kHEIMAttrSCRAMServer,
	kHEIMAttrSCRAMSessionKey
    };
    const void *attr_values[] = {
	proof_data,
	server_data,
	sessionkey_data
    };

    attr_cfdict = CFDictionaryCreate(NULL, attr_keys, attr_values, sizeof(attr_keys) / sizeof(attr_keys[0]), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);


out:


    krb5_data_free(&salt);
    krb5_data_free(&c1);
    krb5_data_free(&s1);
    krb5_data_free(&c2noproof);

    heim_scram_data_free(&proof);
    heim_scram_data_free(&server);
    heim_scram_data_free(&client_key);
    heim_scram_data_free(&stored);
    heim_scram_data_free(&server_key);
    heim_scram_data_free(&session_key);

    CFRELEASE_NULL(pw_string);

    CFRELEASE_NULL(proof_data);
    CFRELEASE_NULL(server_data);
    CFRELEASE_NULL(sessionkey_data);

    return attr_cfdict;

}
#endif

void
_HeimCredRegisterSCRAM(void)
{
#if ENABLE_SCRAM
	CFMutableSetRef set = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
	CFMutableDictionaryRef schema;

	schema = _HeimCredCreateBaseSchema(kHEIMObjectSCRAM);

	CFDictionarySetValue(schema, kHEIMAttrData, CFSTR("Wd"));
	CFDictionarySetValue(schema, kHEIMAttrSCRAMUsername, CFSTR("Rs"));

	CFSetAddValue(set, schema);
	CFRELEASE_NULL(schema);

	_HeimCredRegisterMech(kHEIMTypeSCRAM, set, NULL, SCRAMAuthCallback, NULL, DefaultTraceCallback, false, NULL);
	CFRELEASE_NULL(set);
#endif /* ENABLE_NTLM */
}
