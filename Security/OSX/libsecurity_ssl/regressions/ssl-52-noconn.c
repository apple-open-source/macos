//
//  ssl-52-noconn.c
//  libsecurity_ssl
//

#include <stdio.h>
#include <Security/SecureTransport.h>
#include "ssl_regressions.h"

static
OSStatus r(SSLConnectionRef connection, void *data, size_t *dataLength) {
    return errSSLWouldBlock;
}

static
OSStatus w(SSLConnectionRef connection, const void *data, size_t *dataLength) {
    return errSSLWouldBlock;
}

//Testing <rdar://problem/13539215> Trivial SecureTransport example crashes on Cab, where it worked on Zin
static
void tests()
{
    OSStatus ortn;
    SSLContextRef ctx;
    ctx = SSLCreateContext(NULL, kSSLClientSide, kSSLStreamType);
    SSLSetIOFuncs(ctx, r, w);
    ortn = SSLHandshake(ctx);

    is(ortn, errSSLWouldBlock, "SSLHandshake unexpected return\n");

    CFRelease(ctx);
}


int ssl_52_noconn(int argc, char *const *argv)
{

    plan_tests(1);

    tests();

    return 0;
}
