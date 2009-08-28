#import "LogItem.h"

static NSString *defaultCategory = @"Some Incredibly Insightfull message";

@implementation LogItem

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject: [self time]];
    [coder encodeObject: [self first]];
    [coder encodeObject: [self second]];
	[coder encodeObject: [self message]];
}

- (id)initWithCoder:(NSCoder *)coder {
    // init variables in the same order as they were encoded    
    [self setTime: [coder decodeObject]];
    [self setFirst: [coder decodeObject]];
    [self setSecond: [coder decodeObject]];
    [self setMessage: [coder decodeObject]];
    return self;
}


+ (LogItem *)logItemWithValues:(UInt32)myTime:(UInt16)myFirst:(UInt16)mySecond:(NSString *)myMessage{
    // convenience method to create a new logItem with default
    // test data given an amount
    LogItem *newLogItem = [[LogItem alloc] init];
    [newLogItem autorelease];
    [newLogItem setTime: [NSNumber numberWithUnsignedLong:myTime]]; 
    [newLogItem setFirst: [NSNumber numberWithUnsignedShort:myFirst]]; 
    [newLogItem setSecond: [NSNumber numberWithUnsignedShort:mySecond]]; 
    [newLogItem setMessage: myMessage];
    return newLogItem;
}

+ (LogItem *)logItem {
    return [[[LogItem alloc] init] autorelease];
}

- (id)init {
    [super init];
    // ensure amount is never nil
    [self setTime: [NSNumber numberWithUnsignedLong:0]];
    [self setFirst: [NSNumber numberWithUnsignedShort:0]]; 
    [self setSecond: [NSNumber numberWithUnsignedShort:0]]; 
    [self setMessage: defaultCategory];
    return self;
}

- (NSNumber *)time {
	return [[time copy] autorelease];
}

- (void)setTime:(NSNumber *)value {
    [value retain];
    [time release];
    time = value;
}

- (NSString *)message {
	return [[message copy] autorelease];
}

- (void)setMessage:(NSString *)value {
    [value retain];
    [message release];
    message = value;
}

- (NSNumber *)first {
    return [[first copy] autorelease];
}
- (NSNumber *)second {
    return [[second copy] autorelease];
}

- (void)setFirst:(NSNumber *)value {
    NSNumber *copy = [value copy];
    [first release];
    first = copy;
}

- (void)setSecond:(NSNumber *)value {
    NSNumber *copy = [value copy];
    [second release];
    second = copy;
}

- (void)dealloc {
    [self setTime:nil];
    [self setFirst:nil];
    [self setSecond:nil];
    [self setMessage:nil];
    [super dealloc];
}

@end
