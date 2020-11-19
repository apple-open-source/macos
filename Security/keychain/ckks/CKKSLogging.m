
#import <Foundation/Foundation.h>
#import "keychain/ckks/CKKS.h"

os_log_t CKKSLogObject(NSString* scope, NSString* _Nullable zoneName)
{
    __block os_log_t ret = OS_LOG_DISABLED;

    static dispatch_queue_t logQueue = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        logQueue = dispatch_queue_create("ckks-logger", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
    });

    static NSMutableDictionary* scopeMap = nil;

    dispatch_sync(logQueue, ^{
        if(scopeMap == nil) {
            scopeMap = [NSMutableDictionary dictionary];
        }

        NSString* key = zoneName ? [scope stringByAppendingFormat:@"-%@", zoneName] : scope;

        ret = scopeMap[key];

        if(!ret) {
            ret = os_log_create("com.apple.security.ckks", [key cStringUsingEncoding:NSUTF8StringEncoding]);
            scopeMap[key] = ret;
        }
    });

    return ret;
}
