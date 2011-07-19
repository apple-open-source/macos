/*
 * Copyright (c) 2008-2010 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2008-2010 Apple Inc. All rights reserved.
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

#include "heim.h"
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include <dispatch/dispatch.h>

#include <CommonCrypto/CommonDigest.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Heimdal/gkrb5_err.h>
#include <Heimdal/wind_err.h>
#include <Heimdal/krb_err.h>
#include <Heimdal/hx509_err.h>

const char *__KerberosInternal_krb5_defkeyname = "FILE:/etc/krb5.keytab";

int krb5_use_broken_arcfour_string2key = 0;

static int do_log = 0;

static CFTypeRef
GetKeyFromFile(CFStringRef domain, CFStringRef key)
{
    CFReadStreamRef s;
    CFDictionaryRef d;
    CFStringRef file;
    CFErrorRef e;
    CFURLRef url;
    CFTypeRef val;
    
    file = CFStringCreateWithFormat(NULL, 0, CFSTR("/Library/Preferences/%s.plist"), domain);
    if (file == NULL)
	return NULL;
    
    url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, domain, kCFURLPOSIXPathStyle, false);
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

static void
init_log(void)
{
    static dispatch_once_t once = 0;
    dispatch_once(&once, ^{
	    CFBooleanRef b;
	    b = GetKeyFromFile(CFSTR("com.apple.MITKerberosShim"),
			       CFSTR("EnableDebugging"));
	    if (b && CFGetTypeID(b) == CFBooleanGetTypeID())
		do_log = CFBooleanGetValue(b);
	    if (b)
		CFRelease(b);
    });
}


void
mshim_log_entry(const char *msg, ...)
{
    init_log();

    if (do_log) {
	va_list ap;
	va_start(ap, msg);
	vsyslog(LOG_DEBUG, msg, ap);
	va_end(ap);
    }
}

int
mshim_failure(const char *func, int error, const char *subsystem)
{
    init_log();

    if (do_log && error)
	syslog(LOG_DEBUG, "%s failed with %d for '%s'", func, error, subsystem);
    return error;
}

void *
mshim_malloc(size_t size)
{
    void *ptr = malloc(size);
    if (ptr == NULL) {
	syslog(LOG_DEBUG, "mshim_malloc: can't allocate %d", (int)size);
	abort();
    }
    memset(ptr, 0, size);
    return ptr;
}

krb5_error_code
mshim_hdata2mdata(const krb5_data *h, mit_krb5_data *m)
{
    m->magic = MIT_KV5M_DATA;
    m->length = h->length;
    m->data = mshim_malloc(h->length);
    memcpy(m->data, h->data, h->length);
    return 0;
}


krb5_error_code
mshim_mdata2hdata(const mit_krb5_data *m, krb5_data *h)
{
    h->length = m->length;
    h->data = mshim_malloc(m->length);
    memcpy(h->data, m->data, m->length);
    return 0;
}

void
mshim_hkeyblock2mkeyblock(const krb5_keyblock *h, mit_krb5_keyblock *m)
{
    m->magic = MIT_KV5M_KEYBLOCK;
    m->enctype = h->keytype;
    m->length = h->keyvalue.length;
    m->contents = mshim_malloc(h->keyvalue.length);
    memcpy(m->contents, h->keyvalue.data, h->keyvalue.length);
}

void
mshim_mkeyblock2hkeyblock(const mit_krb5_keyblock *m, krb5_keyblock *h)
{
    h->keytype = m->enctype;
    h->keyvalue.length = m->length;
    h->keyvalue.data = mshim_malloc(h->keyvalue.length);
    memcpy(h->keyvalue.data, m->contents, h->keyvalue.length);
}


mit_krb5_error_code KRB5_CALLCONV
krb5_copy_keyblock_contents(mit_krb5_context context,
			    const mit_krb5_keyblock *from,
			    mit_krb5_keyblock *to)
{
    LOG_ENTRY();
    to->magic = MIT_KV5M_KEYBLOCK;
    to->enctype = from->enctype;
    to->length = from->length;
    to->contents = mshim_malloc(from->length);
    memcpy(to->contents, from->contents, from->length);
    return 0;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_copy_keyblock(mit_krb5_context context,
		   const mit_krb5_keyblock *from,
		   mit_krb5_keyblock **to)
{
    LOG_ENTRY();
    *to = mshim_malloc(sizeof(**to));
    return krb5_copy_keyblock_contents(context, from, *to);
}



void
mshim_mcred2hcred(krb5_context context, mit_krb5_creds *m, krb5_creds *h)
{
    struct comb_principal *p;
    memset(h, 0, sizeof(*h));

    p = (struct comb_principal *)m->client;
    if (p)
        heim_krb5_copy_principal(context, p->heim, &h->client);
    p = (struct comb_principal *)m->server;
    if (p)
        heim_krb5_copy_principal(context, p->heim, &h->server);

    h->session.keytype = m->keyblock.enctype;
    heim_krb5_data_copy(&h->session.keyvalue, m->keyblock.contents, m->keyblock.length);

    heim_krb5_data_copy(&h->ticket, m->ticket.data, m->ticket.length);

    h->times.authtime = m->times.authtime;
    h->times.starttime = m->times.starttime;
    h->times.endtime = m->times.endtime;
    h->times.renew_till = m->times.renew_till;

    h->flags.i = 0;
    if (m->ticket_flags & MIT_TKT_FLG_FORWARDABLE)
	h->flags.b.forwardable = 1;
    if (m->ticket_flags & MIT_TKT_FLG_FORWARDED)
	h->flags.b.forwarded = 1;
    if (m->ticket_flags & MIT_TKT_FLG_PROXIABLE)
	h->flags.b.proxiable = 1;
    if (m->ticket_flags & MIT_TKT_FLG_PROXY)
	h->flags.b.proxy = 1;
    if (m->ticket_flags & MIT_TKT_FLG_MAY_POSTDATE)
	h->flags.b.may_postdate = 1;
    if (m->ticket_flags & MIT_TKT_FLG_POSTDATED)
	h->flags.b.postdated = 1;
    if (m->ticket_flags & MIT_TKT_FLG_INVALID)
	h->flags.b.invalid = 1;
    if (m->ticket_flags & MIT_TKT_FLG_RENEWABLE)
	h->flags.b.renewable = 1;
    if (m->ticket_flags & MIT_TKT_FLG_INITIAL)
	h->flags.b.initial = 1;
    if (m->ticket_flags & MIT_TKT_FLG_PRE_AUTH)
	h->flags.b.pre_authent = 1;
    if (m->ticket_flags & MIT_TKT_FLG_HW_AUTH)
	h->flags.b.hw_authent = 1;
    if (m->ticket_flags & MIT_TKT_FLG_TRANSIT_POLICY_CHECKED)
	h->flags.b.transited_policy_checked = 1;
    if (m->ticket_flags & MIT_TKT_FLG_OK_AS_DELEGATE)
	h->flags.b.ok_as_delegate = 1;
    if (m->ticket_flags & MIT_TKT_FLG_ANONYMOUS)
	h->flags.b.anonymous = 1;

}

void
mshim_hcred2mcred(krb5_context context, krb5_creds *h, mit_krb5_creds *m)
{
    memset(m, 0, sizeof(*m));

    m->magic = MIT_KV5M_CREDS;
    if (h->client)
        m->client = mshim_hprinc2mprinc(context, h->client);
    if (h->server)
        m->server = mshim_hprinc2mprinc(context, h->server);

    mshim_hkeyblock2mkeyblock(&h->session, &m->keyblock);

    mshim_hdata2mdata(&h->ticket, &m->ticket);

    m->times.authtime = h->times.authtime;
    m->times.starttime = h->times.starttime;
    m->times.endtime = h->times.endtime;
    m->times.renew_till = h->times.renew_till;

    m->ticket_flags = 0;
    if (h->flags.b.forwardable)
	m->ticket_flags |= MIT_TKT_FLG_FORWARDABLE;
    if (h->flags.b.forwarded)
	m->ticket_flags |= MIT_TKT_FLG_FORWARDED;
    if (h->flags.b.proxiable)
	m->ticket_flags |= MIT_TKT_FLG_PROXIABLE;
    if (h->flags.b.proxy)
	m->ticket_flags |= MIT_TKT_FLG_PROXY;
    if (h->flags.b.may_postdate)
	m->ticket_flags |= MIT_TKT_FLG_MAY_POSTDATE;
    if (h->flags.b.postdated)
	m->ticket_flags |= MIT_TKT_FLG_POSTDATED;
    if (h->flags.b.invalid)
	m->ticket_flags |= MIT_TKT_FLG_INVALID;
    if (h->flags.b.renewable)
	m->ticket_flags |= MIT_TKT_FLG_RENEWABLE;
    if (h->flags.b.initial)
	m->ticket_flags |= MIT_TKT_FLG_INITIAL;
    if (h->flags.b.pre_authent)
	m->ticket_flags |= MIT_TKT_FLG_PRE_AUTH;
    if (h->flags.b.hw_authent)
	m->ticket_flags |= MIT_TKT_FLG_HW_AUTH;
    if (h->flags.b.transited_policy_checked)
	m->ticket_flags |= MIT_TKT_FLG_TRANSIT_POLICY_CHECKED;
    if (h->flags.b.ok_as_delegate)
	m->ticket_flags |= MIT_TKT_FLG_OK_AS_DELEGATE;
    if (h->flags.b.anonymous)
	m->ticket_flags |= MIT_TKT_FLG_ANONYMOUS;
}

void
mshim_haprepencpart2maprepencpart(const krb5_ap_rep_enc_part *h,
				  mit_krb5_ap_rep_enc_part *m)
{
    m->magic = MIT_KV5M_AP_REP_ENC_PART;
    m->ctime = h->ctime;
    m->cusec = h->cusec;

    if (h->subkey) {
	m->subkey = mshim_malloc(sizeof(*m->subkey));
	mshim_hkeyblock2mkeyblock(h->subkey, m->subkey);
    } else
	m->subkey = NULL;

    if (h->seq_number) {
	m->seq_number = *h->seq_number;
    } else 
	m->seq_number = 0;
}

void
mshim_hreplay2mreplay(const krb5_replay_data *h, mit_krb5_replay_data *m)
{
    m->timestamp = h->timestamp;
    m->usec = h->usec;
    m->seq = h->seq;
}


void KRB5_CALLCONV
krb5_free_ap_rep_enc_part(mit_krb5_context context,
			  mit_krb5_ap_rep_enc_part *enc_part)
{
    LOG_ENTRY();
    if (enc_part->subkey)
	krb5_free_keyblock(context, enc_part->subkey);
    free(enc_part);
}

void KRB5_CALLCONV
krb5_free_error(mit_krb5_context context, mit_krb5_error *error)
{
    LOG_ENTRY();
    krb5_free_principal(context, error->client);
    krb5_free_principal(context, error->server);
    krb5_free_data_contents(context, &error->text);
    krb5_free_data_contents(context, &error->e_data);
    free(error);
}

void
mshim_herror2merror(krb5_context context, const krb5_error *h, mit_krb5_error *m)
{
    LOG_ENTRY();
    memset(m, 0, sizeof(*m));

    m->magic = MIT_KV5M_ERROR;
    if (h->ctime)
	m->ctime = *h->ctime;
    if (h->cusec)
	m->cusec = *h->cusec;
    m->stime = h->stime;
    m->susec = h->susec;
#if 0
    m->client = mshim_hprinc2mprinc(context, h->client);
    m->server = mshim_hprinc2mprinc(context, h->server);
#endif
    m->error = h->error_code;
    if (h->e_text) {
	m->text.magic = MIT_KV5M_DATA;
	m->text.data = strdup(*(h->e_text));
	m->text.length = strlen(*(h->e_text));
    }
    if (h->e_data)
	mshim_hdata2mdata(h->e_data, &m->e_data);
#if 0
    krb5_principal client;		/* client's principal identifier;
					   optional */
    krb5_principal server;		/* server's principal identifier */
