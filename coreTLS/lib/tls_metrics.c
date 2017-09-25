//
//  tls_metrics.c
//  coretls
//

#include "tls_metrics.h"
#include "tls_handshake_priv.h"
#include "sslSession.h"

#include <stdlib.h>
#include <syslog.h>
#include <asl.h>

#include <System/sys/codesign.h>
#include <unistd.h>
#include <errno.h>
#include <dispatch/dispatch.h>

/*
  TLS diagnostic events:

    TLS diagnostic events are identified by an event name.
    Events also have a type (dtls or tls) and an origin which identitify the process generating the event.

    They do also include multiple attributes. Attributes may have a string value or be a boolean.
    For example, 'pv' is an attribute whose value is the protocol version, while 'alpn_received'
    is a boolean attribute.

  OSX Reporting:

    On OSX, events are reported by logging a MessageTracer(MT) message for the domain com.apple.coretls.<event_name> which
    include all the boolean attribute in an MT attribute of the form com.apple.message.value_<attribute_name>, and for
    each event attribute with a value a MessageTracer message for the domain com.apple.coretls.<event_name>.<attribute_name>
    and the attribute com.apple.message.signature3 to contain the attribute value.
    All messages use com.apple.message.signature for the event type, and com.apple.message.signature2 for the origin.
    Those MT message are all summarized by MT.

  iOS Reporting:

    On iOS, events are reported using aggregated counters of the form: com.apple.coretls.<event>.<type>.<attribute>[=value].<origin>


  For ease of parsing the results, event names and attributes names should not include dots ('.') or equal signs ('=')
  Origin may include dots. By default the bundleID of the process is used. If not available the process name is used.
  Clients of coreTLS can override the origin using the tls_handshake_set_user_agent() function.


  Integration with the TLS stack:

    tls_metric.h export high level even reporting functions that take as an input an handshake object and are called at
    the appropriate place in the TLS stack.

    Each of this high level functions define the attributes for the event being reported. In each function, an event is
    initialized by calling tls_metric_event_new(), attributes are added to the event by calling tls_metric_event_add_bool()
    and tls_metric_event_add_string() as many times as necessary. The event must be finalized by calling tls_metric_event_done()

 */

/***** sampling rates *****/

#define FINISHED_SAMPLING_RATE 10
#define DESTROYED_SAMPLING_RATE 1

static
bool sampling(int percent)
{
    return arc4random_uniform(100)<percent;
}

/***** process identification *****/

struct csheader {
    uint32_t magic;
    uint32_t length;
};

static
char *csops_identifier_data(void)
{
    char *csops_data = NULL;
    struct csheader header;
    uint32_t bufferlen;
    int ret;

    ret = csops(getpid(), CS_OPS_IDENTITY, &header, sizeof(header));
    if (ret != -1 || errno != ERANGE)
        return NULL;

    bufferlen = ntohl(header.length);
    /* check for insane values */
    if (bufferlen > 1024 || bufferlen < 8) {
        return NULL;
    }
    csops_data = malloc(bufferlen + 1);
    if (csops_data == NULL) {
        return NULL;
    }
    ret = csops(getpid(), CS_OPS_IDENTITY, csops_data, bufferlen);
    if (ret) {
        free(csops_data);
        return NULL;
    }

    csops_data[bufferlen] = 0;

    return csops_data;
}

#define _I_NEED_TLS_METRICS_BUNDLES_INC_ 1
#include "tls_metrics_bundles.inc"

static const char *com_apple_prefix = "com.apple.";

/* Return true if bundle starts with com.apple. or is part of the top_bundles */
static bool bundle_id_match(const char *bundle)
{
    int i;

    if(strncmp(bundle, com_apple_prefix, strlen(com_apple_prefix)) == 0)
       return true;

    for(i=0; i<top_bundles_n; i++) {
        if(strcmp(top_bundles[i], bundle)==0) {
            return true;
        }
    }

    return false;
}


static char *process_identifier_data = NULL;

