//
//  NSError+KCCreationHelpers.m
//  KechainCircle
//
//

#import <Foundation/Foundation.h>

#import <NSError+KCCreationHelpers.h>

static NSString* coreCryptoDomain = @"kSecCoreCryptoDomain";
static NSString* srpDomain = @"com.apple.security.srp";

static NSDictionary* UserInfoFromVA(NSString*description, va_list va) {
    return @{NSLocalizedDescriptionKey:[[NSString alloc] initWithFormat:description
                                                              arguments:va]};
}

// We should get this from SecCFError.h and Security.framework..
bool CoreCryptoError(int cc_result, NSError** error, NSString* description, ...)
{
    bool failed = cc_result != 0;

    if (failed && error && !*error) {
        va_list va;
        va_start(va, description);
        *error = [NSError errorWithCoreCryptoStatus:cc_result
                                        description:description
                                               args:va];
        va_end(va);
    }

    return !failed;
}

bool OSStatusError(OSStatus status, NSError** error, NSString*description, ...) {
    bool failed = status != 0;

    if (failed && error && !*error) {
        va_list va;
        va_start(va, description);
        *error = [NSError errorWithOSStatus:status
                                description:description
                                       args:va];
        va_end(va);
    }

    return !failed;
}

bool RequirementError(bool requirement, NSError** error, NSString*description, ...) {
    bool failed = !requirement;

    if (failed && error && !*error) {
        va_list va;
        va_start(va, description);
        *error = [NSError errorWithOSStatus:-50
                                description:description
                                       args:va];
        va_end(va);
    }

    return !failed;
}


@implementation NSError(KCCreationHelpers)

+ (instancetype) errorWithOSStatus:(OSStatus) status
                          userInfo:(NSDictionary *)dict {
    return [[NSError alloc] initWithOSStatus:status userInfo:dict];
}

- (instancetype) initWithOSStatus:(OSStatus) status
                         userInfo:(NSDictionary *)dict {
    return [self initWithDomain:NSOSStatusErrorDomain code:status userInfo:dict];
}

+ (instancetype) errorWithOSStatus:(OSStatus) status
                       description:(NSString*)description
                              args:(va_list)va {
    return [[NSError alloc] initWithOSStatus:status description:description args:va];
}

- (instancetype) initWithOSStatus:(OSStatus) status
                      description:(NSString*)description
                             args:(va_list)va {
    return [self initWithOSStatus:status
                         userInfo:UserInfoFromVA(description, va)];
}

+ (instancetype) errorWithCoreCryptoStatus:(int) status
                                 userInfo:(NSDictionary *)dict {
    return [[NSError alloc] initWithCoreCryptoStatus:status userInfo:dict];
}


- (instancetype) initWithCoreCryptoStatus:(int) status
                                 userInfo:(NSDictionary *)dict {
    return [self initWithDomain:coreCryptoDomain code:status userInfo:dict];
}

+ (instancetype) errorWithCoreCryptoStatus:(int) status
                               description:(NSString*)description
                                      args:(va_list)va {
    return [[NSError alloc] initWithCoreCryptoStatus:status description:description args:va];
}

- (instancetype) initWithCoreCryptoStatus:(int) status
                              description:(NSString*)description
                                     args:(va_list)va {
    return [self initWithCoreCryptoStatus:status
                                 userInfo:UserInfoFromVA(description, va)];
}

@end
