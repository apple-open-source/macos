//
//  tls_01_record.c
//  coretls
//

#include <stdio.h>
#include <stdlib.h>
#include <AssertMacros.h>

#include <tls_record.h>
#include <tls_ciphersuites.h>
#include "CipherSuite.h"

/* extern struct ccrng_state *ccDRBGGetRngState(); */
#include <CommonCrypto/CommonRandomSPI.h>
#define CCRNGSTATE() ccDRBGGetRngState()

#include "tls_regressions.h"

static
int record_test(uint16_t cipher, tls_protocol_version pv, size_t write_size, bool split)
{
    int err=0;

    uint8_t key_data[256] = {0,};

    // For 3DES, the keys *cannot* be the same. So we must make each key unique. 
    for (int i = 0; i < 256; i++) {
        key_data[i] = i;
    }

    tls_buffer key = {
        .data = key_data,
        .length = sslCipherSuiteGetKeydataSize(cipher),
    };

    if(key.length>sizeof(key_data)) {
        abort();
    }

    struct ccrng_state *rng = CCRNGSTATE();
    if (!rng) {
        abort();
    }

    tls_record_t client = tls_record_create(true, rng);
    tls_record_t server = tls_record_create(true, rng);

    test_log_start();
    test_printf("Test case: cipher=%04x, pv=%04x, write_size=%zd, split=%d\n", cipher, pv, write_size, split);

    require_action(client, out, err=-1);
    require_action(server, out, err=-1);

    require_noerr((err=tls_record_set_record_splitting(client, split)), out);
    require_noerr((err=tls_record_set_protocol_version(client, pv)), out);
    require_noerr((err=tls_record_set_protocol_version(server, pv)), out);

    require_noerr((err=tls_record_init_pending_ciphers(client, cipher, false, key)), out);
    require_noerr((err=tls_record_init_pending_ciphers(server, cipher, true, key)), out);

    require_noerr((err=tls_record_advance_write_cipher(client)), out);
    require_noerr((err=tls_record_advance_read_cipher(server)), out);


    tls_buffer inData, outData, decData;
    uint8_t content_type;

    inData.data=malloc(write_size);
    inData.length=write_size;

    for (int i=tls_record_type_Alert; i<=tls_record_type_AppData; i++) {
        size_t declen = 0;
        size_t enclen = tls_record_encrypted_size(client, i, inData.length);
        tls_buffer encData;

        outData.data = malloc(enclen);
        outData.length = enclen;

        test_printf("encrypted: allocated size = %lu\n", outData.length);

        require_noerr((err=tls_record_encrypt(client, inData, i, &outData)), out);

        test_printf("encrypted: actual len = %lu\n", outData.length);

        require_action(outData.length==enclen, out, err=-1);

        encData.data = outData.data;
        encData.length = outData.length;
        while(encData.length) {
            size_t reclen;
            require_noerr((err=tls_record_parse_header(server, encData, &reclen, &content_type)), out);

            reclen += tls_record_get_header_size(server);

            test_printf("decrypted: record len = %lu\n", reclen);

            decData.length=tls_record_decrypted_size(server, reclen);
            decData.data=malloc(decData.length);

            require_action(decData.data, out, err=-1);

            test_printf("decrypted: allocated size = %lu\n", decData.length);

            require_noerr((err=tls_record_decrypt(server, encData, &decData, &content_type)), out);

            test_printf("decrypted: fragment len = %lu\n", decData.length);

            declen += decData.length;
            require_action(content_type==i, out, err=-1);

            free(decData.data);

            encData.data+=reclen;
            encData.length-=reclen;
        }

        test_printf("decrypted: total len = %lu\n", declen);

        require_action(declen==inData.length, out, err=-1);

        free(outData.data);
    }

    free(inData.data);

out:
    if(client) {
        tls_record_destroy(client);
    }
    if(server) {
        tls_record_destroy(server);
    }

    ok(!err, "Test case: cipher=%04x, pv=%04x, write_size=%zd, split=%d, err = %d", cipher, pv, write_size, split, err);
    test_log_end(err);
    return err;
}

