/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 - 2010 Apple Inc. All rights reserved.
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
 * 3. Neither the name of KTH nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KTH AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KTH OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <config.h>

#include <stdio.h>
#include <gssapi.h>
#include <gssapi_krb5.h>
#include <gssapi_spnego.h>
#include <gssapi_ntlm.h>
#include <gssapi_oid.h>
#include <gssapi_spi.h>
#include <heim-ipc.h>
#include <err.h>
#include <roken.h>
#include <getarg.h>
#include <rtbl.h>
#include <hex.h>
#include <gss-commands.h>
#include <krb5.h>
#ifdef __APPLE__
#include <heimcred.h>
#endif
#include <parse_time.h>

#include "crypto-headers.h"

static void usage (int ret) __attribute__((noreturn));


static int version_flag = 0;
static int help_flag	= 0;

static struct getargs args[] = {
    {"version",	0,	arg_flag,	&version_flag, "print version", NULL },
    {"help",	0,	arg_flag,	&help_flag,  NULL, NULL }
};

static void
usage (int ret)
{
    arg_printusage (args, sizeof(args)/sizeof(*args),
		    NULL, "service@host");
    exit (ret);
}

#define COL_DESC	"Description"
#define COL_ENABLED	"Enabled"
#define COL_EXPIRE	"Expire"
#define COL_MECH	"Mech"
#define COL_NAME	"Name"
#define COL_OID		"OID"
#define COL_OPTION	"Option"
#define COL_SASL	"SASL"
#define COL_VALUE	"Value"
#define COL_UUID	"UUID"

int
supported_mechanisms(struct supported_mechanisms_options *opt, int argc, char **argv)
{
    OM_uint32 maj_stat, min_stat;
    gss_OID_set mechs;
    rtbl_t ct;
    size_t i;

    maj_stat = gss_indicate_mechs(&min_stat, &mechs);
    if (maj_stat != GSS_S_COMPLETE)
	errx(1, "gss_indicate_mechs failed");

    printf("Supported mechanisms:\n");

    ct = rtbl_create();
    if (ct == NULL)
	errx(1, "rtbl_create");

    rtbl_set_separator(ct, "  ");
    rtbl_add_column(ct, COL_OID, 0);
    rtbl_add_column(ct, COL_NAME, 0);
    rtbl_add_column(ct, COL_SASL, 0);
    if (opt->options_flag) {
	rtbl_add_column(ct, COL_OPTION, 0);
	rtbl_add_column(ct, COL_ENABLED, 0);
    }

    for (i = 0; i < mechs->count; i++) {
	gss_buffer_desc str;
	const char *name = NULL;

	maj_stat = gss_oid_to_str(&min_stat, &mechs->elements[i], &str);
	if (maj_stat != GSS_S_COMPLETE)
	    errx(1, "gss_oid_to_str failed");

	rtbl_add_column_entryv(ct, COL_OID, "%.*s",
			       (int)str.length, (char *)str.value);
	gss_release_buffer(&min_stat, &str);

	name = gss_oid_to_name(&mechs->elements[i]);
	if (name)
	    rtbl_add_column_entry(ct, COL_NAME, name);
	else
	    rtbl_add_column_entry(ct, COL_NAME, "");

	maj_stat = gss_inquire_saslname_for_mech(&min_stat, &mechs->elements[i], &str, NULL, NULL);
	if (maj_stat != GSS_S_COMPLETE)
	    errx(1, "gss_inquire_saslname_for_mech failed");
	rtbl_add_column_entryv(ct, COL_SASL, "%.*s",
			       (int)str.length, (char *)str.value);
	gss_release_buffer(&min_stat, &str);

	if (opt->options_flag) {
	    gss_OID_set options = GSS_C_NO_OID_SET;
	    gss_buffer_desc oname;
	    size_t n;
	    int ena;

	    gss_mo_list(&mechs->elements[i], &options);
	    
	    rtbl_add_column_entry(ct, COL_OPTION, "");
	    rtbl_add_column_entry(ct, COL_ENABLED, "");

	    for (n = 0; options && n < options->count; n++) {
		maj_stat = gss_mo_name(&mechs->elements[i], &options->elements[n], &oname);
		if (maj_stat != GSS_S_COMPLETE)
		    continue;

		rtbl_add_column_entry(ct, COL_OID, "");
		rtbl_add_column_entry(ct, COL_NAME, "");
		rtbl_add_column_entry(ct, COL_SASL, "");

		ena = gss_mo_get(&mechs->elements[i], &options->elements[n], NULL);

		rtbl_add_column_entryv(ct, COL_OPTION, "%.*s", (int)oname.length, (char *)oname.value);
		rtbl_add_column_entry(ct, COL_ENABLED, ena ? "yes" : "no");
		gss_release_buffer(&min_stat, &oname);
	    }
	}
    }
    gss_release_oid_set(&min_stat, &mechs);

    rtbl_format(ct, stdout);
    rtbl_destroy(ct);

    return 0;
}

