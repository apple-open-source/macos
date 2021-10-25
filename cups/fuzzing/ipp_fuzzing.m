#include <Foundation/Foundation.h>

#include "config.h"
#include "array.c"
#include "ipp-support.c"
#include "options.c"
#include "transcode.c"
#include "usersys.c"
#include "language.c"
#include "thread.c"
#include "ipp.c"
#include "globals.c"
#include "debug.c"
#include "file.c"
#include "hash.c"
#include "dir.c"

#include "stubs.m"

struct Payload {
    size_t        pos;
    size_t        len;
    ipp_uchar_t*  buf;
};

static ssize_t _ipp_read_cb(void* context, ipp_uchar_t* buffer, size_t bytes)
{
    struct Payload* p = (struct Payload*) context;

    if ((p->pos + bytes) > p->len) {
        if (gVerbose) {
            NSLog(@"%s: attempt to read %ld bytes at %ld > %ld length\n", __FUNCTION__, bytes, p->pos, p->len);
        }
        return -1;
    }

    memcpy(buffer, &p->buf[p->pos], bytes);
    p->pos += bytes;

    return (ssize_t) bytes;
}

static ssize_t _ipp_write_cb(void* context, ipp_uchar_t* buffer, size_t bytes)
{
    struct Payload* p = (struct Payload*) context;

    p->pos = p->len;
    p->buf = realloc(p->buf, p->pos + bytes);
    memcpy(&p->buf[p->pos], buffer, bytes);
    p->len += bytes;

    return (ssize_t) bytes;
}

static unsigned long long md5(const ipp_uchar_t* buffer, size_t bytes)
{
    union {
        unsigned long long result;
        ipp_uchar_t tmp[64];
    } tmp;

    bzero(&tmp, sizeof(tmp));

    cupsHashData("md5", (const void*) buffer, bytes, &tmp.tmp[0], sizeof(tmp.tmp));

    return tmp.result;
}

static void failErr(const char* file, const char* msg, int err)
{
    NSLog(@"Error: %s for %s (%d %s)\n", msg, file, err, strerror(err));
    exit(-1);
}

static void fuzz0(ipp_uchar_t* p, size_t len, int first_pass, char* outbuf, size_t outbufLen)
{
    struct Payload r = {
        0,
        len,
        p
    };

    struct Payload w = {
        0,
        0,
        NULL
    };

    ipp_t* job = ippNew();

    if (ippReadIO(&r, _ipp_read_cb, 1, NULL, job) < IPP_STATE_IDLE) {
        snprintf(outbuf, outbufLen, "ERR couldn't read into ipp");
    } else {
        ippSetState(job, IPP_STATE_IDLE);

        if (ippWriteIO(&w, _ipp_write_cb, 1, NULL, job) < IPP_STATE_IDLE) {
            snprintf(outbuf, outbufLen, "ERR couldn't write from ipp");
        } else {
            int pos = snprintf(outbuf, outbufLen, "ERR read %ld(%llx) write %ld(%llx)", r.len, md5(r.buf, r.len), w.len, md5(w.buf, w.len));

            if (first_pass) {
                char secondBuf[1024];

                fuzz0(w.buf, w.len, 0, secondBuf, sizeof(secondBuf));

                int mismatch = (strcmp(outbuf, secondBuf) != 0);

                assert((int) outbufLen > pos);
                snprintf(&outbuf[pos], outbufLen - (size_t) pos, " / vs %s%s", secondBuf, mismatch? " (ERR mismatch)" : "");
            }
        }
    }

    ippDelete(job);

    free((char*) w.buf);
}

int _ipp_fuzzing(Boolean verbose, const uint8_t* data, size_t len)
{
    Boolean save = gVerbose;
    gVerbose = verbose;

    char outbuf[1024] = { 0 };

    fuzz0((ipp_uchar_t*) data, len, 1, outbuf, sizeof(outbuf));

    if (gVerbose) {
        NSLog(@"%s", outbuf);
    }

    gVerbose = save;
    
    return strstr(outbuf, "ERR") == nil;
}

extern int LLVMFuzzerTestOneInput(const uint8_t *buffer, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *buffer, size_t size)
{
    return _ipp_fuzzing(gVerbose, buffer, size);
}