static
const char *process_identifier(void)
{
    static dispatch_once_t __csops_once;

    dispatch_once(&__csops_once, ^{
        char *csops_data = csops_identifier_data();
        if(csops_data) {
            if(bundle_id_match(csops_data+8)) {
                process_identifier_data = strdup(csops_data+8);
            } else {
                process_identifier_data = "redacted_bundle_id";
            }
        } else {
            process_identifier_data = "no_bundle_id";
        }
        free(csops_data);
    });

    return process_identifier_data;
}


/***** logging to MessageTracer or AggregateDictionary *****/

#if !TARGET_OS_IPHONE
static
__attribute__((format(printf, 2, 3)))
void asl_set_bool_with_format(asl_object_t m, const char *key,...)
{
    va_list ap;
    char *str = NULL;

    va_start(ap, key);
    vasprintf(&str, key, ap);

    if(str) {
        asl_set(m, str, "1");
        free(str);
    }
}

static
__attribute__((format(printf, 3, 4)))
void asl_set_string_with_format(asl_object_t m, const char *key, const char *f, ...)
{
    va_list ap;
    char *str = NULL;

    va_start(ap, f);
    vasprintf(&str, f, ap);

    if(str) {
        asl_set(m, key, str);
        free(str);
    }
}

#endif

#if TARGET_OS_IPHONE && ! TARGET_OS_SIMULATOR

#include <xpc/xpc.h>
#include <xpc/private.h>

// XPC message keys
#define kADXPCOperationKey "operation"
#define kADXPCKeyStringKey "key"
#define kADXPCValueKey     "value"
#define kADXPCKeyBatchAddKeys "batch-add"
#define kADXPCKeyBatchSetKeys "batch-set"

typedef enum {
    ADXPCOperationCommit,
    ADXPCOperationCheckpoint,
    ADXPCOperationSignificantTimeChanged,
    ADXPCOperationDeleteScalarKey,
    ADXPCOperationSetValueForScalarKey,
    ADXPCOperationAddValueForScalarKey,
    ADXPCOperationDeleteDistributionKey,
    ADXPCOperationSetValueForDistributionKey,
    ADXPCOperationPushValueForDistributionKey,
    ADXPCOperationGetLogsMessages,
    ADXPCOperationSetLogsMessages,
    ADXPCOperationDebugSchedule,
    ADXPCOperationBatchKeys,
} ADXPCOperation;

static
dispatch_queue_t queue() {
    static dispatch_once_t __once;
    static dispatch_queue_t __queue;

    dispatch_once(&__once, ^{
        dispatch_queue_attr_t attr = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_BACKGROUND, 0);
        __queue = dispatch_queue_create("com.apple.aggregated.requestQueue", attr);
    });

    return __queue;
}

static
xpc_connection_t connection() {
    static dispatch_once_t __once;
    static xpc_connection_t __connection;

    dispatch_once(&__once, ^{
        __connection = xpc_connection_create_mach_service("com.apple.aggregated", queue(), 0);

        xpc_connection_set_event_handler(__connection, ^(xpc_object_t event) {
            const char *description = xpc_dictionary_get_string(event, XPC_ERROR_KEY_DESCRIPTION);
            fprintf(stderr, "error in %s: %s\n", __func__, description);
        });

        xpc_connection_resume(__connection);
    });

    return __connection;
}

static
void sendAsyncMessageWithIntValue(ADXPCOperation operation, const char *key, int64_t value) {
    xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_int64(request, kADXPCOperationKey, operation);
    xpc_dictionary_set_int64(request, kADXPCValueKey, value);

    if (key) {
        xpc_dictionary_set_string(request, kADXPCKeyStringKey, key);
    }

    xpc_connection_send_notification(connection(), request);

    xpc_release(request);
}

static
__attribute__((format(printf, 1, 2)))
void ADClientIncValueForScalarKeyWithFormat(const char *fmt, ...)
{
    va_list ap;
    char *key = NULL;

    va_start(ap, fmt);
    vasprintf(&key, fmt, ap);

    if(key) {
        sendAsyncMessageWithIntValue(ADXPCOperationAddValueForScalarKey, key, 1);
        free(key);
    }
}

#endif

#if TARGET_OS_IPHONE && TARGET_OS_SIMULATOR
static
__attribute__((format(printf, 1, 2)))
void ADClientIncValueForScalarKeyWithFormat(const char *fmt, ...)
{
    return;
}
#endif