static
int gcm_size_test(size_t write_size)
{
    int err=0;
    uint16_t cipher = TLS_RSA_WITH_AES_128_GCM_SHA256;
    tls_protocol_version pv = tls_protocol_version_TLS_1_2;

    uint8_t key_data[256] = {0,};

    tls_buffer key = {
        .data = key_data,
        .length = sslCipherSuiteGetKeydataSize(cipher),
    };

    if(key.length>sizeof(key_data)) {
        abort();
    }

    struct ccrng_state *rng = CCRNGSTATE();
    if (!rng) {
        abort();
    }

    tls_record_t client = tls_record_create(false, rng);
    tls_record_t server = tls_record_create(false, rng);

    test_log_start();
    test_printf("Test case: gcm size test cipher=%04x, pv=%04x, write_size=%zd\n", cipher, pv, write_size);

    require_action(client, out, err=-1);
    require_action(server, out, err=-1);

    require_noerr((err=tls_record_set_protocol_version(client, pv)), out);
    require_noerr((err=tls_record_set_protocol_version(server, pv)), out);

    /* We don't call tls_record_init_pending_ciphers on client so the records are not actually encrypted. This is used to generate payload smaller than the minimum size required for AES-GCM encrypted record, so we can test for rdar://26994467 */
    require_noerr((err=tls_record_init_pending_ciphers(server, cipher, true, key)), out);

    require_noerr((err=tls_record_advance_read_cipher(server)), out);


    tls_buffer inData, outData, decData;
    uint8_t content_type;

    inData.data=malloc(write_size);
    inData.length=write_size;

    size_t enclen = tls_record_encrypted_size(client, tls_record_type_AppData, inData.length);
    tls_buffer encData;

    outData.data = malloc(enclen);
    outData.length = enclen;

    test_printf("encrypted: allocated size = %lu\n", outData.length);

    require_noerr((err=tls_record_encrypt(client, inData, tls_record_type_AppData, &outData)), out);

    test_printf("encrypted: actual len = %lu\n", outData.length);

    require_action(outData.length==enclen, out, err=-1);

    encData.data = outData.data;
    encData.length = outData.length;
    size_t reclen;
    require_noerr((err=tls_record_parse_header(server, encData, &reclen, &content_type)), out);

    reclen += tls_record_get_header_size(server);

    test_printf("decrypted: record len = %lu\n", reclen);

    decData.length=tls_record_decrypted_size(server, reclen);
    decData.data=malloc(decData.length);

    require_action(decData.data, out, err=-1);

    test_printf("decrypted: allocated size = %lu\n", decData.length);

    err=tls_record_decrypt(server, encData, &decData, &content_type);

    free(decData.data);
    free(outData.data);
    free(inData.data);

out:
    if(client) {
        tls_record_destroy(client);
    }
    if(server) {
        tls_record_destroy(server);
    }
    if (write_size == 24)
        ok(err==-10006, "Test case: cipher=%04x, pv=%04x, write_size=%zd, err = %d", cipher, pv, write_size, err);
    else
        ok(err==-10008, "Test case: cipher=%04x, pv=%04x, write_size=%zd, err = %d", cipher, pv, write_size, err);

    test_log_end(err);
    return err;
}

typedef struct _CipherSuiteName {
    uint16_t cipher;
    const char *name;
} CipherSuiteName;

#define CIPHER(cipher) { cipher, #cipher},

static const CipherSuiteName ssl_ciphers[] = {
    //SSL_NULL_WITH_NULL_NULL, unsupported
    CIPHER(SSL_RSA_WITH_NULL_SHA)
    CIPHER(SSL_RSA_WITH_NULL_MD5)
    CIPHER(SSL_RSA_WITH_RC4_128_MD5)
    CIPHER(SSL_RSA_WITH_RC4_128_SHA)
    CIPHER(SSL_RSA_WITH_3DES_EDE_CBC_SHA)
    CIPHER(TLS_RSA_WITH_AES_128_CBC_SHA)
    CIPHER(TLS_RSA_WITH_AES_256_CBC_SHA)
};
static int n_ssl_ciphers = sizeof(ssl_ciphers)/sizeof(ssl_ciphers[0]);

static const CipherSuiteName tls_ciphers[] = {
    CIPHER(TLS_RSA_WITH_NULL_SHA256)
    CIPHER(TLS_RSA_WITH_AES_128_CBC_SHA256)
    CIPHER(TLS_RSA_WITH_AES_256_CBC_SHA256)
    CIPHER(TLS_RSA_WITH_AES_128_GCM_SHA256)
    CIPHER(TLS_RSA_WITH_AES_256_GCM_SHA384)
    CIPHER(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256)
    CIPHER(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384)
};
static int n_tls_ciphers = sizeof(tls_ciphers)/sizeof(tls_ciphers[0]);

static int protos[]={
    tls_protocol_version_SSL_3,
    tls_protocol_version_TLS_1_0,
    tls_protocol_version_TLS_1_1,
    tls_protocol_version_TLS_1_2,
};
static int n_protos = sizeof(protos)/sizeof(protos[0]);


static size_t wsizes[] = {
                            0, 1, 2, 3, 4,
                           16384, 16385, 16386, 16387, 16388,
                           32768, 32769, 32770, 32771, 32772, 32773
};

static int nwsizes = sizeof(wsizes)/sizeof(wsizes[0]);

int tls_01_record(int argc, char * const argv[])
{
    plan_tests(nwsizes*2 * (n_ssl_ciphers*n_protos + n_tls_ciphers*(n_protos-1)) + 24);

    int i,j,k;

    for (i=1; i<=24; i++)
        gcm_size_test(i);

    for(k=0; k<nwsizes; k++) {

        for(i=0; i<n_ssl_ciphers; i++) {
            for(j=0; j<n_protos; j++) {
                record_test(ssl_ciphers[i].cipher, protos[j], wsizes[k] ,false);
                record_test(ssl_ciphers[i].cipher, protos[j], wsizes[k] ,true);
            }
        }

        for(i=0; i<n_tls_ciphers; i++) {
            for(j=1; j<n_protos; j++) {
                record_test(tls_ciphers[i].cipher, protos[j], wsizes[k] ,false);
                record_test(tls_ciphers[i].cipher, protos[j], wsizes[k] ,true);
            }
        }

    }

    return 0;
}
