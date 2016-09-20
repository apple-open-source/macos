//
//  NSError+KCCreationHelpers.h
//  KeychainCircle
//
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// Returns false and fills in error with formatted description if cc_result is an error
bool CoreCryptoError(int cc_result, NSError * _Nullable * _Nullable error,  NSString * _Nonnull description, ...) NS_FORMAT_FUNCTION(3, 4);
// Returns false and fills in a requirement error if requirement is false
// We should have something better than -50 here.
bool RequirementError(bool requirement, NSError * _Nullable * _Nullable error, NSString * _Nonnull description, ...) NS_FORMAT_FUNCTION(3, 4);

bool OSStatusError(OSStatus status, NSError * _Nullable * _Nullable error, NSString* _Nonnull description, ...) NS_FORMAT_FUNCTION(3, 4);


// MARK: Error Extensions
@interface NSError(KCCreationHelpers)

+ (instancetype) errorWithOSStatus:(OSStatus) status
                          userInfo:(NSDictionary *)dict;

- (instancetype) initWithOSStatus:(OSStatus) status
                         userInfo:(NSDictionary *)dict;

+ (instancetype) errorWithOSStatus:(OSStatus) status
                       description:(NSString*)description
                              args:(va_list)va;

- (instancetype) initWithOSStatus:(OSStatus) status
                      description:(NSString*)description
                             args:(va_list)va;

+ (instancetype) errorWithCoreCryptoStatus:(int) status
                                  userInfo:(NSDictionary *)dict;

- (instancetype) initWithCoreCryptoStatus:(int) status
                                 userInfo:(NSDictionary *)dict;

+ (instancetype) errorWithCoreCryptoStatus:(int) status
                               description:(NSString*)description
                                      args:(va_list)va;

- (instancetype) initWithCoreCryptoStatus:(int) status
                              description:(NSString*)description
                                     args:(va_list)va;

@end

NS_ASSUME_NONNULL_END
