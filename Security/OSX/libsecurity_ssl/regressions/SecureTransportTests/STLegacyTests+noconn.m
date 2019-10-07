//
//  ssl-52-noconn.c
//  libsecurity_ssl
//

#include <stdio.h>
#include <Security/SecureTransport.h>
#import "STLegacyTests.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

@implementation STLegacyTests (dhe)

static
OSStatus r(SSLConnectionRef connection, void *data, size_t *dataLength) {
    return errSSLWouldBlock;
}

static
OSStatus w(SSLConnectionRef connection, const void *data, size_t *dataLength) {
    return errSSLWouldBlock;
}

//Testing <rdar://problem/13539215> Trivial SecureTransport example crashes on Cab, where it worked on Zin
-(void) testNoConn
{
    OSStatus ortn;
    SSLContextRef ctx;
    ctx = SSLCreateContext(NULL, kSSLClientSide, kSSLStreamType);
    SSLSetIOFuncs(ctx, r, w);
    ortn = SSLHandshake(ctx);

    XCTAssertEqual(ortn, errSSLWouldBlock, "SSLHandshake unexpected return\n");

    CFRelease(ctx);
}

@end

#pragma clang diagnostic pop