typedef struct {
    const char *name;
    const char *type;  // tls vs dtls
    const char *origin;
#if !TARGET_OS_IPHONE
    aslmsg m;
#else
    const char *prefix;
#endif
} tls_metric_event_t;

static
void tls_metric_event_add_bool(tls_metric_event_t *event, const char *name)
{
#if !TARGET_OS_IPHONE
    asl_set_bool_with_format(event->m, "com.apple.message.value_%s", name);
#else
    ADClientIncValueForScalarKeyWithFormat("com.apple.coretls.%s.%s.%s.%s", event->name, event->type, name, event->origin);
#endif
}

static
__attribute__((format(printf, 3, 4)))
void tls_metric_event_add_string(tls_metric_event_t *event, const char *name, const char *fmt, ...)
{
    va_list ap;
    char *value = NULL;

    va_start(ap, fmt);
    vasprintf(&value, fmt, ap);

    if(value) {
#if !TARGET_OS_IPHONE
        asl_object_t m = asl_new(ASL_TYPE_MSG);
        if(m) {
            asl_set_string_with_format(m, "com.apple.message.domain", "com.apple.coretls.%s.%s",event->name, name);
            asl_set(m, "com.apple.message.signature", event->type);
            asl_set(m, "com.apple.message.signature2", event->origin);
            asl_set(m, "com.apple.message.signature3", value);
            asl_set(m, "com.apple.message.summarize", "YES");
            asl_log(NULL, m, ASL_LEVEL_NOTICE, "");
            asl_free(m);
        }
#else
        ADClientIncValueForScalarKeyWithFormat("com.apple.coretls.%s.%s.%s=%s.%s",
                                               event->name, event->type, name, value, event->origin);
#endif
        free(value);
    }
}

static
tls_metric_event_t *tls_metric_event_new(const char *name, const char *origin, const char *type)
{
    tls_metric_event_t *event = malloc(sizeof(tls_metric_event_t));
    require(event, out);

    event->name = name;
    event->type = type;
    event->origin = origin;

#if !TARGET_OS_IPHONE
    event->m = asl_new(ASL_TYPE_MSG);
    require(event->m, out);
    asl_set_string_with_format(event->m, "com.apple.message.domain", "com.apple.coretls.%s",event->name);
    asl_set_string_with_format(event->m, "com.apple.message.signature", "%s", event->type);
    asl_set_string_with_format(event->m, "com.apple.message.signature2", "%s", event->origin);
#endif

    tls_metric_event_add_bool(event, "events");

out:
    return event;
}

static
void tls_metric_event_done(tls_metric_event_t *event)
{
#if !TARGET_OS_IPHONE
    asl_set(event->m, "com.apple.message.summarize", "YES");
    asl_log(NULL, event->m, ASL_LEVEL_NOTICE, "");
    asl_free(event->m);
#endif

    free(event);
}


/***** Actual mesaured events *****/