#endif
}

unsigned long
mshim_remap_flags(unsigned long in, const struct mshim_map_flags *table)
{
    unsigned long out = 0;
    while(table->in) {
	if (table->in & in)
	    out |= table->out;
	table++;
    }
    return out;
}


mit_krb5_error_code
krb5_init_context(mit_krb5_context *context)
{
    LOG_ENTRY();
    return heim_krb5_init_context((krb5_context *)context);
}

mit_krb5_error_code
krb5_init_secure_context(mit_krb5_context *context)
{
    LOG_ENTRY();
    return heim_krb5_init_context((krb5_context *)context);
}

void
krb5_free_context(mit_krb5_context context)
{
    LOG_ENTRY();
    heim_krb5_free_context(HC(context));
}

const char *
error_message(errcode_t code)
{
    static dispatch_once_t once = 0;
    static struct et_list *et_list = NULL;
    static char buffer[1024];
    const char *str;

    dispatch_once(&once, ^{
	initialize_asn1_error_table_r(&et_list);
	initialize_gk5_error_table_r(&et_list);
	initialize_wind_error_table_r(&et_list);
	initialize_krb5_error_table_r(&et_list);
	initialize_krb_error_table_r(&et_list);
	initialize_k524_error_table_r(&et_list);
	initialize_heim_error_table_r(&et_list);
	initialize_hx_error_table_r(&et_list);
    });

    str = heim_com_right_r(et_list, code, buffer, sizeof(buffer));
    if (str == NULL) {
	snprintf(buffer, sizeof(buffer), "Unknown error %d", (int)code);
	str = buffer;
    }
    return str;
}

