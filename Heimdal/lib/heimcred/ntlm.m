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

#import "heimcred.h"
#import "heimbase.h"
#import "config.h"

#if ENABLE_NTLM

#if  0
static CFTypeRef
DictionaryGetTypedValue(CFDictionaryRef dict, CFStringRef key, CFTypeID reqType)
{
    CFTypeRef value = CFDictionaryGetValue(dict, key);
    if (value == NULL || CFGetTypeID(value) != reqType)
	return NULL;
    return value;
}

/*
 *
 */

static int
ntlm_domain_is_hostname(CFStringRef name)
{
    return CFStringHasPrefix(name, CFSTR("\\"));
}
#endif

static CFDictionaryRef
NTLMAuthCallback(HeimCredRef cred, CFDictionaryRef input)
{
#if 0
    CFMutableDictionaryRef output = NULL;
    CFDataRef type2data = NULL, cb = NULL, type1data = NULL;
    CFStringRef targetname = NULL;
    CFNumberRef _type1flags = NULL;

    CFStringRef userdomain;

    struct ntlm_type2 type2;
    struct ntlm_type3 type3;
    char *user = NULL, *domain = NULL, *targetname = NULL;
    struct ntlm_buf ndata, sessionkey, tidata;
    krb5_data cb, type1data, tempdata;
    krb5_error_code ret;
    uint32_t type1flags, flags = 0;
    const char *type = "unknown";
    char flagname[256];
    size_t mic_offset = 0;

    KCM_LOG_REQUEST(context, client, opcode);

    memset(&tidata, 0, sizeof(tidata));
    memset(&type2, 0, sizeof(type2));
    memset(&type3, 0, sizeof(type3));
    sessionkey.data = NULL;
    sessionkey.length = 0;
    
    //    kcm_log(10, "NTLM AUTH with cred %s\\%s", domain, user);

    type2data = DictionaryGetTypedValue(input, CFSTR("type2data"), CFDataGetTypeID());
    type1data = DictionaryGetTypedValue(input, CFSTR("type1data"), CFDataGetTypeID());
    cb = DictionaryGetTypedValue(input, CFSTR("cb"), CFDataGetTypeID());
    targetname = DictionaryGetValue(input, CFSTR("targetname"), CFStringGetTypeID());
    type1flags = DictionaryGetValue(input, CFSTR("type1flags"), CFNumberGetTypeID());

    /*
     *
     */

    username = CFDictionaryGetValue(cred->attributes, kHEIMAttrNTLMUsername);
    userdomain = CFDictionaryGetValue(cred->attributes, kHEIMAttrNTLMDomain);

    /*
     *
     */

    ndata.data = CFDataGetBytePtr(type2data);
    ndata.length = CFDataGetLength(type2data);

    ret = heim_ntlm_decode_type2(&ndata, &type2);
    if (ret)
	goto error;

#if 0 /* XXX */
    kcm_log(10, "checking for ntlm mirror attack");
    ret = check_ntlm_challage(type2.challenge);
    if (ret) {
	kcm_log(0, "ntlm mirror attack detected");
	goto error;
    }
#endif

    /* if service name or case matching with domain, let pick the domain */
    if (ntlm_domain_is_hostname(userdomain) || strcasecmp(domain, type2.targetname) == 0) {
	domain = type2.targetname;
    } else {
	domain = c->domain;
    }

    type3.username = c->user;
    type3.flags = type2.flags;
    /* only allow what we negotiated ourself */
    type3.flags &= type1flags;
    type3.targetname = domain;
    type3.ws = rk_UNCONST("workstation");

    /*
     * Only do NTLM Version 1 if they force us
     */
    
    if (gss_mo_get(GSS_NTLM_MECHANISM, GSS_C_NTLM_FORCE_V1, NULL)) {
	
	type = "v1";

	if (type2.flags & NTLM_NEG_NTLM2_SESSION) {
	    unsigned char nonce[8];
	    
	    if (CCRandomCopyBytes(kCCRandomDefault, nonce, sizeof(nonce))) {
		ret = EINVAL;
		goto error;
	    }

	    ret = heim_ntlm_calculate_ntlm2_sess(nonce,
						 type2.challenge,
						 c->nthash.data,
						 &type3.lm,
						 &type3.ntlm);
	} else {
	    ret = heim_ntlm_calculate_ntlm1(c->nthash.data,
					    c->nthash.length,
					    type2.challenge,
					    &type3.ntlm);

	}
	if (ret)
	    goto error;
	
	if (type3.flags & NTLM_NEG_KEYEX) {
	    ret = heim_ntlm_build_ntlm1_master(c->nthash.data,
					       c->nthash.length,
					       &sessionkey,
					       &type3.sessionkey);
	} else {
	    ret = heim_ntlm_v1_base_session(c->nthash.data, 
					    c->nthash.length,
					    &sessionkey);
	}
	if (ret)
	    goto error;

    } else {
	unsigned char ntlmv2[16];
	struct ntlm_targetinfo ti;
	static uint8_t zeros[16] = { 0 };
	
	type = "v2";

	/* verify infotarget */

	ret = heim_ntlm_decode_targetinfo(&type2.targetinfo, 1, &ti);
	if (ret)
	    goto error;
	
	if (ti.avflags & NTLM_TI_AV_FLAG_GUEST)
	    flags |= KCM_NTLM_FLAG_AV_GUEST;

	if (ti.channel_bindings.data)
	    free(ti.channel_bindings.data);
	if (ti.targetname)
	    free(ti.targetname);

	/* 
	 * We are going to use MIC, tell server so it can reject the
	 * authenticate if the mic is missing.
	 */
	ti.avflags |= NTLM_TI_AV_FLAG_MIC;
	ti.targetname = targetname;

	if (cb.length == 0) {
	    ti.channel_bindings.data = zeros;
	    ti.channel_bindings.length = sizeof(zeros);
	} else {
	    kcm_log(10, "using channelbindings of size %lu", (unsigned long)cb.length);
	    ti.channel_bindings.data = cb.data;
	    ti.channel_bindings.length = cb.length;
	}

	ret = heim_ntlm_encode_targetinfo(&ti, TRUE, &tidata);

	ti.targetname = NULL;
	ti.channel_bindings.data = NULL;
	ti.channel_bindings.length = 0;

	heim_ntlm_free_targetinfo(&ti);
	if (ret)
	    goto error;

	/*
	 * Prefer NTLM_NEG_EXTENDED_SESSION over NTLM_NEG_LM_KEY as
	 * decribed in 2.2.2.5.
	 */

	if (type3.flags & NTLM_NEG_NTLM2_SESSION)
	    type3.flags &= ~NTLM_NEG_LM_KEY;

	if ((type3.flags & NTLM_NEG_LM_KEY) && 
	    gss_mo_get(GSS_NTLM_MECHANISM, GSS_C_NTLM_SUPPORT_LM2, NULL)) {
	    ret = heim_ntlm_calculate_lm2(c->nthash.data,
					  c->nthash.length,
					  type3.username,
					  domain,
					  type2.challenge,
					  ntlmv2,
					  &type3.lm);
	} else {
	    type3.lm.data = malloc(24);
	    if (type3.lm.data == NULL) {
		ret = ENOMEM;
	    } else {
		type3.lm.length = 24;
		memset(type3.lm.data, 0, type3.lm.length);
	    }
	}
	if (ret)
	    goto error;

	ret = heim_ntlm_calculate_ntlm2(c->nthash.data,
					c->nthash.length,
					type3.username,
					domain,
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

	memset(ntlmv2, 0, sizeof(ntlmv2));
	if (ret)
	    goto error;
    }

    ret = heim_ntlm_encode_type3(&type3, &ndata, &mic_offset);
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

    tempdata.data = ndata.data;
    tempdata.length = ndata.length;
    ret = krb5_store_data(response, tempdata);
    heim_ntlm_free_buf(&ndata);

    if (ret) goto error;

    output = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);


    ret = krb5_store_int32(response, flags);
    if (ret) goto error;

    tempdata.data = sessionkey.data;
    tempdata.length = sessionkey.length;

    ret = krb5_store_data(response, tempdata);
    if (ret) goto error;
    ret = krb5_store_string(response, c->user);
    if (ret) goto error;
    ret = krb5_store_string(response, domain);
    if (ret) goto error;
    ret = krb5_store_uint32(response, type3.flags);
    if (ret) goto error;

    heim_ntlm_unparse_flags(type3.flags, flagname, sizeof(flagname));

    kcm_log(0, "ntlm %s request processed for %s\\%s flags: %s",
	    type, domain, c->user, flagname);

 error:
    HEIMDAL_MUTEX_unlock(&cred_mutex);

    krb5_data_free(&cb);
    krb5_data_free(&type1data);
    krb5_data_free(&type2data);
    if (type3.lm.data)
	free(type3.lm.data);
    if (type3.ntlm.data)
	free(type3.ntlm.data);
    if (type3.sessionkey.data)
	free(type3.sessionkey.data);
    if (targetname)
	free(targetname);
    heim_ntlm_free_type2(&type2);
    heim_ntlm_free_buf(&sessionkey);
    heim_ntlm_free_buf(&tidata);
    free(user);


    return output;
#else
    return NULL;
#endif
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

    CFDictionarySetValue(schema, kHEIMAttrData, CFSTR("d"));
    CFDictionarySetValue(schema, kHEIMAttrNTLMUsername, CFSTR("Rs"));
    CFDictionarySetValue(schema, kHEIMAttrNTLMDomain, CFSTR("Rs"));
    CFDictionarySetValue(schema, kHEIMAttrTransient, CFSTR("b"));
    CFDictionarySetValue(schema, kHEIMAttrAllowedDomain, CFSTR("as"));
    CFDictionarySetValue(schema, kHEIMAttrStatus, CFSTR("n"));

    CFSetAddValue(set, schema);
    CFRelease(schema);

    _HeimCredRegisterMech(kHEIMTypeNTLM, set, NULL, NTLMAuthCallback);
    CFRelease(set);
#endif /* ENABLE_NTLM */
}
