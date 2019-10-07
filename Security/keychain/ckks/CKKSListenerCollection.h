
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * This class holds a set of weak pointers to 'listener' objects, and offers the chance to dispatch updates
 * to them on each listener's own serial dispatch queue
 */

@interface CKKSListenerCollection<__covariant ListenerType> : NSObject
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithName:(NSString*)name;

- (void)registerListener:(ListenerType)listener;
- (void)iterateListeners:(void (^)(ListenerType))block;
@end

NS_ASSUME_NONNULL_END