void KRB5_CALLCONV
krb5_free_keyblock(mit_krb5_context context, mit_krb5_keyblock *keyblock)
{
    LOG_ENTRY();
    krb5_free_keyblock_contents(context, keyblock);
    free(keyblock);
}

void KRB5_CALLCONV
krb5_free_keyblock_contents(mit_krb5_context context, mit_krb5_keyblock *keyblock)
{
    LOG_ENTRY();
    memset(keyblock->contents, 0, keyblock->length);
    free(keyblock->contents);
    memset(keyblock, 0, sizeof(*keyblock));
}

void KRB5_CALLCONV
krb5_free_data(mit_krb5_context context, mit_krb5_data *data)
{
    LOG_ENTRY();
    krb5_free_data_contents(context, data);
    free(data);
}

void KRB5_CALLCONV
krb5_free_data_contents(mit_krb5_context context, mit_krb5_data *data)
{
    LOG_ENTRY();
    free(data->data);
    memset(data, 0, sizeof(*data));
}

mit_krb5_error_code KRB5_CALLCONV
krb5_copy_data(mit_krb5_context context,
		const mit_krb5_data *from,
		mit_krb5_data **to)
{
    *to = mshim_malloc(sizeof(**to));
    (*to)->magic = MIT_KV5M_DATA;
    (*to)->length = from->length;
    (*to)->data = mshim_malloc(from->length);
    memcpy((*to)->data, from->data, from->length);
    return 0;
}

