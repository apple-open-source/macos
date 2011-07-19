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
#include <syslog.h>


mit_krb5_error_code KRB5_CALLCONV
krb5_decode_ticket(const mit_krb5_data *code, 
		   mit_krb5_ticket **rep)
{
    krb5_error_code ret;
    Ticket t;
    
    LOG_ENTRY();

    ret = decode_Ticket((unsigned char *)code->data, code->length, &t, NULL);
    if (ret)
	return ret;
    
    *rep = calloc(1, sizeof(**rep));

    /* XXX */
    (*rep)->enc_part.kvno = t.enc_part.kvno ? *t.enc_part.kvno : 0;

    free_Ticket(&t);

    return 0;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_get_credentials(mit_krb5_context context,
		     mit_krb5_flags flags,
		     mit_krb5_ccache id,
		     mit_krb5_creds *mcreds,
		     mit_krb5_creds **creds)
{
    krb5_error_code ret;
    krb5_flags options = flags;
    krb5_creds *hcreds = NULL, hmcreds;

    LOG_ENTRY();

    mshim_mcred2hcred(HC(context), mcreds, &hmcreds);

    ret = heim_krb5_get_credentials(HC(context), options, (krb5_ccache)id, &hmcreds, &hcreds);

    heim_krb5_free_cred_contents(HC(context), &hmcreds);
    if (ret == 0) {
	*creds = calloc(1, sizeof(**creds));
	mshim_hcred2mcred(HC(context), hcreds, *creds);
	heim_krb5_free_creds(HC(context), hcreds);
    }

    return ret;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_copy_creds(mit_krb5_context context,
		const mit_krb5_creds *from,
		mit_krb5_creds **to)
{
    mit_krb5_error_code ret;
    mit_krb5_creds *c;

    c = mshim_malloc(sizeof(*c));

    c->magic = MIT_KV5M_CREDS;

    ret = krb5_copy_principal(context, from->client, &c->client);
    if (ret)
	abort();
    ret = krb5_copy_principal(context, from->server, &c->server);
    if (ret)
	abort();
    
    ret = krb5_copy_keyblock_contents(context, &from->keyblock,
				      &c->keyblock);
    if (ret)
	abort();

    c->ticket.magic = MIT_KV5M_DATA;
    c->ticket.length = from->ticket.length;
    c->ticket.data = mshim_malloc(from->ticket.length);
    memcpy(c->ticket.data, from->ticket.data, c->ticket.length);

    c->times.authtime = from->times.authtime;
    c->times.starttime = from->times.starttime;
    c->times.endtime = from->times.endtime;
    c->times.renew_till = from->times.renew_till;

    c->ticket_flags = from->ticket_flags;

    *to = c;

    return 0;
}
