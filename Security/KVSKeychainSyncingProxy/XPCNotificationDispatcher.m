//
//  XPCNotificationDispatcher.m
//  Security
//
//  Created by Mitch Adler on 11/1/16.
//
//

#import "XPCNotificationDispatcher.h"
#include <dispatch/dispatch.h>

#include <utilities/debugging.h>

#include <xpc/xpc.h>

//
// PointerArray helpers

@interface NSPointerArray (Removal)
- (void) removePointer: (nullable void *)pointer;
@end

@implementation NSPointerArray (Removal)
- (void) removePointer: (nullable void *)pointer {
    NSUInteger pos = 0;
    while(pos < [self count]) {
        if (pointer == [self pointerAtIndex:pos]) {
            [self removePointerAtIndex:pos];
        } else {
            pos += 1;
        }
    }
}
@end

//
//

static const char *kXPCNotificationStreamName = "com.apple.notifyd.matching";
static const char *kXPCNotificationNameKey = "Notification";

@interface XPCNotificationDispatcher ()
@property dispatch_queue_t queue;
@property NSPointerArray* listeners;

- (void) notification: (const char *) value;

@end


@implementation XPCNotificationDispatcher

+ (instancetype) dispatcher {
    static dispatch_once_t onceToken;
    static XPCNotificationDispatcher* sDispactcher;
    dispatch_once(&onceToken, ^{
        sDispactcher = [[XPCNotificationDispatcher alloc] init];
    });

    return sDispactcher;
}

- (instancetype) init {
    self = [super init];

    if (self) {
        self.queue = dispatch_queue_create("XPC Notification Dispatch", DISPATCH_QUEUE_SERIAL);
        self.listeners = [NSPointerArray weakObjectsPointerArray];

        xpc_set_event_stream_handler(kXPCNotificationStreamName, self.queue, ^(xpc_object_t event){
            const char *notificationName = xpc_dictionary_get_string(event, kXPCNotificationNameKey);
            if (notificationName) {
                [self notification:notificationName];
            }
        });
    }

    return self;
}

- (void) notification:(const char *)name {
    [self.listeners compact];
    [[self.listeners allObjects] enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
        [obj handleNotification: name];
    }];

}

- (void) addListener: (NSObject<XPCNotificationListener>*) newHandler {
    dispatch_sync(self.queue, ^{
        [self.listeners compact];
        [self.listeners addPointer:(__bridge void * _Nullable)(newHandler)];
    });
}

- (void) removeListener: (NSObject<XPCNotificationListener>*) existingHandler {
    dispatch_sync(self.queue, ^{
        [self.listeners removePointer:(__bridge void * _Nullable)existingHandler];
    });
}

@end
