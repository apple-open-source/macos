/*
 * Copyright (c) 2009 Kungliga Tekniska Högskolan
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

#define HEIMDAL_PRINTF_ATTRIBUTE(x)

#include "mech_locl.h"
#include "heim_threads.h"
#include "heimbase.h"

#include <asl.h>
#include <CoreFoundation/CoreFoundation.h>
#include <krb5.h>

struct mg_thread_ctx {
    gss_OID mech;
    OM_uint32 min_stat;
    gss_buffer_desc min_error;
    aslclient asl;
    aslmsg msg;
};

static HEIMDAL_MUTEX context_mutex = HEIMDAL_MUTEX_INITIALIZER;
static int created_key;
static HEIMDAL_thread_key context_key;


static void
destroy_context(void *ptr)
{
    struct mg_thread_ctx *mg = ptr;
    OM_uint32 junk;

    if (mg == NULL)
	return;

    gss_release_buffer(&junk, &mg->min_error);

    if (mg->msg)
	asl_free(mg->msg);
    if (mg->asl)
	asl_close(mg->asl);

    free(mg);
}


static struct mg_thread_ctx *
_gss_mechglue_thread(void)
{
    struct mg_thread_ctx *ctx;
    int ret = 0;

    HEIMDAL_MUTEX_lock(&context_mutex);

    if (!created_key) {
	HEIMDAL_key_create(&context_key, destroy_context, ret);
	if (ret) {
	    HEIMDAL_MUTEX_unlock(&context_mutex);
	    return NULL;
	}
	created_key = 1;
    }
    HEIMDAL_MUTEX_unlock(&context_mutex);

    ctx = HEIMDAL_getspecific(context_key);
    if (ctx == NULL) {

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL)
	    return NULL;
	HEIMDAL_setspecific(context_key, ctx, ret);
	if (ret) {
	    free(ctx);
	    return NULL;
	}

	ctx->asl = asl_open(getprogname(), NULL, 0);
	ctx->msg = asl_new(ASL_TYPE_MSG);
	asl_set(ctx->msg, "org.h5l.asl", "gssapi");
    }
    return ctx;
}

OM_uint32
_gss_mg_get_error(const gss_OID mech, OM_uint32 value, gss_buffer_t string)
{
    struct mg_thread_ctx *mg;

    mg = _gss_mechglue_thread();
    if (mg == NULL)
	return GSS_S_BAD_STATUS;

    if (value != mg->min_stat || mg->min_error.length == 0) {
	_mg_buffer_zero(string);
	return GSS_S_BAD_STATUS;
    }
    string->value = malloc(mg->min_error.length);
    if (string->value == NULL) {
	_mg_buffer_zero(string);
	return GSS_S_FAILURE;
    }
    string->length = mg->min_error.length;
    memcpy(string->value, mg->min_error.value, mg->min_error.length);
    return GSS_S_COMPLETE;
}

void
_gss_mg_error(gssapi_mech_interface m, OM_uint32 min)
{
    OM_uint32 major_status, minor_status;
    OM_uint32 message_content = 0;
    struct mg_thread_ctx *mg;

    /*
     * Mechs without gss_display_status() does
     * gss_mg_collect_error() by themself.
     */
    if (m->gm_display_status == NULL)
	return ;

    mg = _gss_mechglue_thread();
    if (mg == NULL)
	return;

    gss_release_buffer(&minor_status, &mg->min_error);

    mg->mech = &m->gm_mech_oid;
    mg->min_stat = min;

    major_status = m->gm_display_status(&minor_status,
					min,
					GSS_C_MECH_CODE,
					&m->gm_mech_oid,
					&message_content,
					&mg->min_error);
    if (major_status != GSS_S_COMPLETE) {
	_mg_buffer_zero(&mg->min_error);
    } else {
	_gss_mg_log(5, "_gss_mg_error: captured %.*s (%d) from underlaying mech %s",
		    (int)mg->min_error.length, (const char *)mg->min_error.value,
		    (int)min, m->gm_name);
    }
}

void
gss_mg_collect_error(gss_OID mech, OM_uint32 maj, OM_uint32 min)
{
    gssapi_mech_interface m = __gss_get_mechanism(mech);
    if (m == NULL)
	return;
    _gss_mg_error(m, min);
}

