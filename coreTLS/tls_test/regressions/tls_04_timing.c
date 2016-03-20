//
//  tls_04_timing.c
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

#define _printf printf

#include <mach/mach_time.h>
#include <time.h>


static uint64_t getcputime(void)
{
#if 0
    uint64_t t;
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);

    t = (uint64_t)ru.ru_utime.tv_sec*1000000ULL+ru.ru_utime.tv_usec;

    return t;
#else
    return mach_absolute_time();

#endif
}

#define perf_start() uint64_t _perf_time = getcputime();
//#define perf_scale_factor() ({struct mach_timebase_info info; mach_timebase_info(&info); ((double)info.numer) / (1000000000.0 * info.denom);})
#define perf_time() ((getcputime() - _perf_time))

uint8_t muxb(int s, uint8_t a, uint8_t b);
uint8_t muxb(int s, uint8_t a, uint8_t b)
{
    uint8_t cond =~((uint8_t)s-(uint8_t)1);//s?~zero:zero; see above
    uint8_t rc = (cond&a)|(~cond&b);
    return rc;
}

uint64_t mux64(int s, uint64_t a, uint64_t b);
uint64_t mux64(int s, uint64_t a, uint64_t b)
{
    uint64_t cond =~((uint64_t)s-(uint64_t)1);//s?~zero:zero; see above
    uint64_t rc = (cond&a)|(~cond&b);
    //printf("%016llx %016llx %016llx %016llx\n", cond, a, b, rc);
    return rc;
}


static
int decrypt_one(size_t loop, tls_record_t server, tls_buffer C0, tls_buffer C1, uint64_t *t0, uint64_t *t1)
{
    uint8_t content_type;
    size_t reclen;
    int err;
    tls_buffer decData;
    uint64_t t;

    require_noerr((err=tls_record_parse_header(server, C0, &reclen, &content_type)), out);

    reclen += tls_record_get_header_size(server);

    //_printf("decrypted: record len = %lu\n", reclen);

    decData.length=tls_record_decrypted_size(server, reclen);
    decData.data=malloc(decData.length);

    require_action(decData.data, out, err=-1);

    //_printf("decrypted: allocated size = %lu\n", decData.length);

    tls_buffer copyData;

    copyData.data = malloc(C0.length);
    copyData.length = C0.length;

    for(size_t i=0;i<C0.length; i++) {
        uint8_t a = C0.data[i];
        uint8_t b = C1.data[i];
        copyData.data[i] = muxb(loop&1, b, a);
    }

    perf_start();
    err=tls_record_decrypt(server, copyData, &decData, &content_type);
    t = perf_time();

    t0[loop/2] = mux64((int)(loop&1), t, t0[loop/2]);
    t1[loop/2] = mux64((int)(loop&1), t1[loop/2], t);

    free(copyData.data);

    //require_action(content_type==tls_record_type_AppData, out, err=-1);

    free(decData.data);

out:
    return err;
}
#if 0
static
int decrypt_M0(tls_record_t server, tls_buffer encData, uint64_t *t)
{
    return decrypt_one(server, encData, t);
}

static
int decrypt_M1(tls_record_t server, tls_buffer encData, uint64_t *t)
{
    return decrypt_one(server, encData, t);
}
#endif

static int
cmpuint64(const void *p1, const void *p2)
{
    uint64_t u1, u2;

    u1=*(uint64_t *)p1;
    u2=*(uint64_t *)p2;

    if(u1<u2) return -1;
    if(u1>u2) return 1;

    return 0;
}

