//
//  tls_handshake.c
//  libsecurity_ssl
//
//  Created by Fabrice Gautier on 8/6/13.
//
//

#include <stdio.h>
#include <stdlib.h>

#include "tls_handshake_priv.h"
#include "tls_metrics.h"
#include "sslHandshake.h"
#include "sslHandshake_priv.h"
#include "sslDebug.h"
#include "sslDigests.h"
#include "sslCipherSpecs.h"
#include "sslAlertMessage.h"
#include "sslCrypto.h"

/*
 * Minimum and maximum supported versions
 */
#define DEFAULT_CLIENT_MINIMUM_STREAM_VERSION tls_protocol_version_SSL_3
#define DEFAULT_SERVER_MINIMUM_STREAM_VERSION tls_protocol_version_TLS_1_0
#define DEFAULT_MAXIMUM_STREAM_VERSION tls_protocol_version_TLS_1_2

#define DEFAULT_MINIMUM_DATAGRAM_VERSION  tls_protocol_version_DTLS_1_0
#define DEFAULT_MAXIMUM_DATAGRAM_VERSION  tls_protocol_version_DTLS_1_0

#define DEFAULT_DTLS_TIMEOUT    1
#define DEFAULT_DTLS_MTU        1400
#define MIN_ALLOWED_DTLS_MTU    64      /* this ensure than there will be no integer
                                            underflow when calculating max write size */

#define DEFAULT_MIN_DH_GROUP_SIZE 1024
#define LOWEST_MIN_DH_GROUP_SIZE  512
#define HIGHEST_MIN_DH_GROUP_SIZE 2048

int SSLAllocBuffer(tls_buffer *buf, size_t length);

tls_private_key_t
tls_private_key_rsa_create(tls_private_key_ctx_t ctx, size_t size, tls_private_key_rsa_sign sign, tls_private_key_rsa_decrypt decrypt)
{
    tls_private_key_desc_t desc;

    desc.type = tls_private_key_type_rsa;
    desc.rsa.decrypt = decrypt;
    desc.rsa.sign = sign;
    desc.rsa.size = size;

    return tls_private_key_create(&desc, ctx, NULL);
}

tls_private_key_t
tls_private_key_ecdsa_create(tls_private_key_ctx_t ctx, size_t size, uint16_t curve, tls_private_key_ecdsa_sign sign)
{
    tls_private_key_desc_t desc;

    desc.type = tls_private_key_type_ecdsa;
    desc.ecdsa.sign = sign;
    desc.ecdsa.size = size;
    desc.ecdsa.curve = curve;

    return tls_private_key_create(&desc, ctx, NULL);
};

tls_private_key_t tls_private_key_create(tls_private_key_desc_t *desc,
                                         tls_private_key_ctx_t ctx,
                                         tls_private_key_ctx_release ctx_release)
{
    tls_private_key_t key;

    key = sslMalloc(sizeof(struct _tls_private_key));

    if(key==NULL) return NULL;

    key->ctx = ctx;
    key->ctx_release = ctx_release;
    key->desc.type = desc->type;
    switch(desc->type) {
        case tls_private_key_type_rsa:
            key->desc.rsa = desc->rsa;
            break;
        case tls_private_key_type_ecdsa:
            key->desc.ecdsa = desc->ecdsa;
            break;
        default:
            sslFree(key);
            key = NULL;
            break;
    }

    return key;
}

tls_private_key_ctx_t tls_private_key_get_context(tls_private_key_t key)
{
    return key->ctx;
}

void tls_private_key_destroy(tls_private_key_t key)
{
    if(key->ctx_release && key->ctx)
        key->ctx_release(key->ctx);
    sslFree(key);
}


static int
tls_handshake_set_ciphersuites_internal(tls_handshake_t filter, tls_handshake_config_t config, const uint16_t *ciphersuites, unsigned n)
{
    unsigned i;
    unsigned count = 0;
    uint16_t *_cs;

    for(i=0;i<n;i++) {
        uint16_t cs = ciphersuites[i];
        if(tls_handshake_ciphersuite_is_supported(filter->isServer, filter->isDTLS, cs) &&
           tls_handshake_ciphersuite_is_allowed(config, cs))
        {
            count++;
        }
    }

    if(count == 0) {
        return errSSLParam;
    }

    sslFree(filter->enabledCipherSuites);
    filter->numEnabledCipherSuites=0;

    _cs = sslMalloc(count*sizeof(uint16_t));
    if(!_cs) {
        return errSSLAllocate;
    }

    filter->numEnabledCipherSuites = count;
    filter->enabledCipherSuites = _cs;

    for(i=0;i<n;i++) {
        uint16_t cs = ciphersuites[i];
        if(tls_handshake_ciphersuite_is_supported(filter->isServer, filter->isDTLS, cs) &&
           tls_handshake_ciphersuite_is_allowed(config, cs))
        {
            *_cs++ = cs;
        }
    }

    sslAnalyzeCipherSpecs(filter);

    return errSSLSuccess;
}