int
acquire_credential(struct acquire_credential_options *opt, int argc, char **argv)
{
    char password[512];
    OM_uint32 maj_stat, min_stat;
    gss_const_OID mech = NULL;
    gss_OID nametype = GSS_C_NT_USER_NAME;
    gss_name_t name = GSS_C_NO_NAME;
    gss_buffer_desc buffer;
    gss_cred_id_t cred = GSS_C_NO_CREDENTIAL;
    CFMutableDictionaryRef attributes;
    CFStringRef pw;
    CFErrorRef error = NULL;

    /*
     * mech
     */

    attributes = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
					   &kCFTypeDictionaryKeyCallBacks,
					   &kCFTypeDictionaryValueCallBacks);
    if (attributes == NULL)
	errx(1, "out of memory");

    if (opt->mech_string) {
	mech = gss_name_to_oid(opt->mech_string);
	if (mech == NULL)
	    errx(1, "No such mech: %s", opt->mech_string);
    } else {
	mech = GSS_KRB5_MECHANISM;
    }

    /*
     * user
     */

    if (opt->user_string == NULL && argc < 1)
	errx(1, "no user string");

    if (opt->user_string) {
	buffer.value = rk_UNCONST(opt->user_string);
	buffer.length = strlen(opt->user_string);
    } else {
	buffer.value = argv[0];
	buffer.length = strlen(argv[0]);
    }

    maj_stat = gss_import_name(&min_stat, &buffer, nametype, &name);
    if (maj_stat)
	errx(1, "failed to import name");


    if (opt->certificate_persistant_string) {
	size_t slen = strlen(opt->certificate_persistant_string);
	CFMutableDataRef data = CFDataCreateMutable(NULL, slen);
	ssize_t dlen;

	CFDataSetLength(data, strlen);

	dlen = hex_decode(opt->certificate_persistant_string,
			  CFDataGetMutableBytePtr(data),
			  slen);
	if (dlen < 0)
	    errx(1, "failed to hex decode reference");
	
	CFDataSetLength(data, dlen);

	CFDictionarySetValue(attributes, kGSSICCertificate, data);
	CFRelease(data);

    } else {

	/*
	 * password
	 */

	if (UI_UTIL_read_pw_string(password, sizeof(password),
				   "Password: ", 0) != 0)
	    errx(1, "failed reading password");

	pw = CFStringCreateWithCString(kCFAllocatorDefault, password, kCFStringEncodingUTF8);
	CFDictionarySetValue(attributes, kGSSICPassword, pw);
	CFRelease(pw);
    }

    if (opt->validate_flag)
	CFDictionarySetValue(attributes, kGSSICVerifyCredential, kCFBooleanTrue);
    if (opt->kdc_hostname_string) {
	CFStringRef hn = CFStringCreateWithCString(NULL, opt->kdc_hostname_string, kCFStringEncodingUTF8);
	if (hn == NULL)
	    errx(1, "CFStringCreateWithCString");
	CFDictionarySetValue(attributes, kGSSICLKDCHostname, hn);
	CFRelease(hn);
    }

    maj_stat = gss_aapl_initial_cred(name,
				     mech,
				     attributes,
				     &cred,
				     &error);
    if (maj_stat != GSS_S_COMPLETE) {
	char *msg = NULL;
	if (error) {
	    CFStringRef m;
	    m = CFErrorCopyDescription(error);
	    if (m) {
		msg = rk_cfstring2cstring(m);
		CFRelease(m);
	    }
	}
	errx(1, "gss_aapl_initial_cred: %s: %d", 
	     msg ? msg : "", (int)maj_stat);
    }
    gss_release_cred(&min_stat, &cred);
    gss_release_name(&min_stat, &name);

    CFRelease(attributes);

    if (error)
	CFRelease(error);

    return 0;
}