OM_uint32
gss_mg_set_error_string(gss_OID mech,
			OM_uint32 maj, OM_uint32 min,
			const char *fmt, ...)
    HEIMDAL_PRINTF_ATTRIBUTE((printf, 4, 5))
{
    struct mg_thread_ctx *mg;
    char *str = NULL;
    OM_uint32 junk;
    va_list ap;
    
    mg = _gss_mechglue_thread();
    if (mg == NULL)
	return maj;

    va_start(ap, fmt);
    vasprintf(&str, fmt, ap);
    va_end(ap);

    if (str) {
	gss_release_buffer(&junk, &mg->min_error);

	mg->mech = mech;
	mg->min_stat = min;

	mg->min_error.value = str;
	mg->min_error.length = strlen(str);

	_gss_mg_log(5, "gss_mg_set_error_string: %.*s (%d/%d)",
		    (int)mg->min_error.length, (const char *)mg->min_error.value,
		    (int)maj, (int)min);
    }
    return maj;
}

#ifdef __APPLE__

#include <CoreFoundation/CoreFoundation.h>

CFErrorRef
_gss_mg_cferror(OM_uint32 major_status,
		OM_uint32 minor_status,
		gss_const_OID mech)
{
    struct mg_thread_ctx *mg;
    CFErrorRef e;
#define NUM_ERROR_DESC 5
    void const *keys[NUM_ERROR_DESC] = { 
	CFSTR("kGSSMajorErrorCode"),
	CFSTR("kGSSMinorErrorCode"),
	CFSTR("kGSSMechanismOID"),
	CFSTR("kGSSMechanism"),
	kCFErrorDescriptionKey
    };
    void const *values[NUM_ERROR_DESC] = { 0 };
    gss_buffer_desc oid;
    const char *name;
    OM_uint32 junk;
    size_t n;

    values[0] = CFNumberCreate(NULL, kCFNumberSInt32Type, &major_status);
    values[1] = CFNumberCreate(NULL, kCFNumberSInt32Type, &minor_status);

    if (mech && gss_oid_to_str(&junk, (gss_OID)mech, &oid) == GSS_S_COMPLETE) {
	values[2] = CFStringCreateWithFormat(NULL, NULL, CFSTR("%.*s"), (int)oid.length, (char *)oid.value);
	gss_release_buffer(&junk, &oid);
    } else {
	values[2] = CFStringCreateWithFormat(NULL, NULL, CFSTR("no-mech"));
    }

    if (mech && (name = gss_oid_to_name(mech)) != NULL) {
	values[3] = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), name);;
    }  else {
	values[3] = CFStringCreateWithFormat(NULL, NULL, CFSTR("no mech given"));
    }

    mg = _gss_mechglue_thread();
    if (mg && minor_status == mg->min_stat && mg->min_error.length != 0) {
	values[4] = CFStringCreateWithFormat(NULL, NULL, CFSTR("%.*s"),
					       (int)mg->min_error.length,
					       mg->min_error.value);
    } else {
	values[4] = CFStringCreateWithFormat(NULL, NULL, CFSTR("Unknown minor status: %d"), (int)minor_status);
    }

    e = CFErrorCreateWithUserInfoKeysAndValues(NULL,
					       CFSTR("org.h5l.GSS"),
					       (CFIndex)major_status,
					       keys,
					       values,
					       NUM_ERROR_DESC);
    for (n = 0; n < sizeof(values) / sizeof(values[0]); n++)
	CFRelease(values[n]);
    
    return e;
}



static CFTypeRef
CopyKeyFromFile(CFStringRef domain, CFStringRef key)
{
    CFReadStreamRef s;
    CFDictionaryRef d;
    CFStringRef file;
    CFErrorRef e;
    CFURLRef url;
    CFTypeRef val;
    
    file = CFStringCreateWithFormat(NULL, 0, CFSTR("/Library/Preferences/%@.plist"), domain);
    if (file == NULL)
	return NULL;
    
    url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, file, kCFURLPOSIXPathStyle, false);
    CFRelease(file);
    if (url == NULL)
	return NULL;
    
    s = CFReadStreamCreateWithFile(kCFAllocatorDefault, url);
    CFRelease(url);
    if (s == NULL)
	return NULL;
    
    if (!CFReadStreamOpen(s)) {
	CFRelease(s);
	return NULL;
    }
    
    d = (CFDictionaryRef)CFPropertyListCreateWithStream (kCFAllocatorDefault, s, 0, kCFPropertyListImmutable, NULL, &e);
    CFRelease(s);
    if (d == NULL)
	return NULL;
    
    if (CFGetTypeID(d) != CFDictionaryGetTypeID()) {
	CFRelease(d);
	return NULL;
    }
    
    val = CFDictionaryGetValue(d, key);
    if (val)
	CFRetain(val);
    CFRelease(d);
    return val;
}