tls_handshake_t
tls_handshake_create(bool dtls, bool server)
{
    tls_handshake_t ctx;

    ctx=(tls_handshake_t)malloc(sizeof(struct _tls_handshake_s));

    if(ctx == NULL) {
		return NULL;
	}

    memset(ctx, 0, sizeof(struct _tls_handshake_s));

    ctx->state = SSL_HdskStateUninit;
    ctx->retransmit_attempt = 0;
    ctx->clientCertState = kSSLClientCertNone;

    ctx->isDTLS = dtls;
    ctx->mtu = DEFAULT_DTLS_MTU;

    ctx->isServer = server;
	/* Default value so we can send and receive hello msgs */
	ctx->sslTslCalls = &Ssl3Callouts;

    /* Initialize the cipher state to NULL_WITH_NULL_NULL */
    ctx->selectedCipher        = TLS_NULL_WITH_NULL_NULL;
    InitCipherSpecParams(ctx);

    /* 
     * Default configuration
     */
    tls_handshake_set_config(ctx, tls_handshake_config_default);

    /* 
     * Enabled SCT and OCSP extension for client 
     */
    if(!ctx->isServer) {
        tls_handshake_set_sct_enable(ctx, true);
        tls_handshake_set_ocsp_enable(ctx, true);
    }

    tls_handshake_set_ems_enable(ctx, true);
	/*
	 * Initial/default set of ECDH curves
	 */
    tls_handshake_set_curves(ctx, KnownCurves, CurvesCount);
	ctx->ecdhPeerCurve = tls_curve_none;		/* until we negotiate one */
	ctx->negAuthType = tls_client_auth_type_None;		/* ditto */

    /*
     * Initial/default set of SigAlgs
     */
    tls_handshake_set_sigalgs(ctx, KnownSigAlgs, SigAlgsCount);

    if (ctx->isServer) {
        SSLChangeHdskState(ctx, SSL_HdskStateServerUninit);
    } else {
        SSLChangeHdskState(ctx, SSL_HdskStateClientUninit);
    }

	return ctx;
}

void
tls_handshake_destroy(tls_handshake_t filter)
{
    tls_metric_destroyed(filter);

    /* Free the last handshake message flight */
    SSLResetFlight(filter);

    CloseHash(&SSLHashSHA1, &filter->shaState);
    CloseHash(&SSLHashMD5,  &filter->md5State);
    CloseHash(&SSLHashSHA256,  &filter->sha256State);
    CloseHash(&SSLHashSHA384,  &filter->sha384State);
    CloseHash(&SSLHashSHA512,  &filter->sha512State);

    sslFreePubKey(&filter->peerPubKey);
    sslFreePubKey(&filter->rsaEncryptPubKey);

    SSLFreeBuffer(&filter->fragmentedMessageCache);
    SSLFreeBuffer(&filter->peerID);
    SSLFreeBuffer(&filter->proposedSessionID);
    SSLFreeBuffer(&filter->sessionID);
    SSLFreeBuffer(&filter->sessionTicket);
    SSLFreeBuffer(&filter->externalSessionTicket);
    SSLFreeBuffer(&filter->preMasterSecret);
    SSLFreeBuffer(&filter->dhPeerPublic);
	SSLFreeBuffer(&filter->ecdhPeerPublic);
    SSLFreeBuffer(&filter->npnOwnData);
    SSLFreeBuffer(&filter->npnPeerData);
    SSLFreeBuffer(&filter->alpnOwnData);
    SSLFreeBuffer(&filter->alpnPeerData);
    SSLFreeBuffer(&filter->ownVerifyData);
    SSLFreeBuffer(&filter->peerVerifyData);
    SSLFreeBuffer(&filter->pskIdentity);
    SSLFreeBuffer(&filter->pskSharedSecret);
    SSLFreeBuffer(&filter->peerDomainName);
    SSLFreeBuffer(&filter->ocsp_response);
    SSLFreeBuffer(&filter->ocsp_request_extensions);
    tls_free_buffer_list(filter->ocsp_responder_id_list);
    tls_free_buffer_list(filter->sct_list);

    sslFree(filter->userAgent);
    sslFree(filter->enabledCipherSuites);
    sslFree(filter->requestedCipherSuites);
    sslFree(filter->ecdhCurves);
    sslFree(filter->peerSigAlgs);
    sslFree(filter->localSigAlgs);
    sslFree(filter->clientAuthTypes);
    sslFree(filter->ecdhContext);
    sslFree(filter->dhParams);
    sslFree(filter->dhContext);
    sslFree(filter->requested_ecdh_curves);

    if(filter->signingPrivKeyRef)
        tls_private_key_destroy(filter->signingPrivKeyRef);
    SSLFreeCertificates(filter->localCert);

    SSLFreeCertificates(filter->peerCert);
    if(!filter->isServer) {
        SSLFreeDNList(filter->acceptableDNList);
    }

    free(filter);
}

int
tls_handshake_set_callbacks(tls_handshake_t filter,
                                   tls_handshake_callbacks_t *callbacks,
                                   tls_handshake_ctx_t ctx)
{
    filter->callback_ctx = ctx;
    filter->callbacks = callbacks;
    
    return 0;
}

int
tls_handshake_process(tls_handshake_t filter, const tls_buffer message, uint8_t contentType)
{
    int err;
    switch (contentType)
    {
        case tls_record_type_Handshake:
            sslLogRxProtocolDebug("Handshake");
            err = SSLProcessHandshakeRecord(message, filter);
            break;
        case tls_record_type_Alert:
            sslLogRxProtocolDebug("Alert");
            err = SSLProcessAlert(message, filter);
            break;
        case tls_record_type_ChangeCipher:
            sslLogRxProtocolDebug("ChangeCipher");
            err = SSLProcessChangeCipherSpec(message, filter);
            break;
        case tls_record_type_SSL2:
            sslLogRxProtocolDebug("SSL2");
            err = SSLProcessSSL2Message(message, filter);
            break;
        default:
            sslLogRxProtocolDebug("Not a supported protocol message");
            return errSSLProtocol;
    }

    if(err==errSSLUnexpectedRecord)
        err=DTLSRetransmit(filter);

    if(err)
        sslErrorLog("Error processing a message (ct=%d, err=%d)", contentType, err);

    return err;
}

