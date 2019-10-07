
#import "tpctl-objc.h"

@implementation TPCTLObjectiveC

+ (BOOL)catchNSException:(void(^)(void))block error:(NSError**)error {
    @try {
        block();
        return true;
    }
    @catch(NSException* exception) {
        if(error) {
            NSMutableDictionary* ui = exception.userInfo ? [exception.userInfo mutableCopy] : [NSMutableDictionary dictionary];
            if(exception.reason) {
                ui[NSLocalizedDescriptionKey] = exception.reason;
            }
            *error = [NSError errorWithDomain:exception.name code:0 userInfo:ui];
        }
        return false;
    }
}

+ (NSString* _Nullable)jsonSerialize:(id)something error:(NSError**)error {
    @try {
        NSError* localError = nil;
        NSData* jsonData = [NSJSONSerialization dataWithJSONObject:something options:(NSJSONWritingPrettyPrinted | NSJSONWritingSortedKeys) error:&localError];
        if(!jsonData || localError) {
            if(error) {
                *error = localError;
            }
            return nil;
        }

        NSString* utf8String = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
        if(!utf8String) {
            if(error) {
                *error = [NSError errorWithDomain:@"text" code:0 userInfo:@{NSLocalizedDescriptionKey: @"JSON data could not be decoded as UTF8"}];
            }
            return nil;
        }
        return utf8String;
    }
    @catch(NSException* exception) {
        if(error) {
            NSMutableDictionary* ui = exception.userInfo ? [exception.userInfo mutableCopy] : [NSMutableDictionary dictionary];
            if(exception.reason) {
                ui[NSLocalizedDescriptionKey] = exception.reason;
            }
            *error = [NSError errorWithDomain:exception.name code:0 userInfo:ui];
        }
        return nil;
    }
}

@end
