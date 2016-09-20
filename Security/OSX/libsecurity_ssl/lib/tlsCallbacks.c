/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


#include <stdio.h>
#include "tlsCallbacks.h"
#include "sslContext.h"
#include "sslCrypto.h"
#include "sslDebug.h"
#include "sslMemory.h"
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include "utilities/SecCFRelease.h"

#include <tls_helpers.h>
#include <tls_cache.h>

static
int tls_handshake_write_callback(tls_handshake_ctx_t ctx, const SSLBuffer data, uint8_t content_type)
{
    SSLContext *myCtx = (SSLContext *)ctx;
    sslDebugLog("%p (rec.len=%zd, ct=%d, d[0]=%d)\n", myCtx, data.length, content_type, data.data[0]);

    SSLRecord rec;

    rec.contents=data;
    rec.contentType=content_type;

    return myCtx->recFuncs->write(myCtx->recCtx,rec);
}


static int
tls_handshake_message_callback(tls_handshake_ctx_t ctx, tls_handshake_message_t event)
{
    SSLContext *myCtx = (SSLContext *)ctx;
    const tls_buffer *npn_data;
    const tls_buffer *alpn_data;
    int err = 0;

    sslDebugLog("%p, message = %d\n", ctx, event);
    
    switch(event) {
        case tls_handshake_message_certificate_request:
            assert(myCtx->protocolSide == kSSLClientSide);
            // Need to call this here, in case SetCertificate was already called.
            myCtx->clientCertState = kSSLClientCertRequested;
            myCtx->clientAuthTypes = tls_handshake_get_peer_acceptable_client_auth_type(myCtx->hdsk, &myCtx->numAuthTypes);
            if (myCtx->breakOnCertRequest && (myCtx->localCertArray==NULL)) {
                myCtx->signalCertRequest = true;
                err = errSSLClientCertRequested;
            }
            break;
        case tls_handshake_message_client_hello:
            myCtx->peerSigAlgs = tls_handshake_get_peer_signature_algorithms(myCtx->hdsk, &myCtx->numPeerSigAlgs);
            if (myCtx->breakOnClientHello) {
                err = errSSLClientHelloReceived;
            }
            break;
        case tls_handshake_message_server_hello:
            myCtx->serverHelloReceived = true;
            alpn_data = tls_handshake_get_peer_alpn_data(myCtx->hdsk);
            if(alpn_data) {
                myCtx->alpnFunc(myCtx, myCtx->alpnFuncInfo, alpn_data->data, alpn_data->length);
            } else {
                npn_data = tls_handshake_get_peer_npn_data(myCtx->hdsk);
                if(npn_data) {
                    myCtx->npnFunc(myCtx, myCtx->npnFuncInfo, npn_data->data, npn_data->length);
                }
            }
            myCtx->peerSigAlgs = tls_handshake_get_peer_signature_algorithms(myCtx->hdsk, &myCtx->numPeerSigAlgs);
            break;
        case tls_handshake_message_certificate:
            /* For clients, we only check the cert when we receive the ServerHelloDone message.
               For servers, we check the client's cert right here. For both we set the public key */
            err = tls_helper_set_peer_pubkey(myCtx->hdsk);
            if(!err && (myCtx->protocolSide == kSSLServerSide)) {
                err = tls_verify_peer_cert(myCtx);
            }
            break;
        case tls_handshake_message_server_hello_done:
            err = tls_verify_peer_cert(myCtx);
            break;
        case tls_handshake_message_NPN_encrypted_extension:
            npn_data = tls_handshake_get_peer_npn_data(myCtx->hdsk);
            if(npn_data)
                myCtx->npnFunc(myCtx, myCtx->npnFuncInfo, npn_data->data, npn_data->length);
            break;
        case tls_handshake_message_certificate_status:
            break;
        default:
            break;
    }

    return err;
}

static void
tls_handshake_ready_callback(tls_handshake_ctx_t ctx, bool write, bool ready)
{
    SSLContext *myCtx = (SSLContext *)ctx;

    sslDebugLog("%p %s ready=%d\n", myCtx, write?"write":"read", ready);

    if(write) {
        myCtx->writeCipher_ready=ready?1:0;
    } else {
        myCtx->readCipher_ready=ready?1:0;
        if(ready) {
            SSLChangeHdskState(myCtx, SSL_HdskStateReady);
        } else {
            SSLChangeHdskState(myCtx, SSL_HdskStatePending);
        }
    }
}

static int
tls_handshake_set_retransmit_timer_callback(tls_handshake_ctx_t ctx, int attempt)
{
    SSLContext *myCtx = (SSLContext *)ctx;

    sslDebugLog("%p attempt=%d\n", ctx, attempt);

    if(attempt) {
        myCtx->timeout_deadline = CFAbsoluteTimeGetCurrent()+((1<<(attempt-1))*myCtx->timeout_duration);
    } else {
        myCtx->timeout_deadline = 0; // cancel the timeout
    }
    return 0;
}