int
tls_handshake_continue(tls_handshake_t filter)
{
    int err;

    if(!filter->advanceHandshake)
        return 0;

    require_noerr((err=SSLAdvanceHandshake(filter->currentMessage, filter)), errOut);

    if (filter->fragmentedMessageCache.data != 0)
        err = SSLProcessHandshakeRecordInner(filter->fragmentedMessageCache, filter);

errOut:
    return err;
}

int
tls_handshake_set_ciphersuites(tls_handshake_t filter, const uint16_t *ciphersuites, unsigned n)
{
    return tls_handshake_set_ciphersuites_internal(filter, tls_handshake_config_none, ciphersuites, n);
}

int
tls_handshake_get_ciphersuites(tls_handshake_t filter, const uint16_t **ciphersuites, unsigned *n)
{
    *ciphersuites = filter->enabledCipherSuites;
    *n = filter->numEnabledCipherSuites;
    return 0;
}

int
tls_handshake_set_curves(tls_handshake_t filter, const uint16_t *curves, unsigned n)
{

    unsigned i;
    unsigned count = 0;
    uint16_t *_c;

    for(i=0;i<n;i++) {
        uint16_t c = curves[i];
        if(tls_handshake_curve_is_supported(c))
            count++;
    }

    sslFree(filter->ecdhCurves);
    filter->ecdhNumCurves=0;

    _c = sslMalloc(count*sizeof(uint16_t));
    if(!_c) {
        return errSSLAllocate;
    }

    filter->ecdhNumCurves = count;
    filter->ecdhCurves = _c;

    for(i=0;i<n;i++) {
        uint16_t c = curves[i];
        if(tls_handshake_curve_is_supported(c))
            *_c++ = c;
    }

    return 0;
}

int
tls_handshake_set_resumption(tls_handshake_t filter, bool allow)
{
    filter->allowResumption=allow;
    return 0;
}

int
tls_handshake_set_session_ticket_enabled(tls_handshake_t filter, bool enabled)
{
    filter->sessionTicket_enabled=enabled;
    return 0;
}

int
tls_handshake_set_renegotiation(tls_handshake_t filter, bool allow)
{
    filter->allowRenegotiation=allow;
    return 0;
}

int
tls_handshake_set_client_auth(tls_handshake_t filter, bool request)
{
    assert(filter->isServer);
    filter->tryClientAuth=request;
    return 0;
}

int
tls_handshake_set_mtu(tls_handshake_t filter, size_t mtu)
{
    if(mtu<MIN_ALLOWED_DTLS_MTU)
        return errSSLParam;

    filter->mtu = mtu;
    return 0;
}

static
tls_protocol_version tls_handshake_min_allowed_version(tls_handshake_t filter, tls_handshake_config_t config)
{
    if(filter->isDTLS)
        return tls_protocol_version_DTLS_1_0;

    switch(config) {
        case tls_handshake_config_ATSv2:
        case tls_handshake_config_ATSv1:
        case tls_handshake_config_ATSv1_noPFS:
        case tls_handshake_config_anonymous:
            return tls_protocol_version_TLS_1_2;
        case tls_handshake_config_standard:
        case tls_handshake_config_standard_TLSv3:
        case tls_handshake_config_RC4_fallback:
        case tls_handshake_config_TLSv1_fallback:
        case tls_handshake_config_TLSv1_RC4_fallback:
        case tls_handshake_config_default:
        case tls_handshake_config_legacy:
        case tls_handshake_config_legacy_DHE:
        case tls_handshake_config_none:
        case tls_handshake_config_3DES_fallback:
        case tls_handshake_config_TLSv1_3DES_fallback:
            return tls_protocol_version_TLS_1_0;
    }

    /* Note: we do this here instead of a 'default:' case, so that the compiler will warn us when
     adding new config in the enum */
    return tls_protocol_version_TLS_1_0;
}

static
tls_protocol_version tls_handshake_max_allowed_version(tls_handshake_t filter, tls_handshake_config_t config)
{
    if(filter->isDTLS)
        return tls_protocol_version_DTLS_1_0;

    switch(config) {
        case tls_handshake_config_TLSv1_fallback:
        case tls_handshake_config_TLSv1_RC4_fallback:
        case tls_handshake_config_TLSv1_3DES_fallback:
            return tls_protocol_version_TLS_1_0;
        case tls_handshake_config_none:
        case tls_handshake_config_default:
        case tls_handshake_config_ATSv1:
        case tls_handshake_config_ATSv1_noPFS:
        case tls_handshake_config_standard:
        case tls_handshake_config_RC4_fallback:
        case tls_handshake_config_legacy:
        case tls_handshake_config_legacy_DHE:
        case tls_handshake_config_anonymous:
        case tls_handshake_config_3DES_fallback:
            return tls_protocol_version_TLS_1_2;
        case tls_handshake_config_standard_TLSv3:
        case tls_handshake_config_ATSv2:
            return tls_protocol_version_TLS_1_3;
    }

    /* Note: we do this here instead of a 'default:' case, so that the compiler will warn us when
     adding new config in the enum */
    return tls_protocol_version_TLS_1_2;
}

int
tls_handshake_set_min_protocol_version(tls_handshake_t filter, tls_protocol_version min)
{
    filter->minProtocolVersion = min;
    if((!filter->isDTLS && filter->maxProtocolVersion<min) ||
       (filter->isDTLS && filter->maxProtocolVersion>min)) {
        filter->maxProtocolVersion = min;
    }
    return 0;
}

int
tls_handshake_get_min_protocol_version(tls_handshake_t filter, tls_protocol_version *min)
{
    *min = filter->minProtocolVersion;
    return 0;
}

int
tls_handshake_set_max_protocol_version(tls_handshake_t filter, tls_protocol_version max)
{
    filter->maxProtocolVersion = max;
    if((!filter->isDTLS && filter->minProtocolVersion>max) ||
       (filter->isDTLS && filter->minProtocolVersion<max)) {
            filter->minProtocolVersion = max;
    }
    return 0;
}

