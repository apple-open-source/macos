
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface TPCTLObjectiveC : NSObject

+ (BOOL)catchNSException:(void(^)(void))block error:(NSError**)error;
+ (NSString* _Nullable)jsonSerialize:(id)something error:(NSError**)error;

@end

NS_ASSUME_NONNULL_END
