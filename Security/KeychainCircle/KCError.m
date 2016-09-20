//
//  KCError.m
//  Security
//
//

#import "KCError.h"

static NSString* KCErrorDomain = @"com.apple.security.keychaincircle";


@implementation NSError(KCJoiningError)

+ (nonnull instancetype) errorWithJoiningError:(KCJoiningError) code
                                        format:(NSString*) format
                                     arguments:(va_list) va {
    return [[NSError alloc] initWithJoiningError:code
                                        userInfo:@{NSLocalizedDescriptionKey:[[NSString alloc] initWithFormat:format arguments:va]}];

}

+ (nonnull instancetype) errorWithJoiningError:(KCJoiningError) code
                                        format:(NSString*) format, ... {

    va_list va;
    va_start(va, format);
    NSError* result = [NSError errorWithJoiningError:code format:format arguments:va];
    va_end(va);

    return result;

}
- (nonnull instancetype) initWithJoiningError:(KCJoiningError) code
                                     userInfo:(nonnull NSDictionary *)dict {
    return [self initWithDomain:KCErrorDomain code:code userInfo:dict];
}
@end

void KCJoiningErrorCreate(KCJoiningError code, NSError** error, NSString* format, ...) {
    if (error && (*error == nil)) {
        va_list va;
        va_start(va, format);
        *error = [NSError errorWithJoiningError:code format:format arguments:va];
        va_end(va);
    }
}