int
tls_handshake_get_max_protocol_version(tls_handshake_t filter, tls_protocol_version *max)
{
    *max = filter->maxProtocolVersion;
    return 0;
}

int
tls_handshake_set_peer_hostname(tls_handshake_t filter, const char *hostname, size_t len)
{
    assert(!filter->isServer);
    SSLFreeBuffer(&filter->peerDomainName); // in case you set it twice
    return SSLCopyBufferTerm(hostname, len, &filter->peerDomainName);
}

int
tls_handshake_get_peer_hostname(tls_handshake_t filter, const char **hostname, size_t *len)
{
    assert(!filter->isServer);
    *hostname = (char *)filter->peerDomainName.data;
    *len = filter->peerDomainName.length;
    return 0;
}

int
tls_handshake_set_min_dh_group_size(tls_handshake_t filter, unsigned nbits)
{
    if(nbits<LOWEST_MIN_DH_GROUP_SIZE) nbits = LOWEST_MIN_DH_GROUP_SIZE;
    if(nbits>HIGHEST_MIN_DH_GROUP_SIZE) nbits = HIGHEST_MIN_DH_GROUP_SIZE;
    filter->dhMinGroupSize = nbits;
    return 0;
}

int
tls_handshake_get_min_dh_group_size(tls_handshake_t filter, unsigned *nbits)
{
    *nbits = filter->dhMinGroupSize;
    return 0;
}

/* Set DH parameters - Server only */
int
tls_handshake_set_dh_parameters(tls_handshake_t filter, tls_buffer *params)
{
    assert(filter->isServer);
    assert(params);
    const uint8_t *der, *der_end;
    size_t n;

    der = params->data;
    der_end = params->data + params->length;
    n = ccder_decode_dhparam_n(der, der_end);

    sslFree(filter->dhParams);
    filter->dhParams = sslMalloc(ccdh_gp_size(ccn_sizeof_n(n)));
    if(!filter->dhParams) {
        return errSSLAllocate;
    }

    CCDH_GP_N(filter->dhParams) = n;

    der = ccder_decode_dhparams(filter->dhParams, der, der_end);
    if (der == NULL) {
        return errSSLParam;
    } else {
        return 0;
    }
}

/* Set the local identity (cert chain and private key) */
int
tls_handshake_set_identity(tls_handshake_t filter, SSLCertificate *certs, tls_private_key_t key)
{
    SSLFreeCertificates(filter->localCert);
    if(filter->signingPrivKeyRef)
        tls_private_key_destroy(filter->signingPrivKeyRef);
    if(certs) {
        tls_buffer_list_t *copy;
        tls_copy_buffer_list((tls_buffer_list_t *)certs, &copy);
        filter->localCert = (SSLCertificate *)copy;
    } else {
        filter->localCert = NULL;
    }
    if(key)
        filter->signingPrivKeyRef = tls_private_key_create(&key->desc, key->ctx, key->ctx_release);
    else
        filter->signingPrivKeyRef = NULL;
    return 0;
}

int
tls_handshake_set_encrypt_rsa_public_key(tls_handshake_t filter, const tls_buffer *modulus, const tls_buffer *exponent)
{
    sslFreePubKey(&filter->rsaEncryptPubKey);
    return sslGetPubKeyFromBits(modulus, exponent, &filter->rsaEncryptPubKey);
}

/* Set the PSK identity - Client only */
int
tls_handshake_set_psk_identity(tls_handshake_t filter, tls_buffer *psk_identity)
{
    assert(!filter->isServer);
    SSLCopyBuffer(psk_identity, &filter->pskIdentity);
    return 0;
}

/* Set the PSK identity hint - Server only */
int
tls_handshake_set_psk_identity_hint(tls_handshake_t filter, tls_buffer *psk_identity_hint)
{
    assert(filter->isServer);
    /* Not Implemented */
    return -1;
}


int
tls_handshake_set_psk_secret(tls_handshake_t filter, tls_buffer *psk_secret)
{
    SSLCopyBuffer(psk_secret, &filter->pskSharedSecret);
    return 0;
}

int
tls_handshake_set_client_auth_type(tls_handshake_t filter, tls_client_auth_type auth_type)
{
    assert(!filter->isServer);
    filter->negAuthType = auth_type;
    return 0;
}

int
tls_handshake_set_acceptable_dn_list(tls_handshake_t filter, DNListElem *dn_list)
{
    assert(filter->isServer);
    filter->acceptableDNList = dn_list;
    return 0;
}

int
tls_handshake_get_acceptable_dn_list(tls_handshake_t filter, DNListElem **dn_list)
{
    assert(filter->isServer);
    *dn_list = filter->acceptableDNList;
    return 0;
}

int
tls_handshake_set_acceptable_client_auth_type(tls_handshake_t filter, tls_client_auth_type *auth_types, unsigned n)
{
    assert(filter->isServer);
    sslFree(filter->clientAuthTypes);
    filter->clientAuthTypes = sslMalloc(sizeof(tls_client_auth_type)*n);
    if(filter->clientAuthTypes==NULL)
        return errSSLAllocate;
    filter->numAuthTypes = n;
    memcpy(filter->clientAuthTypes, auth_types, sizeof(tls_client_auth_type)*n);
    return 0;
}

/* Set the peer public key data, called by the client upon processing the peer cert */
int
tls_handshake_set_peer_rsa_public_key(tls_handshake_t filter, const tls_buffer *modulus, const tls_buffer *exponent)
{
    sslFreePubKey(&filter->peerPubKey);
    return sslGetPubKeyFromBits(modulus, exponent, &filter->peerPubKey);
}

int
tls_handshake_set_peer_ec_public_key(tls_handshake_t filter, tls_named_curve curve, const tls_buffer *public_key)
{
    sslFreePubKey(&filter->peerPubKey);
    return sslGetEcPubKeyFromBits(curve, public_key, &filter->peerPubKey);
}

int
tls_handshake_set_peer_trust(tls_handshake_t filter, tls_handshake_trust_t trust)
{
    filter->peerTrust = trust;
    return 0;
}

int
tls_handshake_set_false_start(tls_handshake_t filter, bool enabled)
{
    assert(!filter->isServer);
    filter->falseStartEnabled = enabled;
    return 0;
}

int
tls_handshake_get_false_start(tls_handshake_t filter, bool *enabled)
{
    assert(!filter->isServer);
    *enabled = filter->falseStartEnabled;
    return 0;
}

int
tls_handshake_set_npn_enable(tls_handshake_t filter, bool enabled)
{
    assert(!filter->isServer);
    filter->npn_enabled = enabled;
    return 0;
}

int
tls_handshake_set_npn_data(tls_handshake_t filter, tls_buffer npnData)
{
    SSLFreeBuffer(&filter->npnOwnData);
    return SSLCopyBuffer(&npnData, &filter->npnOwnData);
}

int
tls_handshake_set_alpn_data(tls_handshake_t filter, tls_buffer alpnData)
{
    SSLFreeBuffer(&filter->alpnOwnData);
    return SSLCopyBuffer(&alpnData, &filter->alpnOwnData);
}

int
tls_handshake_set_server_identity_change(tls_handshake_t filter, bool allowed)
{
    filter->allowServerIdentityChange = allowed;
    return 0;
}

int
tls_handshake_get_server_identity_change(tls_handshake_t filter, bool *allowed)
{
    *allowed = filter->allowServerIdentityChange;
    return 0;
}

int
tls_handshake_set_fallback(tls_handshake_t filter, bool enabled)
{
    filter->fallback = enabled;
    return 0;
}

/* Client only: get the fallback state */
int
tls_handshake_get_fallback(tls_handshake_t filter, bool *enabled)
{
    *enabled = filter->fallback;
    return 0;
}

int
tls_handshake_set_ocsp_enable(tls_handshake_t filter, bool enabled)
{
    filter->ocsp_enabled=enabled;
    return 0;
}

/* Client: set ocsp responder_id_list */
int
tls_handshake_set_ocsp_responder_id_list(tls_handshake_t filter, tls_buffer_list_t *ocsp_responder_id_list)
{
    assert(!filter->isServer);
    tls_free_buffer_list(filter->ocsp_responder_id_list);
    return tls_copy_buffer_list(ocsp_responder_id_list, &filter->ocsp_responder_id_list);
}

/* Client: set ocsp request_extensions */
int
tls_handshake_set_ocsp_request_extensions(tls_handshake_t filter, tls_buffer ocsp_request_extensions)
{
    assert(!filter->isServer);
    SSLFreeBuffer(&filter->ocsp_request_extensions);
    return SSLCopyBuffer(&ocsp_request_extensions, &filter->ocsp_request_extensions);
}

/* Server: set ocsp response data */
int
tls_handshake_set_ocsp_response(tls_handshake_t filter, tls_buffer *ocsp_response)
{
    assert(filter->isServer);
    SSLFreeBuffer(&filter->ocsp_response);
    return SSLCopyBuffer(ocsp_response, &filter->ocsp_response);
}

/* Client: enable SCT extension */
int
tls_handshake_set_sct_enable(tls_handshake_t filter, bool enabled)
{
    assert(!filter->isServer);
    filter->sct_enabled = enabled;
    return 0;
}

/* Server: set SCT list */
int
tls_handshake_set_sct_list(tls_handshake_t filter, tls_buffer_list_t *sct_list)
{
    assert(filter->isServer);
    tls_free_buffer_list(filter->sct_list);
    return tls_copy_buffer_list(sct_list, &filter->sct_list);
}

/* Enables/ Disables Extended Master Secret TLS extension */
int
tls_handshake_set_ems_enable(tls_handshake_t filter, bool enabled)
{
    filter->extMSEnabled = enabled;
    return 0;
}

/* Checks if extended master secret was sent */
bool
tls_handshake_get_negotiated_ems(tls_handshake_t filter)
{
    if (filter->extMSEnabled && filter->extMSReceived)
        return true;
    else
        return false;
}

/* Set TLS user agent string, for diagnostic purposes */
int
tls_handshake_set_user_agent(tls_handshake_t filter, const char *user_agent)
{
    sslFree(filter->userAgent);
    filter->userAgent = NULL;

    if(user_agent) {
        filter->userAgent = sslMalloc(strlen(user_agent)+1);
        strcpy(filter->userAgent, user_agent);
    }

    return 0;
}

/*
 Internal table used to initialize default ciphersuites,
 This establish the preference order, but this is further filtered depending on configuration.


 Order by preference, PFS first, more security first

 Ordered by:
 Key Exchange first: ECDHE_ECDSA, ECDHE_RSA, DHE_RSA, RSA
 then by hash algorithm: SHA384, SHA256, SHA
 then by symmetric cipher: AES_256_GCM, AES_128_GCM, AES_256_CBC, AES_128_CBC, 3DES

 All RC4 ciphersuites are relegated at the end. They are deprecated by the IETF TLS WG.

 'DH_Anon' and 'ECDH_Anon' ciphers are only used in the anymous config.

 The list is filtered based on server support, dtls support, and config if necessary.
 */

