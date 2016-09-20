//
//  SRPSession.m
//  Security
//
//


#import <Foundation/Foundation.h>
#import "KCSRPContext.h"

#include <os/base.h>

#include <corecrypto/ccsrp.h>
#include <corecrypto/ccsha2.h>
#include <corecrypto/ccdh_gp.h>
#include <corecrypto/ccder.h>

#import "NSError+KCCreationHelpers.h"

static const NSStringEncoding srpStringEncoding = NSUTF8StringEncoding;

@interface KCSRPContext ()
@property (readwrite) ccsrp_ctx* context;
@property (readwrite) struct ccrng_state *rng;
@property (readwrite) NSString* user;
@end

@implementation KCSRPContext

+ (KCSRPContext*) createWithUser: (NSString*) user
                    digestInfo: (const struct ccdigest_info *) di
                         group: (ccsrp_const_gp_t) gp
                  randomSource: (struct ccrng_state *) rng {
    return [[self alloc] initWithUser:user
                           digestInfo:di
                                group:gp
                         randomSource:rng];
}

- (NSData*) dataForPassword: (NSString*) password {
    return [password dataUsingEncoding:srpStringEncoding];
}

-  (nullable const char *) userNameString {
    return [self.user cStringUsingEncoding:srpStringEncoding];
}

- (instancetype) initWithUser: (NSString*) user
                   digestInfo: (const struct ccdigest_info *) di
                        group: (ccsrp_const_gp_t) gp
                 randomSource: (struct ccrng_state *) rng
{
    self = [super init];
    
    self.context = malloc(ccsrp_sizeof_srp(di, gp));
    ccsrp_ctx_init(self.context, di, gp);

    self.user = user;
    self.rng = rng;

    return self;
}

- (void) finalize {
    ccsrp_ctx_clear(ccsrp_ctx_di(self.context),
                    ccsrp_ctx_gp(self.context),
                    self.context);

    free(self.context);
}

- (NSData*) getKey {
    size_t key_length = 0;
    const void * key = ccsrp_get_session_key(self.context, &key_length);

    return key ? [NSData dataWithBytesNoCopy:(void *)key length:key_length freeWhenDone:false] : nil;
}

- (bool) isAuthenticated {
    return ccsrp_is_authenticated(self.context);
}

@end


@implementation KCSRPClientContext

- (NSData*) copyStart: (NSError**) error {
    NSMutableData* A_data = [NSMutableData dataWithLength: ccsrp_exchange_size(self.context)];

    int result = ccsrp_client_start_authentication(self.context, self.rng, A_data.mutableBytes);
    if (!CoreCryptoError(result, error, @"Start packet copy failed: %d", result)) {
        A_data = NULL;
    }

    return A_data;
}

static bool ExactDataSizeRequirement(NSData* data, NSUInteger expectedLength, NSError**error, NSString* name) {
    return RequirementError(data.length == expectedLength, error, @"%@ incorrect size, Expected %ld, got %ld", name, (unsigned long)expectedLength, (unsigned long)data.length);
}

- (nullable NSData*) copyResposeToChallenge: (NSData*) B_data
                                   password: (NSString*) password
                                       salt: (NSData*) salt
                                      error: (NSError**) error {

    if (!ExactDataSizeRequirement(B_data, ccsrp_exchange_size(self.context), error, @"challenge data"))
        return nil;
    
    NSMutableData* M_data = [NSMutableData dataWithLength: ccsrp_session_size(self.context)];
    NSData* passwordData = [self dataForPassword: password];

    int result = ccsrp_client_process_challenge(self.context,
                                                [self userNameString],
                                                passwordData.length,
                                                passwordData.bytes,
                                                salt.length,
                                                salt.bytes,
                                                B_data.bytes,
                                                M_data.mutableBytes);

    if (!CoreCryptoError(result, error, @"Challenge processing failed: %d", result)) {
        M_data = NULL;
    }

    return M_data;
}

- (bool) verifyConfirmation: (NSData*) HAMK_data
                      error: (NSError**) error {
    if (!ExactDataSizeRequirement(HAMK_data, ccsrp_session_size(self.context), error, @"confirmation data"))
        return nil;

    return ccsrp_client_verify_session(self.context, HAMK_data.bytes);
}

@end

@interface KCSRPServerContext ()
@property (readwrite) NSData* verifier;
@end

@implementation KCSRPServerContext

- (bool) resetWithPassword: (NSString*) password
                     error: (NSError**) error {
    const int salt_length = 16;

    NSMutableData* salt = [NSMutableData dataWithLength: salt_length];
    NSMutableData* verifier = [NSMutableData dataWithLength: ccsrp_ctx_sizeof_n(self.context)];

    NSData* passwordData = [self dataForPassword: password];

    int generateResult = ccsrp_generate_salt_and_verification(self.context,
                                                              self.rng,
                                                              [self userNameString],
                                                              passwordData.length,
                                                              passwordData.bytes,
                                                              salt.length,
                                                              salt.mutableBytes,
                                                              verifier.mutableBytes);

    if (!CoreCryptoError(generateResult, error, @"Error generating SRP salt/verifier")) {
        return false;
    }

    self.verifier = verifier;
    self->_salt = salt;

    return true;
}

- (instancetype) initWithUser: (NSString*)user
                     password: (NSString*)password
                   digestInfo: (const struct ccdigest_info *) di
                        group: (ccsrp_const_gp_t) gp
                 randomSource: (struct ccrng_state *) rng {
    self = [super initWithUser: user
                    digestInfo: di
                         group: gp
                  randomSource: rng];

    if (![self resetWithPassword:password error:nil]) {
        return nil;
    }

    return self;
}

- (instancetype) initWithUser: (NSString*) user
                         salt: (NSData*) salt
                     verifier: (NSData*) verifier
                   digestInfo: (const struct ccdigest_info *) di
                        group: (ccsrp_const_gp_t) gp
                 randomSource: (struct ccrng_state *) rng {
    self = [super initWithUser: user
                    digestInfo: di
                         group: gp
                  randomSource: rng];

    self.verifier = verifier;
    self->_salt = salt;

    return self;
}


- (NSData*) copyChallengeFor: (NSData*) A_data
                       error: (NSError**) error {
    if (!ExactDataSizeRequirement(A_data, ccsrp_exchange_size(self.context), error, @"start data"))
        return nil;

    NSMutableData* B_data = [NSMutableData dataWithLength: ccsrp_exchange_size(self.context)];

    int result = ccsrp_server_start_authentication(self.context, self.rng,
                                                   [self userNameString],
                                                   self.salt.length, self.salt.bytes,
                                                   self.verifier.bytes, A_data.bytes,
                                                   B_data.mutableBytes);

    if (!CoreCryptoError(result, error, @"Server start authentication failed: %d", result)) {
        B_data = NULL;
    }

    return B_data;
}

- (NSData*) copyConfirmationFor: (NSData*) M_data
                          error: (NSError**) error {
    if (!ExactDataSizeRequirement(M_data, ccsrp_session_size(self.context), error, @"response data"))
        return nil;

    NSMutableData* HAMK_data = [NSMutableData dataWithLength: ccsrp_session_size(self.context)];

    bool verify = ccsrp_server_verify_session(self.context, M_data.bytes, HAMK_data.mutableBytes);

    if (!CoreCryptoError(!verify, error, @"SRP verification failed")) {
        HAMK_data = NULL;
    }

    return HAMK_data;
}

@end