struct print_cred {
    rtbl_t t;
};

static void
print_cred(void *ctx, gss_const_OID oid, gss_cred_id_t cred)
{
    struct print_cred *pc = ctx;
    gss_buffer_set_t data_set;
    OM_uint32 major, junk;
    gss_buffer_desc buffer;
    gss_name_t name;
    const char *str;
    OM_uint32 expire;

    if (cred == NULL)
	return;

    major = gss_inquire_cred(&junk, cred, NULL, &expire, NULL, NULL);
    if (major == GSS_S_CREDENTIALS_EXPIRED)
	expire = 0;
    else if (major)
	goto out;
    major = gss_inquire_cred_by_mech(&junk, cred, (gss_OID)oid, &name, NULL, NULL, NULL); 
    if (major) goto out;
    major = gss_display_name(&junk, name, &buffer, NULL);
    gss_release_name(&junk, &name);
    if (major) goto out;
    
    rtbl_add_column_entryv(pc->t, COL_NAME, "%.*s",
			   (int)buffer.length, (char *)buffer.value);
    
    gss_release_buffer(&junk, &buffer);
    
    str = gss_oid_to_name(oid);
    if (str)
	rtbl_add_column_entry(pc->t, COL_MECH, str);
    
    if (expire == GSS_C_INDEFINITE)
	rtbl_add_column_entryv(pc->t, COL_EXPIRE, "never");
    else if (expire == 0)
	rtbl_add_column_entryv(pc->t, COL_EXPIRE, "expired");
    else {
	char life[80];
	unparse_time_approx(expire, life, sizeof(life));
	rtbl_add_column_entry(pc->t, COL_EXPIRE, life);
    }

    major = gss_inquire_cred_by_oid(&junk, cred, GSS_C_NT_UUID, &data_set);
    if (major == GSS_S_COMPLETE && data_set->count == 1)
	rtbl_add_column_entryv(pc->t, COL_UUID, "%.*s\n", (int)data_set->elements[0].length, (const char *)data_set->elements[0].value);
    else
	rtbl_add_column_entry(pc->t, COL_UUID, "");

    gss_release_buffer_set(&junk, &data_set);

 out:
    gss_release_cred(&junk, &cred);
}

static void
diag_cred(void *ctx, gss_const_OID oid, gss_cred_id_t cred)
{
    const char *delim = "----------------";
    gss_buffer_set_t data_set;
    gss_buffer_desc buffer;
    OM_uint32 major, junk;
    gss_name_t name;
    const char *mech;
    size_t n;

    if (cred == NULL)
	return;

    major = gss_inquire_cred_by_mech(&junk, cred, (gss_OID)oid, &name, NULL, NULL, NULL);
    if (major)
	return;

    major = gss_display_name(&junk, name, &buffer, NULL);
    gss_release_name(&junk, &name);
    if (major)
	return;
    
    mech = gss_oid_to_name(oid);

    printf("@GSSCred\n%s\n%s: %.*s\n",
	   delim,
	   mech ? mech : "<unknown-mech>",
	   (int)buffer.length, buffer.value);
    gss_release_buffer(&junk, &buffer);

    major = gss_inquire_cred_by_oid(&junk, cred, GSS_C_CRED_DIAG, &data_set);
    if (major)
	return;

    for (n = 0; n < data_set->count; n++)
	printf("%s\n%.*s\n", delim, (int)data_set->elements[n].length, data_set->elements[n].value);

    printf("%s\n", delim);

    gss_release_buffer_set(&junk, &data_set);

    major = gss_inquire_cred_by_oid(&junk, cred, GSS_C_NT_UUID, &data_set);
    if (major == GSS_S_COMPLETE || data_set->count == 1) {
	printf("UUID: %.*s\n", (int)data_set->elements[0].length, (const char *)data_set->elements[0].value);
	printf("%s\n", delim);
    }
    gss_release_buffer_set(&junk, &data_set);


}



