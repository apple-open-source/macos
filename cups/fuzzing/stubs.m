#include <Foundation/Foundation.h>

#include "config.h"

#include <pthread.h>

struct StubsPerThread {
    Boolean verbose;
};

static void _stub_specific_dtor(void* arg)
{
    free(arg);
}

static struct StubsPerThread* perThread() {
    static pthread_key_t sKey;
    static dispatch_once_t sOnce = 0;
    dispatch_once(&sOnce, ^{
        pthread_key_create(&sKey, _stub_specific_dtor);
    });

    struct StubsPerThread* t = pthread_getspecific(sKey);
    if (t == NULL) {
        t = (struct StubsPerThread*)calloc(1, sizeof(struct StubsPerThread));
        pthread_setspecific(sKey, t);
    }

    return t;
}

#define gVerbose	perThread()->verbose

char* _cupsStrAlloc(const char* s)
{
    return strdup(s);
}

void _cupsStrFree(const char* s)
{
    free((void*) s);
}

int _cups_strcasecmp(const char* a, const char* b)
{
    return strcasecmp(a, b);
}

void _cups_strcpy(char* dst, const char* src)
{
    strcpy(dst, src);
}

int _cups_strncasecmp(const char* a, const char* b, size_t len)
{
    return strncasecmp(a, b, len);
}

http_t        *_cupsConnect(void)
{
    abort();
    return nil;
}

http_tls_credentials_t _httpCreateCredentials(cups_array_t *credentials)
{
    abort();
    return nil;
}

void _httpFreeCredentials(http_tls_credentials_t credentials)
{
    abort();
}

void _httpTLSSetOptions(int options, int min_version, int max_version)
{
}

void        _httpAddrSetPort(http_addr_t *addr, int port)
{
}

int        httpAddrLength(const http_addr_t *addr)
{
    abort();
    return -1;
}


int httpAddrClose(http_addr_t *addr, int fd)
{
    abort();
    return -1;
}

http_addrlist_t *httpAddrConnect(http_addrlist_t *addrlist, int *sock)
{
    abort();
    return nil;
}

void httpAddrFreeList(http_addrlist_t *addrlist)
{
}

http_addrlist_t *httpAddrGetList(const char *hostname, int family, const char *service)
{
    abort();
    return nil;
}

void httpClose(http_t *http)
{
    abort();
}

int httpEncryption(http_t *http, http_encryption_t e)
{
    abort();
}

ssize_t httpRead2(http_t *http, char *buffer, size_t length)
{
    abort();
    return -1;
}

http_uri_status_t httpSeparateURI(http_uri_coding_t decoding, const char *uri, char *scheme, int schemelen, char *username, int usernamelen, char *host, int hostlen, int *port, char *resource, int resourcelen)
{
    abort();
    return HTTP_URI_STATUS_BAD_URI;
}

const char *httpURIStatusString(http_uri_status_t status)
{
    abort();
    return nil;
}

int httpWait(http_t *http, int msec)
{
    abort();
    return -1;
}

ssize_t httpWrite2(http_t *http, const char *buffer, size_t length)
{
    abort();
    return -1;
}

void _cupsSetError(ipp_status_t status, const char *message, int localize)
{
    if (gVerbose) {
        NSLog(@"%s: status %d, message %s", __FUNCTION__, status, message);
    }

    _cups_globals_t* cg = _cupsGlobals();

    cg->last_error = status;
    if (cg->last_status_message) {
        _cupsStrFree(cg->last_status_message);
        cg->last_status_message = nil;
    }
    if (message) {
        cg->last_status_message = _cupsStrAlloc(message);
    }
}

void _cupsSetHTTPError(http_status_t status)
{
    if (gVerbose) {
        NSLog(@"%s: http status %d", __FUNCTION__, status);
    }
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, "from http status", 0);
}
