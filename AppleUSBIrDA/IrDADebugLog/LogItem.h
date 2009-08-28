#import <Foundation/Foundation.h>

@interface LogItem : NSObject <NSCoding> {
    NSNumber *time;				// Used as a UInt32 from IrDA
    NSNumber *first;			// Used as a UInt16 from IrDA
    NSNumber *second;			// Used as a UInt16 from IrDA
    NSString *message;
}

- (id)initWithCoder:(NSCoder *)coder;
- (void)encodeWithCoder:(NSCoder *)coder;

+ (LogItem *)logItem;
+ (LogItem *)logItemWithValues:(UInt32)myTime:(UInt16)myFirst:(UInt16)mySecond:(NSString *)myMessages;

- (NSNumber *)time;
- (void)setTime:(NSNumber *)value;
- (NSNumber *)first;
- (void)setFirst:(NSNumber *)value;
- (NSNumber *)second;
- (void)setSecond:(NSNumber *)value;
- (NSString *)message;
- (void)setMessage:(NSString *)value;

@end
