//
//  coretls_performance
//

#include <mach/mach_time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#include "tls_record.h"
#include "CipherSuite.h"
#include "tls_ciphersuites.h"

/* extern struct ccrng_state *ccDRBGGetRngState(); */
#include <CommonCrypto/CommonRandomSPI.h>
#define CCRNGSTATE() ccDRBGGetRngState()

#define RUNTIME ((double)10.0)

#define perf_start() uint64_t _perf_time = mach_absolute_time();

#define perf_scale_factor() ({struct mach_timebase_info info; mach_timebase_info(&info); ((double)info.numer) / (1000000000.0 * info.denom);})

#define perf_time() ((mach_absolute_time() - _perf_time) * perf_scale_factor())


static void
set_key(tls_record_t filter, bool isServer, tls_protocol_version version, uint16_t cipher, tls_buffer key)
{
    int ret;

    ret = tls_record_set_protocol_version(filter, version);
    if (ret)
        errx(1, "tls_record_set_protocol_version");

    ret = tls_record_init_pending_ciphers(filter, cipher, isServer, key);
    if (ret)
        errx(1, "tls_record_init_pending_ciphers");

    ret = tls_record_advance_read_cipher(filter);
    if (ret)
        errx(1, "tls_record_advance_read_cipher");

    ret = tls_record_advance_write_cipher(filter);
    if (ret)
        errx(1, "tls_record_advance_write_cipher");
}

static void
performance(const char *family, const char *name, unsigned long trimCounter,
            size_t blocksize, void func(void *context), void *context)
{
    /* first trim */
    unsigned long bytes;
    unsigned long count;
    double t;

    /*
     * First figure out a good counter for about 10s worth of runtime
     */

    count = trimCounter;
    {
        perf_start();
        while (count-- > 1)
            func(context);

        t = perf_time();
    }

    count = RUNTIME * trimCounter / t;
    bytes = count * blocksize;

    {
        perf_start();
        while (count-- > 1)
            func(context);

        t = perf_time();
    }

    printf("%-15s%-20s\tblocksize %8zd\ttime: %8.4lfs\tbyte/s: %12.0lf\n",
           family, name, blocksize, t,
           ((double)bytes) / RUNTIME);

}

#define TRIM_COUNTER 5000


struct cipher_context {
    tls_record_t ofilter;
    tls_record_t ifilter;
    tls_buffer inblock;
    tls_buffer encblock;
    tls_buffer outblock;
};


static void
enc_func(void *context)
{
    struct cipher_context *ctx = (struct cipher_context *)context;
    int ret;

    ret = tls_record_encrypt(ctx->ofilter, ctx->inblock, tls_record_type_AppData, &ctx->encblock);
    if (ret)
        errx(1, "tls_record_encrypt");
}

static void
enc_dec_func(void *context)
{
    struct cipher_context *ctx = (struct cipher_context *)context;
    uint8_t type;
    int ret;

    ret = tls_record_encrypt(ctx->ofilter, ctx->inblock, tls_record_type_AppData, &ctx->encblock);
    if (ret)
        errx(1, "tls_record_encrypt");

    ret = tls_record_decrypt(ctx->ifilter, ctx->encblock, &ctx->outblock, &type);
    if (ret)
        errx(1, "tls_record_decrypt");

    if (type != tls_record_type_AppData)
        errx(1, "type");

    if (ctx->inblock.length != ctx->outblock.length)
        errx(1, "length");

    if (memcmp(ctx->inblock.data, ctx->outblock.data, ctx->outblock.length) != 0)
        errx(1, "memcmp");
}

static void
record_layer(const char *name,
             bool dtls,
             bool isServer,
             bool read,
             tls_protocol_version version,
             uint16_t cipher,
             tls_buffer key,
             size_t blocksize,
             struct ccrng_state *rng)
{
    struct cipher_context context;
    size_t outsize;

    context.ofilter = tls_record_create(dtls, rng);
    if (context.ofilter == NULL)
        errx(1, "tls_record_create");

    context.ifilter = tls_record_create(dtls, rng);
    if (context.ifilter == NULL)
        errx(1, "tls_record_create");


    set_key(context.ofilter, isServer, version, cipher, key);
    set_key(context.ifilter, !isServer, version, cipher, key);

    context.inblock.length = blocksize;
    context.inblock.data = malloc(blocksize);

    if (ccrng_generate(rng, context.inblock.length, context.inblock.data))
        errx(1, "ccrng_generate");

    context.outblock.length = blocksize;
    context.outblock.data = malloc(blocksize);

    outsize = tls_record_encrypted_size(context.ofilter, tls_record_type_AppData, blocksize);

    context.encblock.data = malloc(outsize);
    context.encblock.length = outsize;

    outsize = tls_record_decrypted_size(context.ifilter, context.encblock.length);

    context.outblock.data = malloc(outsize);
    context.outblock.length = outsize;

    performance("enc+dec", name, TRIM_COUNTER, blocksize, enc_dec_func, &context);
    performance("enc", name, TRIM_COUNTER, blocksize, enc_func, &context);

    free(context.inblock.data);
    free(context.encblock.data);
    free(context.outblock.data);

    tls_record_destroy(context.ofilter);
    tls_record_destroy(context.ifilter);
}


static struct {
    const char *name;
    uint16_t suite;
} cipherSuites[] = {
    {
        .name = "AES_128_GCM_SHA256",
        .suite = TLS_RSA_WITH_AES_128_GCM_SHA256,
    },
    {
        .name = "AES_256_GCM_SHA384",
        .suite = TLS_RSA_WITH_AES_256_GCM_SHA384,
    },
    {
        .name = "AES_128_CBC_SHA",
        .suite = TLS_RSA_WITH_AES_128_CBC_SHA,
    },
    {
        .name = "AES_128_CBC_SHA256",
        .suite = TLS_RSA_WITH_AES_128_CBC_SHA256,
    },
    {
        .name = "3DES_EDE_CBC_SHA",
        .suite = TLS_RSA_WITH_3DES_EDE_CBC_SHA,
    },
    {
        .name = "RC4_128_SHA",
        .suite = TLS_RSA_WITH_RC4_128_SHA,
    },
};

struct blockSizes {
    size_t blocksize;
} blockSizes[] = {
    {
        .blocksize = 100,
    },
#if 0
    {
        .blocksize = 250,
    },
    {
        .blocksize = 500,
    },
#endif
    {
        .blocksize = 1000,
    },
    {
        .blocksize = 10000,
    },
};


int
main(int argc, const char **argv)
{
    tls_buffer key;
    unsigned long n, m;

    printf("perf stat\n");

    for (n = 0; n < sizeof(cipherSuites) / sizeof(cipherSuites[0]); n++) {

        key.length = sslCipherSuiteGetKeydataSize(cipherSuites[n].suite);
        key.data = malloc(key.length);

        if (ccrng_generate(CCRNGSTATE(), sizeof(key.length), key.data))
            errx(1, "ccrng_generate");

        for (m = 0; m < sizeof(blockSizes)/sizeof(blockSizes[0]); m++) {
            record_layer(cipherSuites[n].name, false, false, true,
                         tls_protocol_version_TLS_1_0, cipherSuites[n].suite,
                         key, blockSizes[m].blocksize,
                         CCRNGSTATE());
        }

        free(key.data);
    }

    printf("done\n");

    return 0;
}