static
const uint16_t all_ciphersuites[] = {
    TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_DHE_RSA_WITH_AES_256_GCM_SHA384,
    TLS_DHE_RSA_WITH_AES_128_GCM_SHA256,
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA256,
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA256,
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA,
    SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_RSA_WITH_AES_256_GCM_SHA384,
    TLS_RSA_WITH_AES_128_GCM_SHA256,
    TLS_RSA_WITH_AES_256_CBC_SHA256,
    TLS_RSA_WITH_AES_128_CBC_SHA256,
    TLS_RSA_WITH_AES_256_CBC_SHA,
    TLS_RSA_WITH_AES_128_CBC_SHA,
    SSL_RSA_WITH_3DES_EDE_CBC_SHA,

    /* those are only used by the 'anonymous' config. */
    TLS_ECDH_anon_WITH_AES_256_CBC_SHA,
    TLS_ECDH_anon_WITH_AES_128_CBC_SHA,
    TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA,

    TLS_DH_anon_WITH_AES_256_CBC_SHA256,
    TLS_DH_anon_WITH_AES_256_CBC_SHA,
    TLS_DH_anon_WITH_AES_128_CBC_SHA256,
    TLS_DH_anon_WITH_AES_128_CBC_SHA,
    TLS_DH_anon_WITH_3DES_EDE_CBC_SHA,

};

static const unsigned all_ciphersuites_count = sizeof(all_ciphersuites)/sizeof(all_ciphersuites[0]);

/* Set tls config */
int
tls_handshake_set_config(tls_handshake_t filter, tls_handshake_config_t config)
{
    filter->config = config;

    /* versions: */
    filter->minProtocolVersion = tls_handshake_min_allowed_version(filter, config);
    filter->maxProtocolVersion = tls_handshake_max_allowed_version(filter, config);

    /* DH group size: */
    tls_handshake_set_min_dh_group_size(filter, DEFAULT_MIN_DH_GROUP_SIZE);

    /* Is this a version fallback ? */
    if(!filter->isServer &&
       ((config == tls_handshake_config_TLSv1_fallback) || (config == tls_handshake_config_TLSv1_RC4_fallback)
        || (config == tls_handshake_config_TLSv1_3DES_fallback))
       ) {
        filter->fallback = true;
    } else {
        filter->fallback = false;
    }


    /* ciphersuites: */
    return tls_handshake_set_ciphersuites_internal(filter, config, all_ciphersuites, all_ciphersuites_count);
}

int
tls_handshake_get_config(tls_handshake_t filter, tls_handshake_config_t *config)
{
    *config = filter->config;
    return 0;
}

/* NOTE: The SigAlgs set here are only used to select which SigAlgs to advertise and select a SigAlg
   for private key operations in TLS 1.2, and is mainly here to allow unit testing. 
   If the peer select a SigAlg that we support but was not set here, coreTLS will still proceed and verify 
   signature. 
 */
int
tls_handshake_set_sigalgs(tls_handshake_t filter, const tls_signature_and_hash_algorithm *sigalgs, unsigned n)
{

    unsigned i;
    unsigned count = 0;

    for(i=0;i<n;i++) {
        if(tls_handshake_sigalg_is_supported(sigalgs[i]))
            count++;
    }

    sslFree(filter->localSigAlgs);
    filter->numLocalSigAlgs = 0;

    filter->localSigAlgs = sslMalloc(count*sizeof(tls_signature_and_hash_algorithm));
    if(!filter->localSigAlgs) {
        return errSSLAllocate;
    }

    for(i=0;i<n;i++) {
        if(tls_handshake_sigalg_is_supported(sigalgs[i])) {
            filter->localSigAlgs[filter->numLocalSigAlgs++]=sigalgs[i];
        }
    }

    assert(filter->numLocalSigAlgs == count);

    return 0;
}


/* (re)handshake */
int
tls_handshake_negotiate(tls_handshake_t filter, tls_buffer *peerID)
{
    assert(!filter->isServer);

    if ((filter->state != SSL_HdskStateClientReady) && (filter->state != SSL_HdskStateClientUninit))
    {
        sslDebugLog("Requesting renegotiation while handshake in progress...");
        return errSSLIllegalParam; // TODO: better error code for this case.
    }

    if(peerID) {
        check(filter->peerID.data==NULL); // Note sure that's illegal, but it's fishy
        filter->callbacks->load_session_data(filter->callback_ctx, *peerID, &filter->resumableSession);
        SSLFreeBuffer(&filter->peerID);
        SSLCopyBuffer(peerID, &filter->peerID);
    } else {
        SSLFreeBuffer(&filter->peerID);
    }
    return SSLAdvanceHandshake(SSL_HdskHelloRequest, filter);
}

int
tls_handshake_request_renegotiation(tls_handshake_t filter)
{
    int err;

    if (filter->state != SSL_HdskStateServerReady)
    {
        sslDebugLog("Requesting renegotiation while handshake in progress...");
        return errSSLIllegalParam; // TODO: better error code for this case.
    }

    require_noerr((err=SSLResetFlight(filter)), errOut);
    filter->hdskMessageSeq=0;
    require_noerr((err=SSLPrepareAndQueueMessage(SSLEncodeServerHelloRequest, tls_record_type_Handshake, filter)), errOut);
    require_noerr((err=SSLSendFlight(filter)), errOut);

errOut:
    return err;
}

int
tls_handshake_close(tls_handshake_t filter)
{
	int      err = errSSLSuccess;

    assert(filter);

    err = SSLSendAlert(SSL_AlertLevelWarning, SSL_AlertCloseNotify, filter);

    SSLChangeHdskState(filter, SSL_HdskStateGracefulClose);

    return err;
}


int
tls_handshake_retransmit_timer_expired(tls_handshake_t filter)
{
    assert(filter->isDTLS);

    return DTLSRetransmit(filter);
}

