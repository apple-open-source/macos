/*
 * Copyright (c) 2001-2003 Simon Wilkinson. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"

#ifdef GSSAPI

#include <openssl/crypto.h>
#include <openssl/bn.h>

#include "xmalloc.h"
#include "buffer.h"
#include "bufaux.h"
#include "kex.h"
#include "log.h"
#include "packet.h"
#include "dh.h"
#include "ssh2.h"
#include "ssh-gss.h"
#include "monitor_wrap.h"

static void kex_gss_send_error(Gssctxt *ctxt);

void
kexgss_server(Kex *kex)
{
	OM_uint32 maj_status, min_status;
	
	/* Some GSSAPI implementations use the input value of ret_flags (an
 	 * output variable) as a means of triggering mechanism specific 
 	 * features. Initializing it to zero avoids inadvertently 
 	 * activating this non-standard behaviour.*/

	OM_uint32 ret_flags = 0;
	gss_buffer_desc gssbuf,send_tok,recv_tok,msg_tok;
	Gssctxt *ctxt = NULL;
        unsigned int klen, kout;
        unsigned char *kbuf;
        unsigned char *hash;
        DH *dh;
        BIGNUM *shared_secret = NULL;
        BIGNUM *dh_client_pub = NULL;
	int type =0;
	u_int slen;
	gss_OID oid;
	
	/* Initialise GSSAPI */

	debug2("%s: Identifying %s",__func__,kex->name);
	oid=ssh_gssapi_server_id_kex(kex->name);
	if (oid==NULL) {
	   fatal("Unknown gssapi mechanism");
	}
	
	debug2("%s: Acquiring credentials",__func__);
	
	if (GSS_ERROR(PRIVSEP(ssh_gssapi_server_ctx(&ctxt,oid)))) {
		kex_gss_send_error(ctxt);
        	fatal("Unable to acquire credentials for the server");
        }
                                                                                                                                
	do {
		debug("Wait SSH2_MSG_GSSAPI_INIT");
		type = packet_read();
		switch(type) {
		case SSH2_MSG_KEXGSS_INIT:
			if (dh_client_pub!=NULL) 
				fatal("Received KEXGSS_INIT after initialising");
			recv_tok.value=packet_get_string(&slen);
			recv_tok.length=slen; /* int vs. size_t */

		        dh_client_pub = BN_new();
		        
		        if (dh_client_pub == NULL)
        			fatal("dh_client_pub == NULL");
		  	packet_get_bignum2(dh_client_pub);
		  	
		  	/* Send SSH_MSG_KEXGSS_HOSTKEY here, if we want */
			break;
		case SSH2_MSG_KEXGSS_CONTINUE:
			recv_tok.value=packet_get_string(&slen);
			recv_tok.length=slen; /* int vs. size_t */
			break;
		default:
			packet_disconnect("Protocol error: didn't expect packet type %d",
					   type);
		}
		
		maj_status=PRIVSEP(ssh_gssapi_accept_ctx(ctxt,&recv_tok, 
							 &send_tok, &ret_flags));

		gss_release_buffer(&min_status,&recv_tok);
		
		if (maj_status!=GSS_S_COMPLETE && send_tok.length==0) {
			fatal("Zero length token output when incomplete");
		}

		if (dh_client_pub == NULL)
			fatal("No client public key");
		
		if (maj_status & GSS_S_CONTINUE_NEEDED) {
			debug("Sending GSSAPI_CONTINUE");
			packet_start(SSH2_MSG_KEXGSS_CONTINUE);
			packet_put_string(send_tok.value,send_tok.length);
			packet_send();
			packet_write_wait();
			gss_release_buffer(&min_status, &send_tok);
		}
	} while (maj_status & GSS_S_CONTINUE_NEEDED);

	if (GSS_ERROR(maj_status)) {
		kex_gss_send_error(ctxt);
		if (send_tok.length>0) {
			packet_start(SSH2_MSG_KEXGSS_CONTINUE);
			packet_put_string(send_tok.value,send_tok.length);
			packet_send();
			packet_write_wait();
		}	
		fatal("accept_ctx died");
	}
	
	debug("gss_complete");
	if (!(ret_flags & GSS_C_MUTUAL_FLAG))
		fatal("mutual authentication flag wasn't set");
		
	if (!(ret_flags & GSS_C_INTEG_FLAG))
		fatal("Integrity flag wasn't set");
			
	dh = dh_new_group1();
	dh_gen_key(dh, kex->we_need * 8);
	
        if (!dh_pub_is_valid(dh, dh_client_pub))
                packet_disconnect("bad client public DH value");

        klen = DH_size(dh);
        kbuf = xmalloc(klen); 
        kout = DH_compute_key(kbuf, dh_client_pub, dh);

	shared_secret = BN_new();
	BN_bin2bn(kbuf, kout, shared_secret);
	memset(kbuf, 0, klen);
	xfree(kbuf);
	
	/* The GSSAPI hash is identical to the Diffie Helman one */
        hash = kex_dh_hash(
            kex->client_version_string,
            kex->server_version_string,
            buffer_ptr(&kex->peer), buffer_len(&kex->peer),
            buffer_ptr(&kex->my), buffer_len(&kex->my),
            NULL, 0, /* Change this if we start sending host keys */
            dh_client_pub,
            dh->pub_key,
            shared_secret
	);
	BN_free(dh_client_pub);
		
	if (kex->session_id == NULL) {
		kex->session_id_len = 20;
		kex->session_id = xmalloc(kex->session_id_len);
		memcpy(kex->session_id, hash, kex->session_id_len);
	}
	                        
	gssbuf.value = hash;
	gssbuf.length = 20; /* Hashlen appears to always be 20 */
	
	if (GSS_ERROR(PRIVSEP(ssh_gssapi_sign(ctxt,&gssbuf,&msg_tok)))) {
		kex_gss_send_error(ctxt);
		fatal("Couldn't get MIC");
	}
	
	packet_start(SSH2_MSG_KEXGSS_COMPLETE);
	packet_put_bignum2(dh->pub_key);
	packet_put_string((char *)msg_tok.value,msg_tok.length);

	if (send_tok.length!=0) {
		packet_put_char(1); /* true */
		packet_put_string((char *)send_tok.value,send_tok.length);
	} else {
		packet_put_char(0); /* false */
	}
 	packet_send();
	packet_write_wait();

        /* We used to store the client name and credentials here for later
         * use. With privsep, its easier to do this as a by product of the
         * call to accept_context, which stores delegated information when
         * the context is complete */
         
	gss_release_buffer(&min_status, &send_tok);	

	/* If we've got a context, delete it. It may be NULL if we've been
	 * using privsep */
	ssh_gssapi_delete_ctx(&ctxt);
	
	DH_free(dh);

	kex_derive_keys(kex, hash, shared_secret);
	BN_clear_free(shared_secret);
	kex_finish(kex);
}

static void 
kex_gss_send_error(Gssctxt *ctxt) {
	char *errstr;
	OM_uint32 maj,min;
		
	errstr=PRIVSEP(ssh_gssapi_last_error(ctxt,&maj,&min));
	if (errstr) {
		packet_start(SSH2_MSG_KEXGSS_ERROR);
		packet_put_int(maj);
		packet_put_int(min);
		packet_put_cstring(errstr);
		packet_put_cstring("");
		packet_send();
		packet_write_wait();
		/* XXX - We should probably log the error locally here */
		xfree(errstr);
	}
}
#endif /* GSSAPI */