CFTypeRef
_gss_mg_copy_key(CFStringRef domain, CFStringRef key)
{
    CFTypeRef val;

    /*
     * First prefer system file, then user copy if we are allowed to
     * touch user home directory.
     */

    val = CopyKeyFromFile(domain, key);

    if (val == NULL && krb5_homedir_access(NULL)) {
	val = CFPreferencesCopyAppValue(key, domain);
	if (val == NULL)
	    val = CFPreferencesCopyValue(key, domain, kCFPreferencesAnyUser, kCFPreferencesAnyHost);
    }
    return val;
}

#endif

static int log_level = 1;
static int asl_level = ASL_LEVEL_DEBUG;

static void
init_log(void *ptr)
{
    CFTypeRef val;
    
    val = _gss_mg_copy_key(CFSTR("com.apple.GSS"), CFSTR("DebugLevel"));
    if (val == NULL)
	return;
    
    if (CFGetTypeID(val) == CFBooleanGetTypeID())
	log_level = CFBooleanGetValue(val) ? 1 : 0;
    else if (CFGetTypeID(val) == CFNumberGetTypeID())
	CFNumberGetValue(val, kCFNumberIntType, &log_level);
    else
	/* ignore other types */;

    CFRelease(val);

    if (log_level > 0)
	asl_level = ASL_LEVEL_NOTICE;
}

int
_gss_mg_log_level(int level)
{
    static heim_base_once_t once = HEIM_BASE_ONCE_INIT;

    heim_base_once_f(&once, NULL, init_log);

    return (level > log_level) ? 0 : 1;
}

void
_gss_mg_log(int level, const char *fmt, ...)
    HEIMDAL_PRINTF_ATTRIBUTE((printf, 2, 3))
{
    struct mg_thread_ctx *mg;
    va_list ap;

    if (!_gss_mg_log_level(level))
	return;

    mg = _gss_mechglue_thread();
    if (mg == NULL)
	return;

    va_start(ap, fmt);
    asl_vlog(mg->asl, mg->msg, asl_level, fmt, ap);
    va_end(ap);
}

void
_gss_mg_log_name(int level, struct _gss_name *name, gss_OID mech_type, const char *fmt, ...)
    HEIMDAL_PRINTF_ATTRIBUTE((printf, 4, 5))
{
    struct _gss_mechanism_name *mn = NULL;
    gssapi_mech_interface m;
    OM_uint32 junk;

    if (!_gss_mg_log_level(level))
        return;

    m = __gss_get_mechanism(mech_type);
    if (m == NULL)
        return;

    if (_gss_find_mn(&junk, name, mech_type, &mn) == GSS_S_COMPLETE) {
	OM_uint32 maj_stat = GSS_S_COMPLETE;
	gss_buffer_desc namebuf;

	if (mn == NULL) {
	    namebuf.value = "no name";
	    namebuf.length = strlen((char *)namebuf.value);
	} else {
	    maj_stat = m->gm_display_name(&junk, mn->gmn_name,
					  &namebuf, NULL);
	}
	if (maj_stat == GSS_S_COMPLETE) {
	    char *str = NULL;
	    va_list ap;

	    va_start(ap, fmt);
	    vasprintf(&str, fmt, ap);
	    va_end(ap);

	    if (str)
	        _gss_mg_log(level, "%s %.*s", str,
			    (int)namebuf.length, (char *)namebuf.value);
	    free(str);
	    if (mn != NULL)
		gss_release_buffer(&junk, &namebuf);
	}
    }

}

void
_gss_mg_log_cred(int level, struct _gss_cred *cred, const char *fmt, ...)
    HEIMDAL_PRINTF_ATTRIBUTE((printf, 3, 4))
{
    struct _gss_mechanism_cred *mc;
    char *str;
    va_list ap;

    if (!_gss_mg_log_level(level))
        return;

    va_start(ap, fmt);
    vasprintf(&str, fmt, ap);
    va_end(ap);

    if (cred) {
	HEIM_SLIST_FOREACH(mc, &cred->gc_mc, gmc_link) {
	    _gss_mg_log(1, "%s: %s", str, mc->gmc_mech->gm_name);
	}
    } else {
	_gss_mg_log(1, "%s: GSS_C_NO_CREDENTIAL", str);
    }
    free(str);
}

static const char *paths[] = {
    "/Library/KerberosPlugins/GSSAPI",
    "/System/Library/KerberosPlugins/GSSAPI",
    NULL
};

static void
load_plugins(void *ptr)
{
    krb5_context context;
    if (krb5_init_context(&context))
	return;
    krb5_load_plugins(context, "gss", paths);
    krb5_free_context(context);
}
	

void
_gss_load_plugins(void)
{
    static heim_base_once_t once = HEIM_BASE_ONCE_INIT;
    heim_base_once_f(&once, NULL, load_plugins);
}