void KRB5_CALLCONV
krb5_free_cred_contents(mit_krb5_context context, mit_krb5_creds *cred)
{
    LOG_ENTRY();
    if (cred == NULL)
	return;
    krb5_free_principal(context, cred->client);
    krb5_free_principal(context, cred->server);
    krb5_free_keyblock_contents(context, &cred->keyblock);
    krb5_free_data_contents(context, &cred->ticket);
    krb5_free_data_contents(context, &cred->second_ticket);
    /*
    mit_krb5_address **addresses;
    mit_krb5_authdata **authdata;
    */
    memset(cred, 0, sizeof(*cred));
}

void KRB5_CALLCONV
krb5_free_creds(mit_krb5_context context, mit_krb5_creds *cred)
{
    LOG_ENTRY();
    krb5_free_cred_contents(context, cred);
    free(cred);
}

void KRB5_CALLCONV
krb5_free_enc_tkt_part(mit_krb5_context context, krb5_enc_tkt_part *enc_part2)
{
    if (enc_part2->session)
	krb5_free_keyblock(context, enc_part2->session);
    if (enc_part2->client)
	krb5_free_principal(context, enc_part2->client);
    free(enc_part2);
    memset(enc_part2, 0, sizeof(*enc_part2));
}

void KRB5_CALLCONV
krb5_free_ticket(mit_krb5_context context, mit_krb5_ticket *ticket)
{
    LOG_ENTRY();
    if (ticket == NULL)
	return;
    if (ticket->server)
	krb5_free_principal(context, ticket->server);
    if (ticket->enc_part.ciphertext.data)
	krb5_free_data_contents(context, &ticket->enc_part.ciphertext);
    if (ticket->enc_part2)
	krb5_free_enc_tkt_part(context, ticket->enc_part2);
    memset(ticket, 0, sizeof(*ticket));
    free(ticket);
}