static int
tls_handshake_init_pending_cipher_callback(tls_handshake_ctx_t ctx,
                                                  uint16_t            selectedCipher,
                                                  bool                server,
                                                  SSLBuffer           key)
{
    sslDebugLog("%p, cipher=%04x, server=%d\n", ctx, selectedCipher, server);
    SSLContext *myCtx = (SSLContext *)ctx;
    return myCtx->recFuncs->initPendingCiphers(myCtx->recCtx, selectedCipher, server, key);
}

static int
tls_handshake_advance_write_callback(tls_handshake_ctx_t ctx)
{
    SSLContext *myCtx = (SSLContext *)ctx;
    sslDebugLog("%p\n", myCtx);
    //FIXME: need to filter on cipher too - require missing coretls ciphersuite header */
    bool split = (myCtx->oneByteRecordEnable && (myCtx->negProtocolVersion<=TLS_Version_1_0));
    myCtx->recFuncs->setOption(myCtx->recCtx, kSSLRecordOptionSendOneByteRecord, split);
    return myCtx->recFuncs->advanceWriteCipher(myCtx->recCtx);
}

static
int tls_handshake_rollback_write_callback(tls_handshake_ctx_t ctx)
{
    SSLContext *myCtx = (SSLContext *)ctx;
    sslDebugLog("%p\n", myCtx);
    return myCtx->recFuncs->rollbackWriteCipher(myCtx->recCtx);
}

static
int tls_handshake_advance_read_cipher_callback(tls_handshake_ctx_t ctx)
{
    SSLContext *myCtx = (SSLContext *)ctx;
    sslDebugLog("%p\n", myCtx);
    return myCtx->recFuncs->advanceReadCipher(myCtx->recCtx);
}

static
int tls_handshake_set_protocol_version_callback(tls_handshake_ctx_t ctx,
                                      tls_protocol_version  protocolVersion)
{
    SSLContext *myCtx = (SSLContext *)ctx;
    myCtx->negProtocolVersion = protocolVersion;
    return myCtx->recFuncs->setProtocolVersion(myCtx->recCtx, protocolVersion);
}

static int
tls_handshake_save_session_data_callback(tls_handshake_ctx_t ctx, SSLBuffer sessionKey, SSLBuffer sessionData)
{
    int err = errSSLSessionNotFound;
    SSLContext *myCtx = (SSLContext *)ctx;

    sslDebugLog("%s: %p, key len=%zd, k[0]=%02x, data len=%zd\n", __FUNCTION__, myCtx, sessionKey.length, sessionKey.data[0], sessionData.length);

    if(myCtx->cache) {
        err = tls_cache_save_session_data(myCtx->cache, &sessionKey, &sessionData, myCtx->sessionCacheTimeout);
    }
    return err;
}

static int
tls_handshake_load_session_data_callback(tls_handshake_ctx_t ctx, SSLBuffer sessionKey, SSLBuffer *sessionData)
{
    SSLContext *myCtx = (SSLContext *)ctx;
    int err = errSSLSessionNotFound;

    SSLFreeBuffer(&myCtx->resumableSession);
    if(myCtx->cache) {
        err = tls_cache_load_session_data(myCtx->cache, &sessionKey, &myCtx->resumableSession);
    }
    sslDebugLog("%p, key len=%zd, data len=%zd, err=%d\n", ctx, sessionKey.length, sessionData->length, err);
    *sessionData = myCtx->resumableSession;

    return err;
}

static int
tls_handshake_delete_session_data_callback(tls_handshake_ctx_t ctx, SSLBuffer sessionKey)
{
    int err = errSSLSessionNotFound;
    SSLContext *myCtx = (SSLContext *)ctx;

    sslDebugLog("%p, key len=%zd k[0]=%02x\n", ctx, sessionKey.length, sessionKey.data[0]);
    if(myCtx->cache) {
        err = tls_cache_delete_session_data(myCtx->cache, &sessionKey);
    }
    return err;
}

static int
tls_handshake_delete_all_sessions_callback(tls_handshake_ctx_t ctx)
{
    SSLContext *myCtx = (SSLContext *)ctx;
    sslDebugLog("%p\n", ctx);

    if(myCtx->cache) {
        tls_cache_empty(myCtx->cache);
    }
    return 0;
}

tls_handshake_callbacks_t tls_handshake_callbacks = {
    .write = tls_handshake_write_callback,
    .message = tls_handshake_message_callback,
    .ready = tls_handshake_ready_callback,
    .set_retransmit_timer = tls_handshake_set_retransmit_timer_callback,
    .save_session_data = tls_handshake_save_session_data_callback,
    .load_session_data = tls_handshake_load_session_data_callback,
    .delete_session_data = tls_handshake_delete_session_data_callback,
    .delete_all_sessions = tls_handshake_delete_all_sessions_callback,
    .init_pending_cipher = tls_handshake_init_pending_cipher_callback,
    .advance_write_cipher = tls_handshake_advance_write_callback,
    .rollback_write_cipher = tls_handshake_rollback_write_callback,
    .advance_read_cipher = tls_handshake_advance_read_cipher_callback,
    .set_protocol_version = tls_handshake_set_protocol_version_callback,
};