int
tls_handshake_send_alert(tls_handshake_t filter, tls_alert_level_t level, tls_alert_t description)
{
    int err = errSSLSuccess;
    assert(filter != NULL);

    AlertLevel alertLevel = level == tls_handshake_alert_level_fatal ? SSL_AlertLevelFatal : SSL_AlertLevelWarning;
    AlertDescription desc = SSL_AlertCloseNotify;

    switch (description) {
        case tls_handshake_alert_CloseNotify:
            desc = SSL_AlertCloseNotify;
            break;
        case tls_handshake_alert_UnexpectedMsg:
            desc = SSL_AlertUnexpectedMsg;
            break;
        case tls_handshake_alert_BadRecordMac:
            desc = SSL_AlertBadRecordMac;
            break;
        case tls_handshake_alert_DecryptionFail_RESERVED:
            desc = SSL_AlertDecryptionFail_RESERVED;
            break;
        case tls_handshake_alert_RecordOverflow:
            desc = SSL_AlertRecordOverflow;
            break;
        case tls_handshake_alert_DecompressFail:
            desc = SSL_AlertDecompressFail;
            break;
        case tls_handshake_alert_HandshakeFail:
            desc = SSL_AlertHandshakeFail;
            break;
        case tls_handshake_alert_NoCert_RESERVED:
            desc = SSL_AlertNoCert_RESERVED;
            break;
        case tls_handshake_alert_BadCert:
            desc = SSL_AlertBadCert;
            break;
        case tls_handshake_alert_UnsupportedCert:
            desc = SSL_AlertUnsupportedCert;
            break;
        case tls_handshake_alert_CertRevoked:
            desc = SSL_AlertCertRevoked;
            break;
        case tls_handshake_alert_CertExpired:
            desc = SSL_AlertCertExpired;
            break;
        case tls_handshake_alert_CertUnknown:
            desc = SSL_AlertCertUnknown;
            break;
        case tls_handshake_alert_IllegalParam:
            desc = SSL_AlertIllegalParam;
            break;
        case tls_handshake_alert_UnknownCA:
            desc = SSL_AlertUnknownCA;
            break;
        case tls_handshake_alert_AccessDenied:
            desc = SSL_AlertAccessDenied;
            break;
        case tls_handshake_alert_DecodeError:
            desc = SSL_AlertDecodeError;
            break;
        case tls_handshake_alert_DecryptError:
            desc = SSL_AlertDecryptError;
            break;
        case tls_handshake_alert_ExportRestriction_RESERVED:
            desc = SSL_AlertExportRestriction_RESERVED;
            break;
        case tls_handshake_alert_ProtocolVersion:
            desc = SSL_AlertProtocolVersion;
            break;
        case tls_handshake_alert_InsufficientSecurity:
            desc = SSL_AlertInsufficientSecurity;
            break;
        case tls_handshake_alert_InternalError:
            desc = SSL_AlertInternalError;
            break;
        case tls_handshake_alert_InappropriateFallback:
            desc = SSL_AlertInappropriateFallback;
            break;
        case tls_handshake_alert_UserCancelled:
            desc = SSL_AlertUserCancelled;
            break;
        case tls_handshake_alert_NoRenegotiation:
            desc = SSL_AlertNoRenegotiation;
            break;
        case tls_handshake_alert_UnsupportedExtension:
            desc = SSL_AlertUnsupportedExtension;
            break;
        default:
            err = errSSLParam;
            break;
    }

    if (err == errSSLSuccess) {
        err = SSLSendAlert(alertLevel, desc, filter);
    }

    return err;
}

tls_protocol_version
tls_handshake_get_negotiated_protocol_version(tls_handshake_t filter)
{
    return filter->negProtocolVersion;
}

uint16_t
tls_handshake_get_negotiated_cipherspec(tls_handshake_t filter)
{
    return filter->selectedCipher;
}

uint16_t
tls_handshake_get_negotiated_curve(tls_handshake_t filter)
{
    return filter->ecdhPeerCurve;
}

const uint8_t *
tls_handshake_get_server_random(tls_handshake_t filter)
{
    return filter->serverRandom;
}

const uint8_t *
tls_handshake_get_client_random(tls_handshake_t filter)
{
    return filter->clientRandom;
}

const uint8_t *
tls_handshake_get_master_secret(tls_handshake_t filter)
{
    return filter->masterSecret;
}

bool
tls_handshake_get_session_proposed(tls_handshake_t filter, tls_buffer *sessionID)
{
    if(sessionID) {
        sessionID->data = filter->proposedSessionID.data;
        sessionID->length = filter->proposedSessionID.length;
    }
    return (filter->proposedSessionID.data!=NULL);
}

bool
tls_handshake_get_session_match(tls_handshake_t filter, tls_buffer *sessionID)
{
    if(sessionID) {
        sessionID->data = filter->sessionID.data;
        sessionID->length = filter->sessionID.length;
    }
    return filter->sessionMatch;
}

const SSLCertificate *
tls_handshake_get_peer_certificates(tls_handshake_t filter)
{
    return filter->peerCert;
}

const tls_buffer *
tls_handshake_get_sni_hostname(tls_handshake_t filter)
{
    return &filter->peerDomainName;
}

const uint16_t *
tls_handshake_get_peer_requested_ciphersuites(tls_handshake_t filter, unsigned *num)
{
    assert(num);
    *num = filter->numRequestedCipherSuites;
    return filter->requestedCipherSuites;
}

const tls_signature_and_hash_algorithm *
tls_handshake_get_peer_signature_algorithms(tls_handshake_t filter, unsigned *num)
{
    assert(num);
    *num = filter->numPeerSigAlgs;
    return filter->peerSigAlgs;
}

