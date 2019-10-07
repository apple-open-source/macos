//
//  SecProtocolHelperTest.m
//  SecProtocol
//

#import <XCTest/XCTest.h>

#import "SecProtocolInternal.h"

#define DefineTLSCiphersuiteGroupList(XXX, ...) \
    static const tls_ciphersuite_t list_##XXX[] = { \
        __VA_ARGS__ \
    };

// Mirror the internal definition of this ciphersuite group
DefineTLSCiphersuiteGroupList(tls_ciphersuite_group_default, CiphersuitesTLS13, CiphersuitesPFS);

#undef DefineTLSCiphersuiteGroupList

@interface SecProtocolHelperTest : XCTestCase
@end

@implementation SecProtocolHelperTest

- (void)testCiphersuiteGroupConversion {
    size_t ciphersuites_len = 0;
    const tls_ciphersuite_t *ciphersuites = sec_protocol_helper_ciphersuite_group_to_ciphersuite_list(tls_ciphersuite_group_default, &ciphersuites_len);
    XCTAssertTrue(ciphersuites != NULL);
    XCTAssertTrue(ciphersuites_len == (sizeof(list_tls_ciphersuite_group_default) / sizeof(tls_ciphersuite_t)));
    for (size_t i = 0; i < ciphersuites_len; i++) {
        XCTAssertTrue(ciphersuites[i] == list_tls_ciphersuite_group_default[i]);
    }
}

@end
