/*
 * Copyright (c) 2010 - 2011 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2010 - 2011 Apple Inc. All rights reserved.
 *
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

#include "mech_locl.h"
#include <CoreFoundation/CoreFoundation.h>
#include <krb5.h>

static int
get_option_def(int def, gss_OID mech, gss_mo_desc *mo, gss_buffer_t value)
{
#ifdef __APPLE__
    CFStringRef domain, key;
    CFPropertyListRef val = NULL;
    const char *name;

    name = gss_oid_to_name(mech);

    domain = CFStringCreateWithFormat(NULL, 0, CFSTR("com.apple.GSS.%s"), name);
    if (domain == NULL)
	return def;
    key = CFStringCreateWithCString(NULL, mo->name,  kCFStringEncodingUTF8);
    if (key == NULL) {
	CFRelease(domain);
	return def;
    }
    
    val = _gss_mg_get_key(domain, key);
    CFRelease(domain);
    CFRelease(key);
    if (val == NULL)
	return def;

    if (CFGetTypeID(val) == CFBooleanGetTypeID()) {
	def = CFBooleanGetValue((CFBooleanRef)val);
    } else if (CFGetTypeID(val) == CFNumberGetTypeID()) {
	CFNumberGetValue((CFNumberRef)val, kCFNumberIntType, &def);
    } else if (CFGetTypeID(val) == CFDictionaryGetTypeID()) {
	CFDictionaryRef dict = (CFDictionaryRef)val;
	CFBooleanRef enable = (CFBooleanRef)CFDictionaryGetValue(dict, CFSTR("enable"));
	CFDataRef data = (CFDataRef)CFDictionaryGetValue(dict, CFSTR("data"));
	
	if (enable && CFGetTypeID(enable) == CFBooleanGetTypeID())
	    def = CFBooleanGetValue(enable);
	else if (enable && CFGetTypeID(enable) == CFNumberGetTypeID())
	    CFNumberGetValue((CFNumberRef)val, kCFNumberIntType, &def);

	if (data && CFGetTypeID(data) == CFDataGetTypeID()) {
	    value->value = malloc(CFDataGetLength(data));
	    if (value->value != NULL) {
		memcpy(value->value, CFDataGetBytePtr(data), value->length);
		value->length = CFDataGetLength(data);
	    }
	}
    }

    CFRelease(val);
#endif
    return def;
}


int
_gss_mo_get_option_1(gss_OID mech, gss_mo_desc *mo, gss_buffer_t value)
{
    return get_option_def(1, mech, mo, value);
}

int
_gss_mo_get_option_0(gss_OID mech, gss_mo_desc *mo, gss_buffer_t value)
{
    return get_option_def(0, mech, mo, value);
}

int
gss_mo_set(gss_OID mech, gss_OID option, int enable, gss_buffer_t value)
{
    gssapi_mech_interface m;
    size_t n;

    if ((m = __gss_get_mechanism(mech)) == NULL)
	return GSS_S_BAD_MECH;

    for (n = 0; n < m->gm_mo_num; n++)
	if (gss_oid_equal(option, m->gm_mo[n].option) && m->gm_mo[n].set)
	    return m->gm_mo[n].set(mech, &m->gm_mo[n], enable, value);
    return 0;
}

int
gss_mo_get(gss_OID mech, gss_OID option, gss_buffer_t value)
{
    gssapi_mech_interface m;
    size_t n;

    if (value)
	_mg_buffer_zero(value);

    if ((m = __gss_get_mechanism(mech)) == NULL)
	return 0;

    for (n = 0; n < m->gm_mo_num; n++)
	if (gss_oid_equal(option, m->gm_mo[n].option) && m->gm_mo[n].get)
	    return m->gm_mo[n].get(mech, &m->gm_mo[n], value);

    return 0;
}

void
gss_mo_list(gss_OID mech, gss_OID_set *options)
{
    gssapi_mech_interface m;
    OM_uint32 major, minor;
    size_t n;

    if (options == NULL)
	return;

    *options = GSS_C_NO_OID_SET;

    if ((m = __gss_get_mechanism(mech)) == NULL)
	return;

    major = gss_create_empty_oid_set(&minor, options);
    if (major != GSS_S_COMPLETE)
	return;

    for (n = 0; n < m->gm_mo_num; n++)
	gss_add_oid_set_member(&minor, m->gm_mo[n].option, options);
}

OM_uint32
gss_mo_name(gss_OID mech, gss_OID option, gss_buffer_t name)
{
    gssapi_mech_interface m;
    size_t n;

    if (name == NULL)
	return GSS_S_BAD_NAME;

    if ((m = __gss_get_mechanism(mech)) == NULL)
	return GSS_S_BAD_MECH;

    for (n = 0; n < m->gm_mo_num; n++) {
	if (gss_oid_equal(option, m->gm_mo[n].option)) {
	    name->value = strdup(m->gm_mo[n].name);
	    if (name->value == NULL)
		return GSS_S_BAD_NAME;
	    name->length = strlen(m->gm_mo[n].name);
	    return GSS_S_COMPLETE;
	}
    }
    return GSS_S_BAD_NAME;
}