const tls_client_auth_type *
tls_handshake_get_peer_acceptable_client_auth_type(tls_handshake_t filter, unsigned *num)
{
    assert(num);
    assert(!filter->isServer);

    *num = filter->numAuthTypes;
    return filter->clientAuthTypes;
}

const DNListElem *
tls_handshake_get_peer_acceptable_dn_list(tls_handshake_t filter)
{
    assert(!filter->isServer);
    return filter->acceptableDNList;
}

const tls_buffer *
tls_handshake_get_peer_psk_identity_hint(tls_handshake_t filter)
{
    assert(!filter->isServer);
    return NULL;
}

const tls_buffer *
tls_handshake_get_peer_psk_identity(tls_handshake_t filter)
{
    assert(filter->isServer);
    return &filter->pskIdentity;
}

const tls_buffer *
tls_handshake_get_peer_npn_data(tls_handshake_t filter)
{
    if(filter->npn_received)
        return &filter->npnPeerData;
    else
        return NULL;
}

const tls_buffer *
tls_handshake_get_peer_alpn_data(tls_handshake_t filter)
{
    if(filter->alpn_received)
        return &filter->alpnPeerData;
    else
        return NULL;
}

bool
tls_handshake_get_peer_ocsp_enabled(tls_handshake_t filter)
{
    return filter->ocsp_peer_enabled;
}


const tls_buffer *
tls_handshake_get_peer_ocsp_response(tls_handshake_t filter)
{
    if(filter->ocsp_response_received)
        return &filter->ocsp_response;
    else
        return NULL;
}

const tls_buffer_list_t *
tls_handshake_get_peer_ocsp_responder_id_list(tls_handshake_t filter)
{
    return filter->ocsp_responder_id_list;
}

const tls_buffer *
tls_handshake_get_peer_ocsp_request_extensions(tls_handshake_t filter)
{
    if(filter->ocsp_peer_enabled)
        return &filter->ocsp_request_extensions;
    else
        return NULL;
}

bool
tls_handshake_get_peer_sct_enabled(tls_handshake_t filter)
{
    assert(filter->isServer);
    return filter->sct_peer_enabled;
}

const tls_buffer_list_t *
tls_handshake_get_peer_sct_list(tls_handshake_t filter)
{
    assert(!filter->isServer);
    return filter->sct_list;
}

const uint16_t *
tls_handshake_get_peer_requested_ecdh_curves(tls_handshake_t filter, unsigned *num)
{
    assert(num);
    *num = filter->num_ec_curves;
    return filter->requested_ecdh_curves;
}

/* Special functions for EAP-FAST */
/* http://tools.ietf.org/html/rfc4851 */
int
tls_handshake_internal_master_secret(tls_handshake_t ctx,
                                     void *secret,        // mallocd by caller, SSL_MASTER_SECRET_SIZE
                                     size_t *secretSize)  // in/out
{
    if(*secretSize < SSL_MASTER_SECRET_SIZE) {
        return errSSLParam;
    }
    memmove(secret, ctx->masterSecret, SSL_MASTER_SECRET_SIZE);
    *secretSize = SSL_MASTER_SECRET_SIZE;
    return errSSLSuccess;
}

int
tls_handshake_internal_server_random(tls_handshake_t ctx,
                                     void *randBuf, 			// mallocd by caller, SSL_CLIENT_SRVR_RAND_SIZE
                                     size_t *randSize)	// in/out
{
    if(*randSize < SSL_CLIENT_SRVR_RAND_SIZE) {
        return errSSLParam;
    }
    memmove(randBuf, ctx->serverRandom, SSL_CLIENT_SRVR_RAND_SIZE);
    *randSize = SSL_CLIENT_SRVR_RAND_SIZE;
    return errSSLSuccess;
}

int
tls_handshake_internal_client_random(tls_handshake_t ctx,
                                     void *randBuf,  	// mallocd by caller, SSL_CLIENT_SRVR_RAND_SIZE
                                     size_t *randSize)	// in/out
{
    if(*randSize < SSL_CLIENT_SRVR_RAND_SIZE) {
        return errSSLParam;
    }
    memmove(randBuf, ctx->clientRandom, SSL_CLIENT_SRVR_RAND_SIZE);
    *randSize = SSL_CLIENT_SRVR_RAND_SIZE;
    return errSSLSuccess;
}

/*
 * Register a callback for obtaining the master_secret when performing
 * PAC-based session resumption.
 */
int
tls_handshake_internal_set_master_secret_function(tls_handshake_t ctx, tls_handshake_master_secret_function_t mFunc, const void *arg)
{
    ctx->masterSecretCallback = mFunc;
    ctx->masterSecretArg = arg;
    return 0;
}

/*
 * Provide an opaque SessionTicket for use in PAC-based session
 * resumption. Client side only. The provided ticket is sent in
 * the ClientHello message as a SessionTicket extension.
 *
 * We won't reject this on the server side, but server-side support
 * for PAC-based session resumption is currently enabled for
 * Development builds only. To fully support this for server side,
 * besides the rudimentary support that's here for Development builds,
 * we'd need a getter for the session ticket, so the app code can
 * access the SessionTicket when its SSLInternalMasterSecretFunction
 * callback is called.
 */
int tls_handshake_internal_set_session_ticket(tls_handshake_t ctx, const void *ticket, size_t ticketLength)
{
    if(ticketLength > 0xffff) {
        /* extension data encoded with a 2-byte length! */
        return errSSLParam;
    }
    SSLFreeBuffer(&ctx->externalSessionTicket);
    return SSLCopyBufferFromData(ticket, ticketLength, &ctx->externalSessionTicket);
}