void KRB5_CALLCONV
krb5_free_authdata(mit_krb5_context context,
		   mit_krb5_authdata **val)
{
    mit_krb5_authdata **ptr;

    for (ptr = val; ptr && *ptr; ptr++) {
	free((*ptr)->contents);
	free(*ptr);
    }
    free(val);
}

mit_krb5_error_code KRB5_CALLCONV
krb5_us_timeofday(mit_krb5_context context,
		  mit_krb5_timestamp *outsec,
		  mit_krb5_int32 *outusec)
{
    krb5_timestamp sec;
    int32_t usec;
    LOG_ENTRY();
    heim_krb5_us_timeofday((krb5_context)context, &sec, &usec);
    *outsec = sec;
    *outusec = usec;
    return 0;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_timeofday(mit_krb5_context context,
	       mit_krb5_timestamp *out)
{
    krb5_timestamp ts;
    LOG_ENTRY();
    heim_krb5_timeofday((krb5_context)context, &ts);
    *out = ts;
    return 0;
}

char *
krb5_pkinit_cert_hash_str(const mit_krb5_data *cert)
{
    CC_SHA1_CTX ctx;
    char *outstr, *cpOut;
    unsigned char digest[CC_SHA1_DIGEST_LENGTH];
    unsigned i;
    
    LOG_ENTRY();

    CC_SHA1_Init(&ctx);
    CC_SHA1_Update(&ctx, cert->data, cert->length);
    CC_SHA1_Final(digest, &ctx);
    
    cpOut = outstr = (char *)malloc((2 * CC_SHA1_DIGEST_LENGTH) + 1);
    if(outstr == NULL)
	return NULL;

    for(i = 0; i < CC_SHA1_DIGEST_LENGTH; i++, cpOut += 2)
	sprintf(cpOut, "%02X", (unsigned)digest[i]);
    *cpOut = '\0';
    return outstr;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_enctype_to_string(mit_krb5_enctype enctype,
		       char *str, 
		       size_t size)
{
    snprintf(str, size, "enctype-%d", enctype);
    return 0;
}

void KRB5_CALLCONV
krb5_free_addresses(mit_krb5_context context, mit_krb5_address **addrs)
{
    unsigned int i;
    for (i = 0; addrs && addrs[i] ; i++) {
	free(addrs[i]->contents);
	free(addrs[i]);
    }
    memset(addrs, 0, sizeof(*addrs));
    free(addrs);
}

mit_krb5_error_code KRB5_CALLCONV
krb5_get_server_rcache(mit_krb5_context context,
		       const mit_krb5_data *foo,
		       mit_krb5_rcache *rcache)
{
    *rcache = NULL;
    return 0;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_os_localaddr(mit_krb5_context context, mit_krb5_address ***addresses)
{
    mit_krb5_address **a;
    krb5_addresses addrs;
    krb5_error_code ret;
    unsigned i;
    
    *addresses = NULL;

    addrs.len = 0;
    addrs.val = NULL;

    ret = heim_krb5_get_all_client_addrs(HC(context), &addrs);
    if (ret)
	return ret;

    a = calloc(addrs.len + 1, sizeof(a[0]));
    for (i = 0; i < addrs.len; i++) {
	a[i] = calloc(1, sizeof(mit_krb5_address));
	a[i]->addrtype = addrs.val[i].addr_type;
	a[i]->length = addrs.val[i].address.length;
	a[i]->contents = mshim_malloc(addrs.val[i].address.length);
	memcpy(a[i]->contents, addrs.val[i].address.data, addrs.val[i].address.length);
    }
    a[i] = NULL;

    return 0;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_string_to_deltat(char *str, mit_krb5_deltat *t)
{
    krb5_error_code ret;
    krb5_deltat ht;

    ret = heim_krb5_string_to_deltat(str, &ht);
    if (ret)
	return ret;
    *t = ht;
    return 0;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_prompter_posix(mit_krb5_context context,
		    void *data,
		    const char *name,
		    const char *banner,
		    int num_prompts,
		    mit_krb5_prompt prompts[])
{
    return EINVAL;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_get_validated_creds(mit_krb5_context context,
			 mit_krb5_creds *creds,
			 mit_krb5_principal client,
			 mit_krb5_ccache ccache,
			 char *in_tkt_service)
{
    struct comb_principal *p = (struct comb_principal *)client;
    krb5_error_code ret;
    krb5_creds hcreds;

    mshim_mcred2hcred(HC(context), creds, &hcreds);

    ret = heim_krb5_get_validated_creds(HC(context), &hcreds, p->heim, (krb5_ccache)ccache, in_tkt_service);
    heim_krb5_free_cred_contents(HC(context), &hcreds);
    return ret;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_get_renewed_creds (mit_krb5_context context,
			mit_krb5_creds *creds,
			mit_krb5_principal client,
			mit_krb5_ccache ccache,
			char *in_tkt_service)
{
    struct comb_principal *p = (struct comb_principal *)client;
    krb5_error_code ret;
    krb5_creds hcreds;

    mshim_mcred2hcred(HC(context), creds, &hcreds);

    ret = heim_krb5_get_renewed_creds(HC(context), &hcreds, p->heim, (krb5_ccache)ccache, in_tkt_service);
    heim_krb5_free_cred_contents(HC(context), &hcreds);
    return ret;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_set_real_time(mit_krb5_context context,
		   mit_krb5_timestamp ts,
		   mit_krb5_int32 usec)
{
    LOG_ENTRY();
    return heim_krb5_set_real_time(HC(context), ts, usec);
}

#include "Kerberos/kim_library.h"

kim_error
kim_library_set_ui_environment(kim_ui_environment in_ui_environment)
{
    LOG_ENTRY();
    return 0;
}

mit_krb5_boolean KRB5_CALLCONV
krb5_kuserok (mit_krb5_context context,
	      mit_krb5_principal client,
	      const char *luser)
{
    struct comb_principal *p = (struct comb_principal *)client;
    LOG_ENTRY();
    return heim_krb5_kuserok(HC(context), p->heim, luser);
}
