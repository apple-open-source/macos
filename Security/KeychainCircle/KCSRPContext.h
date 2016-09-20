//
//  SRPSession.h
//  KeychainCircle
//
//

#import <Foundation/Foundation.h>

#include <corecrypto/ccdigest.h>
#include <corecrypto/ccrng.h>
#include <corecrypto/ccsrp.h>

NS_ASSUME_NONNULL_BEGIN

@interface KCSRPContext : NSObject

- (instancetype) init NS_UNAVAILABLE;

- (instancetype) initWithUser: (NSString*) user
                   digestInfo: (const struct ccdigest_info *) di
                        group: (ccsrp_const_gp_t) gp
                 randomSource: (struct ccrng_state *) rng NS_DESIGNATED_INITIALIZER;

- (bool) isAuthenticated;

// Returns an NSData that refers to the key in the context.
// It becomes invalid when this context is released.
- (NSData*) getKey;

@end

@interface KCSRPClientContext : KCSRPContext

- (nullable NSData*) copyStart: (NSError**) error;
- (nullable NSData*) copyResposeToChallenge: (NSData*) B_data
                          password: (NSString*) password
                              salt: (NSData*) salt
                             error: (NSError**) error;
- (bool) verifyConfirmation: (NSData*) HAMK_data
                      error: (NSError**) error;

@end

@interface KCSRPServerContext : KCSRPContext
@property (readonly) NSData* salt;

- (instancetype) initWithUser: (NSString*) user
                         salt: (NSData*) salt
                     verifier: (NSData*) verifier
                   digestInfo: (const struct ccdigest_info *) di
                        group: (ccsrp_const_gp_t) gp
                 randomSource: (struct ccrng_state *) rng NS_DESIGNATED_INITIALIZER;

- (instancetype) initWithUser: (NSString*)user
                     password: (NSString*)password
                   digestInfo: (const struct ccdigest_info *) di
                        group: (ccsrp_const_gp_t) gp
                 randomSource: (struct ccrng_state *) rng NS_DESIGNATED_INITIALIZER;

- (instancetype) initWithUser: (NSString*) user
                   digestInfo: (const struct ccdigest_info *) di
                        group: (ccsrp_const_gp_t) gp
                 randomSource: (struct ccrng_state *) rng NS_UNAVAILABLE;


- (bool) resetWithPassword: (NSString*) password
                     error: (NSError**) error;

- (nullable NSData*) copyChallengeFor: (NSData*) A_data
                       error: (NSError**) error;
- (nullable NSData*) copyConfirmationFor: (NSData*) M_data
                          error: (NSError**) error;

@end

NS_ASSUME_NONNULL_END