int
list_credentials(struct list_credentials_options *opt, int argc, char **argv)
{
    struct print_cred pc;
    gss_const_OID mech = NULL;

    if (opt->mech_string) {
	mech = gss_name_to_oid(opt->mech_string);
	if (mech == NULL)
	    errx(1, "No such mech: %s", opt->mech_string);
    }

    if (opt->verbose_flag) {

	gss_iter_creds_f(NULL, 0, mech, NULL, diag_cred);

    } else {

	pc.t = rtbl_create();
	if (pc.t == NULL)
	    errx(1, "rtbl_create");
	
	rtbl_set_separator(pc.t, "  ");
	rtbl_add_column(pc.t, COL_NAME, 0);
	rtbl_add_column(pc.t, COL_EXPIRE, 0);
	rtbl_add_column(pc.t, COL_MECH, 0);
	rtbl_add_column(pc.t, COL_UUID, 0);
	
	gss_iter_creds_f(NULL, 0, mech, &pc, print_cred);
	
	rtbl_format(pc.t, stdout);
	rtbl_destroy(pc.t);
    }

    return 0;
}

static gss_cred_id_t
acquire_cred(const char *name_string, gss_const_OID mech, gss_const_OID nametype)
{
    OM_uint32 maj_stat, min_stat;
    gss_OID_set mechset = NULL;
    gss_cred_id_t cred = NULL;
    gss_buffer_desc buffer;
    gss_name_t name;

    buffer.value = rk_UNCONST(name_string);
    buffer.length = strlen(name_string);

    maj_stat = gss_import_name(&min_stat, &buffer, nametype, &name);
    if (maj_stat)
	errx(1, "failed to import name");

    if (mech) {
	gss_create_empty_oid_set(&min_stat, &mechset);
	gss_add_oid_set_member(&min_stat, mech, &mechset);
    }
	
    maj_stat = gss_acquire_cred(&min_stat, name, GSS_C_INDEFINITE,
				mechset, GSS_C_INITIATE,
				&cred, NULL, NULL);
    gss_release_name(&min_stat, &name);
    gss_release_oid_set(&min_stat, &mechset);
    if (maj_stat || cred == NULL)
	errx(1, "acquire_cred failed");
    
    return cred;
}

static void
destroy_cred(void *arg1, gss_const_OID oid, gss_cred_id_t cred)
{
    gss_destroy_cred(NULL, &cred);
}

int
destroy(struct destroy_options *opt, int argc, char **argv)
{
    gss_const_OID mech = NULL;

    if (opt->mech_string) {
	mech = gss_name_to_oid(opt->mech_string);
	if (mech == NULL)
	    errx(1, "No such mech: %s", opt->mech_string);
    }

    if (opt->all_flag) {
	gss_iter_creds_f(NULL, 0, mech, NULL, destroy_cred);
    } else {
	gss_cred_id_t cred;

	if (argc < 1) {
	    printf("%s: missing name\n", getprogname());
	    return 1;
	}

	cred = acquire_cred(argv[0], mech, GSS_C_NT_USER_NAME);

	gss_destroy_cred(NULL, &cred);

    }

    return 0;
}

/*
 *
 */

