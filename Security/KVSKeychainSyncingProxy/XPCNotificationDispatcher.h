//
//  XPCNotificationDispatcher.h
//  Security
//


#import <Foundation/Foundation.h>

@protocol XPCNotificationListener
- (void) handleNotification: (const char *) name;
@end

typedef void (^XPCNotificationBlock)(const char* notification);

@interface XPCNotificationDispatcher : NSObject

+ (instancetype) dispatcher;

- (instancetype) init;

- (void) addListener: (NSObject<XPCNotificationListener>*) newHandler;
- (void) removeListener: (NSObject<XPCNotificationListener>*) existingHandler;

@end