/* Called when an new handshake (not resumed) is finished on the client side */
void tls_metric_client_finished(tls_handshake_t hdsk)
{
    int dhe_bucket = 0;
    const char *key_type = NULL;

    const char *curve = NULL;
    const char *neg_client_cert = NULL;

    bool req_client_cert_rsa = false;
    bool req_client_cert_ecc = false;
    bool req_client_cert_other = false;

    if(!sampling(FINISHED_SAMPLING_RATE))
        return;

    KeyExchangeMethod kem = sslCipherSuiteGetKeyExchangeMethod(hdsk->selectedCipher);

    if(kem==SSL_DHE_RSA && hdsk->dhParams!=NULL) {
        size_t dhlen=ccdh_gp_prime_bitlen(hdsk->dhParams);
        if(dhlen<512) dhe_bucket = 1;
        else if(dhlen<768) dhe_bucket = 2;
        else if(dhlen<1024) dhe_bucket = 3;
        else if(dhlen<2048) dhe_bucket = 4;
        else dhe_bucket = 5;
    }

    if(kem==SSL_ECDHE_RSA || kem==SSL_ECDHE_ECDSA) {
        switch(hdsk->ecdhPeerCurve) {
            case tls_curve_none:
                break;
            case tls_curve_secp256r1:
                curve = "p256";
                break;
            case tls_curve_secp384r1:
                curve = "p384";
                break;
            case tls_curve_secp521r1:
                curve = "p521";
                break;
            default:
                curve = "other";
                break;
        }
    }

    for(int i=0; i<hdsk->numAuthTypes; i++) {
        switch(hdsk->clientAuthTypes[i]) {
            case tls_client_auth_type_RSASign:
                req_client_cert_rsa = true;
                break;
            case tls_client_auth_type_ECDSASign:
                req_client_cert_ecc = true;
                break;
            default:
                req_client_cert_other = true;
                break;
        }
    }

    switch(hdsk->negAuthType) {
        case tls_client_auth_type_None:
            break;
        case tls_client_auth_type_RSASign:
            neg_client_cert = "rsa";
            break;
        case tls_client_auth_type_ECDSASign:
            neg_client_cert = "ecc";
            break;
        default:
            neg_client_cert = "other";
            break;
    }

    if(hdsk->peerPubKey.rsa!=NULL) {
        if(hdsk->peerPubKey.isRSA) {
            key_type = "rsa";
        } else {
            key_type = "ecc";
        }
    } else {
        key_type = "none";
    }

#define COUNT(x, y) if(x) tls_metric_event_add_bool(e, y)
#define HDSK_COUNT(x, y) if(hdsk->x) tls_metric_event_add_bool(e, y)

    tls_metric_event_t *e = tls_metric_event_new("client_finished", process_identifier(), hdsk->isDTLS?"dtls":"tls");

    if(e) {
        tls_metric_event_add_string(e, "config", "%d", hdsk->config);
        tls_metric_event_add_string(e, "pv", "%04x", hdsk->negProtocolVersion);
        tls_metric_event_add_string(e, "cs", "%04x", hdsk->selectedCipher);
        tls_metric_event_add_string(e, "key_type", "%s", key_type);

        if(hdsk->kxSigAlg.hash || hdsk->kxSigAlg.signature) {
            tls_metric_event_add_string(e, "kxSigAlg", "%02x_%02x", hdsk->kxSigAlg.hash, hdsk->kxSigAlg.signature);
        }

        if(hdsk->certSigAlg.hash || hdsk->certSigAlg.signature) {
            tls_metric_event_add_string(e, "certSigAlg", "%02x_%02x", hdsk->certSigAlg.hash, hdsk->certSigAlg.signature);
        }

        if(dhe_bucket)
            tls_metric_event_add_string(e, "dhe_bucket", "%d", dhe_bucket);

        if(curve)
            tls_metric_event_add_string(e, "curve", "%s", curve);

        if(neg_client_cert)
            tls_metric_event_add_string(e, "neg_client_cert", "%s", neg_client_cert);

        COUNT(req_client_cert_rsa, "req_client_cert_rsa");
        COUNT(req_client_cert_ecc, "req_client_cert_ecc");
        COUNT(req_client_cert_other, "req_client_cert_other");

        HDSK_COUNT(npn_confirmed, "npn_confirmed");
        HDSK_COUNT(alpn_received, "alpn_received");
        HDSK_COUNT(ocsp_peer_enabled, "ocsp_peer_enabled");
        HDSK_COUNT(ocsp_response_received, "ocsp_response_received");
        HDSK_COUNT(sct_peer_enabled, "sct_peer_enabled");
        HDSK_COUNT(sct_list, "sct_list");
        HDSK_COUNT(sessionTicket_confirmed, "sessionticket_confirmed");

        if(hdsk->npnPeerData.length)
            tls_metric_event_add_bool(e, "npnpeerdata");
        if(hdsk->sessionTicket.length)
            tls_metric_event_add_bool(e, "sessionticket");

        tls_metric_event_done(e);
    }
}

void tls_metric_destroyed(tls_handshake_t hdsk)
{
    if(!sampling(DESTROYED_SAMPLING_RATE))
        return;

    tls_metric_event_t *e = tls_metric_event_new(hdsk->isServer?"server_released":"client_released", process_identifier(), hdsk->isDTLS?"dtls":"tls");
    tls_metric_event_done(e);
}