static int
common_hold(OM_uint32 (*func)(OM_uint32 *, gss_cred_id_t),
	    const char *mech_string, int argc, char **argv)
{
    OM_uint32 min_stat, maj_stat;
    gss_const_OID mech = GSS_C_NO_OID;
    gss_cred_id_t cred;

    if (argc < 1) {
	printf("missing username to (un)hold\n");
	return 1;
    }
    
    if (mech_string) {
	mech = gss_name_to_oid(mech_string);
	if (mech == NULL)
	    errx(1, "No such mech: %s", mech_string);
    }

    cred = acquire_cred(argv[0], mech, GSS_C_NT_USER_NAME);

    maj_stat = func(&min_stat, cred);
    if (maj_stat != GSS_S_COMPLETE)
	errx(1, "(un)hold failed");

    gss_release_cred(&min_stat, &cred);

    return 0;
}

int
hold(struct hold_options *opt, int argc, char **argv)
{
    return common_hold(gss_cred_hold, opt->mech_string, argc, argv);
}

int
unhold(struct unhold_options *opt, int argc, char **argv)
{
    return common_hold(gss_cred_unhold, opt->mech_string, argc, argv);
}

int
get_label(struct get_label_options *opt, int argc, char **argv)
{
    OM_uint32 min_stat, maj_stat;
    gss_const_OID mech = GSS_C_NO_OID;
    gss_cred_id_t cred;
    gss_buffer_desc buf;

    if (opt->mech_string) {
	mech = gss_name_to_oid(opt->mech_string);
	if (mech == NULL)
	    errx(1, "No such mech: %s", opt->mech_string);
    }

    cred = acquire_cred(argv[0], mech, GSS_C_NT_USER_NAME);

    maj_stat = gss_cred_label_get(&min_stat, cred, argv[1], &buf);
    if (maj_stat != GSS_S_COMPLETE)
	errx(1, "label get failed");

    printf("value: %.*s\n", (int)buf.length, (char *)buf.value);

    gss_release_buffer(&min_stat, &buf);
    gss_release_cred(&min_stat, &cred);

    return 0;
}

int
set_label(struct set_label_options *opt, int argc, char **argv)
{
    OM_uint32 min_stat, maj_stat;
    gss_const_OID mech = GSS_C_NO_OID;
    gss_cred_id_t cred;
    gss_buffer_desc buf;
    gss_buffer_t bufp = NULL;

    if (opt->mech_string) {
	mech = gss_name_to_oid(opt->mech_string);
	if (mech == NULL)
	    errx(1, "No such mech: %s", opt->mech_string);
    }

    cred = acquire_cred(argv[0], mech, GSS_C_NT_USER_NAME);

    if (argc > 2) {
	buf.value = argv[2];
	buf.length = strlen(argv[2]);
	bufp = &buf;
    }

    maj_stat = gss_cred_label_set(&min_stat, cred, argv[1], bufp);
    if (maj_stat != GSS_S_COMPLETE)
	errx(1, "label get failed");

    gss_release_cred(&min_stat, &cred);

    return 0;
}

/*
 *
 */

static void
print_mech_attr(const char *mechname, gss_const_OID mech, gss_OID_set set)
{
    gss_buffer_desc name, desc;
    OM_uint32 major, minor;
    rtbl_t ct;
    size_t n;

    ct = rtbl_create();
    if (ct == NULL)
	errx(1, "rtbl_create");

    rtbl_set_separator(ct, "  ");
    rtbl_add_column(ct, COL_OID, 0);
    rtbl_add_column(ct, COL_DESC, 0);
    if (mech)
	rtbl_add_column(ct, COL_VALUE, 0);

    for (n = 0; n < set->count; n++) {
	major = gss_display_mech_attr(&minor, &set->elements[n], &name, &desc, NULL);
	if (major)
	    continue;

	rtbl_add_column_entryv(ct, COL_OID, "%.*s",
			       (int)name.length, (char *)name.value);
	rtbl_add_column_entryv(ct, COL_DESC, "%.*s",
			       (int)desc.length, (char *)desc.value);
	if (mech) {
	    gss_buffer_desc value;

	    if (gss_mo_get(mech, &set->elements[n], &value) != 0)
		value.length = 0;

	    if (value.length)
		rtbl_add_column_entryv(ct, COL_VALUE, "%.*s",
				       (int)value.length, (char *)value.value);
	    else
		rtbl_add_column_entryv(ct, COL_VALUE, "<>");
	    gss_release_buffer(&minor, &value);
	}

	gss_release_buffer(&minor, &name);
	gss_release_buffer(&minor, &desc);
    }

    printf("attributes for: %s\n", mechname);
    rtbl_format(ct, stdout);
    rtbl_destroy(ct);
}

