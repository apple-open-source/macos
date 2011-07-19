/*
 * Copyright (c) 1997 - 2005 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
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

mit_krb5_error_code KRB5_CALLCONV
krb5_kt_start_seq_get(mit_krb5_context context, mit_krb5_keytab keytab,
		      mit_krb5_kt_cursor *cursor)
{
    krb5_error_code ret;

    LOG_ENTRY();

    *cursor = calloc(1, sizeof(krb5_kt_cursor));
    
    ret = heim_krb5_kt_start_seq_get(HC(context), (krb5_keytab)keytab, (krb5_kt_cursor *)*cursor);
    if (ret) {
	free(*cursor);
	*cursor = NULL;
    }
    return ret;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_kt_next_entry(mit_krb5_context context, mit_krb5_keytab keytab,
		   mit_krb5_keytab_entry *entry, mit_krb5_kt_cursor *cursor)
{
    krb5_error_code ret;
    krb5_keytab_entry e;

    LOG_ENTRY();

    ret = heim_krb5_kt_next_entry(HC(context), (krb5_keytab)keytab,
				  &e, (krb5_kt_cursor *)*cursor);
    if (ret)
	return ret;

    entry->magic = 0;
    entry->principal = mshim_hprinc2mprinc(HC(context), e.principal);
    entry->timestamp = e.timestamp;
    entry->vno = e.vno;
    mshim_hkeyblock2mkeyblock(&e.keyblock, &entry->key);

    heim_krb5_kt_free_entry(HC(context), &e);

    return 0;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_free_keytab_entry_contents(mit_krb5_context context,
				mit_krb5_keytab_entry *entry)
{
    int eric = entry->vno;
    LOG_ENTRY();

    krb5_free_principal(context, entry->principal);
    memset(entry->key.contents, 0, entry->key.length);
    free(entry->key.contents);
    memset(entry, 0, sizeof(*entry));
    entry->vno = eric;

    return 0;
}


mit_krb5_error_code KRB5_CALLCONV
krb5_kt_end_seq_get(mit_krb5_context context, mit_krb5_keytab keytab,
		    mit_krb5_kt_cursor *cursor)
{
    krb5_error_code ret;

    LOG_ENTRY();

    ret = heim_krb5_kt_end_seq_get(HC(context), (krb5_keytab)keytab, (krb5_kt_cursor *)*cursor);
    free(*cursor);
    *cursor = NULL;
    
    return ret;
}

static int
krb5_kt_compare(mit_krb5_context context,
		mit_krb5_keytab_entry *entry,
		mit_krb5_const_principal principal,
		mit_krb5_kvno vno,
		mit_krb5_enctype enctype)
{
    LOG_ENTRY();

    if(principal != NULL &&
       !krb5_principal_compare(context, entry->principal, principal))
	return 0;
    if(vno && vno != entry->vno)
	return 0;
    if(enctype && enctype != entry->key.enctype)
	return 0;
    return 1;
}

static mit_krb5_error_code
krb5_kt_free_entry(mit_krb5_context context,
		   mit_krb5_keytab_entry *entry)
{
    LOG_ENTRY();

    krb5_free_principal (context, entry->principal);
    krb5_free_keyblock_contents (context, &entry->key);
    memset(entry, 0, sizeof(*entry));
    return 0;
}

static mit_krb5_error_code
krb5_kt_copy_entry_contents(mit_krb5_context context,
			    const mit_krb5_keytab_entry *in,
			    mit_krb5_keytab_entry *out)
{
    krb5_error_code ret;

    LOG_ENTRY();

    memset(out, 0, sizeof(*out));
    out->vno = in->vno;

    ret = krb5_copy_principal (context, in->principal, &out->principal);
    if (ret)
	goto fail;
    ret = krb5_copy_keyblock_contents (context, &in->key, &out->key);
    if (ret)
	goto fail;
    out->timestamp = in->timestamp;
    return 0;
fail:
    krb5_kt_free_entry (context, out);
    return ret;
}


mit_krb5_error_code KRB5_CALLCONV
krb5_kt_get_entry(mit_krb5_context context,
		  mit_krb5_keytab id,
		  mit_krb5_const_principal principal,
		  mit_krb5_kvno kvno,
		  mit_krb5_enctype enctype,
		  mit_krb5_keytab_entry *entry)
{
    mit_krb5_keytab_entry tmp;
    mit_krb5_error_code ret;
    mit_krb5_kt_cursor cursor;

    LOG_ENTRY();

    memset(entry, 0, sizeof(*entry));

    ret = krb5_kt_start_seq_get (context, id, &cursor);
    if (ret)
	return KRB5_KT_NOTFOUND;

    entry->vno = 0;
    while (krb5_kt_next_entry(context, id, &tmp, &cursor) == 0) {
	if (krb5_kt_compare(context, &tmp, principal, 0, enctype)) {
	    /* the file keytab might only store the lower 8 bits of
	       the kvno, so only compare those bits */
	    if (kvno == tmp.vno
		|| (tmp.vno < 256 && kvno % 256 == tmp.vno)) {
		krb5_kt_copy_entry_contents (context, &tmp, entry);
		krb5_kt_free_entry (context, &tmp);
		krb5_kt_end_seq_get(context, id, &cursor);
		return 0;
	    } else if (kvno == 0 && tmp.vno > entry->vno) {
		if (entry->vno)
		    krb5_kt_free_entry (context, entry);
		krb5_kt_copy_entry_contents (context, &tmp, entry);
	    }
	}
	krb5_kt_free_entry(context, &tmp);
    }
    krb5_kt_end_seq_get (context, id, &cursor);
    if (entry->vno == 0)
	return KRB5_KT_NOTFOUND;
    return 0;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_kt_get_name(mit_krb5_context context,
		 mit_krb5_keytab keytab,
		 char *name,
		 unsigned int namelen)
{
    return heim_krb5_kt_get_name(HC(context), (krb5_keytab)keytab, name, namelen);
}