static
int timing_test(uint16_t cipher, tls_protocol_version pv)
{
    int err=0;

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

    uint8_t macSize = sslCipherSuiteGetMacSize(cipher);
    uint8_t blockSize = sslCipherSuiteGetSymmetricCipherBlockIvSize(cipher);

    int trim_size;

    trim_size = ((macSize + blockSize) / blockSize) * blockSize;

    //printf("Trim_Size=%d\n", trim_size);

    tls_buffer M0, M1, C0, C1;


    M0.data=malloc(288);
    M0.length=288;

    ccrng_generate(rng, 32, M0.data);
    memset(M0.data+32, 0xFF, M0.length-32);

    M1.data=malloc(288);
    M1.length=288;
    ccrng_generate(rng, 287, M1.data);
    M1.data[287]=0;

    tls_record_t client = NULL;
    tls_record_t server = NULL;
    size_t enclen;
    uint16_t len;

    test_log_start();
    printf("Test case: cipher=%04x, pv=%04x\n", cipher, pv);


    // Creating C0:

    client = tls_record_create(false, rng);
    require_action(client, out, err=-1);
    require_noerr((err=tls_record_set_record_splitting(client, false)), out);
    require_noerr((err=tls_record_set_protocol_version(client, pv)), out);
    require_noerr((err=tls_record_init_pending_ciphers(client, cipher, false, key)), out);
    require_noerr((err=tls_record_advance_write_cipher(client)), out);

    enclen = tls_record_encrypted_size(client, tls_record_type_AppData, M0.length);
    C0.data = malloc(enclen);
    C0.length = enclen;

    require_noerr((err=tls_record_encrypt(client, M0, tls_record_type_AppData, &C0)), out);
    require_action(C0.length==enclen, out, err=-1);

    len = (C0.data[3]<<8) | C0.data[4];

    // printf("C0 len = %d\n", len);




    len -= trim_size;
    C0.data[4] = len & 0xff;
    C0.data[3] = (len >> 8)& 0xff;
    C0.length -= trim_size;

    tls_record_destroy(client);
    client = NULL;


    // Creating C1:

    client = tls_record_create(false, rng);
    require_action(client, out, err=-1);
    require_noerr((err=tls_record_set_record_splitting(client, false)), out);
    require_noerr((err=tls_record_set_protocol_version(client, pv)), out);
    require_noerr((err=tls_record_init_pending_ciphers(client, cipher, false, key)), out);
    require_noerr((err=tls_record_advance_write_cipher(client)), out);

    enclen = tls_record_encrypted_size(client, tls_record_type_AppData, M0.length);
    C1.data = malloc(enclen);
    C1.length = enclen;

    require_noerr((err=tls_record_encrypt(client, M1, tls_record_type_AppData, &C1)), out);
    require_action(C1.length==enclen, out, err=-1);

    len = (C1.data[3]<<8) | C1.data[4];
    //printf("C1 len = %d\n", len);
    len -= trim_size;
    C1.data[4] = len & 0xff;
    C1.data[3] = (len >> 8)& 0xff;
    C1.length -= trim_size;

    tls_record_destroy(client);
    client = NULL;



#define NSAMPLES 5000

    uint64_t t0[NSAMPLES]={0,};
    uint64_t t1[NSAMPLES]={0,};
/*
    uint64_t min0, min1;
    uint64_t sum0, sum1;
    uint64_t med0, med1;
 */
    int loop;


    for(loop=0; loop<2*NSAMPLES; loop++) {

        server = tls_record_create(false, rng);

        require_action(server, out, err=-1);
        require_noerr((err=tls_record_set_record_splitting(server, false)), out);
        require_noerr((err=tls_record_set_protocol_version(server, pv)), out);

        require_noerr((err=tls_record_init_pending_ciphers(server, cipher, true, key)), out);
        require_noerr((err=tls_record_advance_read_cipher(server)), out);

        err = decrypt_one(loop, server, C1, C0, t0, t1);
        if(err!=-10007) {
            test_printf("unexpected decrypt error = %d\n", err);
            err = 1;
            break;
        } else {
            err = 0;
        }

        tls_record_destroy(server);
        server = NULL;

        //printf("T0: %llu\n", t0[loop]);

    }

    // sort
    qsort(t0, NSAMPLES, sizeof(uint64_t), cmpuint64);
    qsort(t1, NSAMPLES, sizeof(uint64_t), cmpuint64);


//    printf("Min: %llu , %llu\n", t0[0], t1[0]);
//    printf("10p: %llu , %llu\n", t0[NSAMPLES/10-1], t1[NSAMPLES/10-1]);
//    printf("25p: %llu , %llu\n", t0[NSAMPLES/4-1], t1[NSAMPLES/4-1]);
    printf("Med: %llu , %llu, d=%lld\n", t0[NSAMPLES/2-1], t1[NSAMPLES/2-1], (int64_t)(t1[NSAMPLES/2-1]-t0[NSAMPLES/2-1]));
//    printf("75p: %llu , %llu\n", t0[NSAMPLES*3/4-1], t1[NSAMPLES*3/4-1]);
//    printf("90p: %llu , %llu\n", t0[NSAMPLES*9/10-1], t1[NSAMPLES*9/10-1]);
//    printf("Max: %llu , %llu\n", t0[NSAMPLES-1], t1[NSAMPLES-1]);



    free(M0.data);
    free(M1.data);
    free(C0.data);
    free(C1.data);

out:
    if(client) {
        tls_record_destroy(client);
    }
    if(server) {
        tls_record_destroy(server);
    }


    ok(!err, "Test case: cipher=%04x, pv=%04x err = %d", cipher, pv, err);
    test_log_end(err);
    return err;
}


typedef struct _CipherSuiteName {
    uint16_t cipher;
    const char *name;
} CipherSuiteName;

#define CIPHER(cipher) { cipher, #cipher},


static const CipherSuiteName tls_ciphers[] = {
    CIPHER(SSL_RSA_WITH_3DES_EDE_CBC_SHA)
    CIPHER(TLS_RSA_WITH_AES_128_CBC_SHA)
    CIPHER(TLS_RSA_WITH_AES_256_CBC_SHA)
    CIPHER(TLS_RSA_WITH_AES_128_CBC_SHA256)
    CIPHER(TLS_RSA_WITH_AES_256_CBC_SHA256)
    CIPHER(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256)
    CIPHER(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384)
};
static int n_tls_ciphers = sizeof(tls_ciphers)/sizeof(tls_ciphers[0]);

static int protos[]={
    //tls_protocol_version_SSL_3,
    tls_protocol_version_TLS_1_0,
    tls_protocol_version_TLS_1_1,
    tls_protocol_version_TLS_1_2,
};
static int n_protos = sizeof(protos)/sizeof(protos[0]);


int tls_04_timing(int argc, char * const argv[])
{
    int n_loops = 5;
    plan_tests(n_tls_ciphers*n_protos*n_loops);

    int i,j,k;

    for(i=0; i<n_tls_ciphers; i++) {
        for(j=0; j<n_protos; j++) {
            printf("-- %d - %04x - %s --\n", j, protos[j], tls_ciphers[i].name);
            for(k=0;k<n_loops;k++) {
                timing_test(tls_ciphers[i].cipher, protos[j]);
            }
        }
    }

    return 0;
}