int
attrs_for_mech(struct attrs_for_mech_options *opt, int argc, char **argv)
{
    gss_OID_set mech_attr = NULL, known_mech_attrs = NULL;
    gss_const_OID mech = GSS_C_NO_OID;
    OM_uint32 major, minor;

    if (opt->mech_string) {
	mech = gss_name_to_oid(opt->mech_string);
	if (mech == NULL)
	    errx(1, "mech %s is unknown", opt->mech_string);
    }

    major = gss_inquire_attrs_for_mech(&minor,
				       mech,
				       &mech_attr,
				       &known_mech_attrs);
    if (major)
	errx(1, "gss_inquire_attrs_for_mech");

    if (mech)
	print_mech_attr(opt->mech_string, mech, mech_attr);

    if (opt->all_flag)
	print_mech_attr("all mechs", NULL, known_mech_attrs);

    gss_release_oid_set(&minor, &mech_attr);
    gss_release_oid_set(&minor, &known_mech_attrs);

    return 0;
}

int
display_status(struct display_status_options *opt, int argc, char **argv)
{
    int status_type = GSS_C_GSS_CODE;
    gss_const_OID mech = GSS_C_NO_OID;

    if (opt->minor_status_flag)
	status_type = GSS_C_MECH_CODE;

    if (opt->mech_string) {
	mech = gss_name_to_oid(opt->mech_string);
	if (mech == NULL)
	    errx(1, "mech %s is unknown", opt->mech_string);
    }

    for (;argc; argc--, argv++) {
	OM_uint32 ret, new_stat, msg_ctx;
	gss_buffer_desc status_string;
	long num;

	num = atol(argv[0]);
	if (num == 0) {
	    warnx("failed to parse %s as a number", argv[0]);
	    continue;
	}

	msg_ctx = 0;
	do {
	    ret = gss_display_status(&new_stat,
				     (OM_uint32)num,
				     status_type,
				     (gss_OID)mech,
				     &msg_ctx,
				     &status_string);
	    if (!GSS_ERROR(ret)) {
		printf("%s: %.*s\n", argv[0],
		       (int)status_string.length,
		       (char *)status_string.value);
		gss_release_buffer(&new_stat, &status_string);
	    }
	} while (!GSS_ERROR(ret) && msg_ctx != 0);
	
    }
    return 0;
}

/*
 *
 */

int
credentials_status(struct credentials_status_options *opt, int argc, char **argv)
{
#ifdef __APPLE__
    CFDictionaryRef status = HeimCredCopyStatus(NULL);
    if (status) {
	CFDataRef data = CFPropertyListCreateData(NULL, status, kCFPropertyListXMLFormat_v1_0, 0, NULL);
	CFRelease(status);
	if (data == NULL)
	    return 1;
	printf("%.*s\n", (int)CFDataGetLength(data), CFDataGetBytePtr(data));
	CFRelease(data);
    } else {
	printf("no credentials to dump\n");
	return 1;
    }
#endif
    return 0;
}

/*
 *
 */

int
help(void *opt, int argc, char **argv)
{
    sl_slc_help(commands, argc, argv);
    return 0;
}

int
main(int argc, char **argv)
{
    int optidx = 0;

    setprogname(argv[0]);
    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optidx))
	usage(1);

    if (help_flag)
	usage (0);

    if(version_flag){
	print_version(NULL);
	exit(0);
    }

    argc -= optidx;
    argv += optidx;

    if (argc == 0) {
	help(NULL, argc, argv);
	return 1;
    }

    return sl_command (commands, argc, argv);
}