mit_krb5_error_code KRB5_CALLCONV
krb5_kt_read_service_key(mit_krb5_context context,
			 mit_krb5_pointer keyprocarg,
			 mit_krb5_principal principal,
			 mit_krb5_kvno vno,
			 mit_krb5_enctype enctype,
			 mit_krb5_keyblock **key)
{
    mit_krb5_keytab keytab;
    mit_krb5_keytab_entry entry;
    mit_krb5_error_code ret;

    LOG_ENTRY();

    if (keyprocarg)
	ret = krb5_kt_resolve (context, keyprocarg, &keytab);
    else
	ret = krb5_kt_default (context, &keytab);

    if (ret)
	return ret;

    ret = krb5_kt_get_entry (context, keytab, principal, vno, enctype, &entry);
    krb5_kt_close (context, keytab);
    if (ret)
	return ret;
    ret = krb5_copy_keyblock (context, &entry.key, key);
    krb5_kt_free_entry(context, &entry);
    return ret;
}

mit_krb5_error_code KRB5_LIB_FUNCTION
krb5_kt_remove_entry(mit_krb5_context context,
		     mit_krb5_keytab id,
		     mit_krb5_keytab_entry *entry)
{
    struct comb_principal *p = (struct comb_principal *)entry->principal;
    krb5_keytab_entry e;

    LOG_ENTRY();

    memset(&e, 0, sizeof(e));

    e.principal = p->heim;
    e.vno = entry->vno;
    e.timestamp = entry->timestamp; 

    return heim_krb5_kt_remove_entry(HC(context), (krb5_keytab)id, &e);
}

mit_krb5_error_code KRB5_CALLCONV
krb5_kt_add_entry(mit_krb5_context context,
		  mit_krb5_keytab id,
		  mit_krb5_keytab_entry *entry)
{
    struct comb_principal *p = (struct comb_principal *)entry->principal;
    krb5_keytab_entry e;

    LOG_ENTRY();

    memset(&e, 0, sizeof(e));

    e.principal = p->heim;
    e.vno = entry->vno;
    e.timestamp = entry->timestamp; 
    e.keyblock.keytype = entry->key.enctype;
    e.keyblock.keyvalue.data = entry->key.contents;
    e.keyblock.keyvalue.length = entry->key.length;

    return heim_krb5_kt_add_entry(HC(context), (krb5_keytab)id, &e);
}



const char * KRB5_CALLCONV
krb5_kt_get_type(mit_krb5_context context, mit_krb5_keytab id)
{
    krb5_error_code ret;
    static char name[80];

    LOG_ENTRY();

    ret = heim_krb5_kt_get_type (HC(context),
				 (krb5_keytab)id,
				 name,
				 sizeof(name));
    if (ret)
	name[0] = '\0';
    return name;
}
