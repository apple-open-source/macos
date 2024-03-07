#if OCTAGON

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol OTTapToRadarAdapter

- (void)postHomePodLostTrustTTR:(NSString*)identifiers;
@end

@interface OTTapToRadarActualAdapter : NSObject <OTTapToRadarAdapter>
@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON

