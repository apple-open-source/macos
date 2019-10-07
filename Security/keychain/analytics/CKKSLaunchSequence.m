//
//  CKKSLaunchSequence.m
//

#import "keychain/analytics/CKKSLaunchSequence.h"
#import <utilities/SecCoreAnalytics.h>
#import <os/assumes.h>

enum { CKKSMaxLaunchEvents = 100 };

@interface CKKSLaunchEvent : NSObject <NSCopying>
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithName:(NSString *)name;
@property (strong) NSString *name; // "human friendly" name, included in metrics
@property (strong) NSDate *date; // first, or last time this event happend (depends on usecase)
@property (assign) unsigned counter; //times this event happened, zero indexed
@end


@interface CKKSLaunchSequence () {
    bool _firstLaunch;
}
@property (readwrite) bool launched;
@property (strong) NSString* name;
@property (strong) NSMutableDictionary<NSString *,CKKSLaunchEvent *>* events; // key is uniqifier, event.name is "human friendly"
@property (strong) NSMutableDictionary<NSString *,id>* attributes;

@property (strong) NSBlockOperation *launchOperation;
@property (strong) NSMutableDictionary<NSString *, CKKSLaunchSequence *> *dependantLaunches;
@end


@implementation CKKSLaunchEvent

- (instancetype)initWithName:(NSString *)name {
    if ((self = [super init]) != NULL) {
        _name = name;
        _date = [NSDate date];
        _counter = 1;
    }
    return self;
}

- (id)copyWithZone:(NSZone *)zone
{
    CKKSLaunchEvent *copy = [[[self class] alloc] init];

    copy.name = [self.name copyWithZone:zone];
    copy.date = [self.date copyWithZone:zone];
    copy.counter = self.counter;

    return copy;
}

@end

@implementation CKKSLaunchSequence

- (instancetype)initWithRocketName:(NSString *)name {
    if ((self = [super init]) != NULL) {
        _name = name;
        _events = [NSMutableDictionary dictionary];
        _events[@"started"] = [[CKKSLaunchEvent alloc] initWithName:@"started"];
        _launchOperation = [[NSBlockOperation alloc] init];
        _dependantLaunches = [NSMutableDictionary dictionary];
    }
    return self;
}

- (bool)firstLaunch {
    @synchronized(self) {
        return _firstLaunch;
    }
}

- (void)setFirstLaunch:(bool)firstLaunch
{
    @synchronized (self) {
        if (self.launched) {
            return;
        }
        _firstLaunch = firstLaunch;
    }
}

- (void)addDependantLaunch:(NSString *)name child:(CKKSLaunchSequence *)child
{
    @synchronized (self) {
        if (self.launched) {
            return;
        }
        if (self.dependantLaunches[name]) {
            return;
        }
        self.dependantLaunches[name] = child;
        [self.launchOperation addDependency:child.launchOperation];
    }
}


- (void)addAttribute:(NSString *)key value:(id)value {
    if (![key isKindOfClass:[NSString class]]) {
        return;
    }
    @synchronized(self) {
        if (self.launched) {
            return;
        }
        if (self.attributes == nil) {
            self.attributes = [NSMutableDictionary dictionary];
        }
        self.attributes[key] = value;
    }
}


- (void)addEvent:(NSString *)eventname {
    if (![eventname isKindOfClass:[NSString class]]) {
        return;
    }
    @synchronized(self) {
        if (self.launched) {
            return;
        }
        if (self.events.count > CKKSMaxLaunchEvents) {
            return;
        }
        CKKSLaunchEvent *event = self.events[eventname];
        if (event) {
            event.counter++;
        } else {
            event = [[CKKSLaunchEvent alloc] initWithName:eventname];
        }
        self.events[eventname] = event;
    }
}

- (void)reportMetric {

    NSMutableDictionary *metric = [NSMutableDictionary dictionary];
    os_assert(self.launched);

    @synchronized(self) {
        /* don't need to lock children, at this point, they have launched and will no longer mutate */
        [self.dependantLaunches enumerateKeysAndObjectsUsingBlock:^(NSString *_Nonnull childKey, CKKSLaunchSequence *_Nonnull child, BOOL * _Nonnull stop) {
            os_assert(child.launched);
            [child.events enumerateKeysAndObjectsUsingBlock:^(NSString * _Nonnull key, CKKSLaunchEvent * _Nonnull obj, BOOL * _Nonnull stop) {
                CKKSLaunchEvent *childEvent = [obj copy];
                childEvent.name = [NSString stringWithFormat:@"c:%@-%@", childKey, childEvent.name];
                self.events[[NSString stringWithFormat:@"c:%@-%@", childKey, key]] = childEvent;
            }];
            self.attributes[[NSString stringWithFormat:@"c:%@", childKey]] = child.attributes;
        }];

        CKKSLaunchEvent *event = [[CKKSLaunchEvent alloc] initWithName:(self.firstLaunch ? @"first-launch" : @"re-launch")];
        self.events[event.name] = event;

        metric[@"events"] = [self eventsRelativeTime];
        if (self.attributes.count) {
            metric[@"attributes"] = self.attributes;
        }

    }
    [SecCoreAnalytics sendEvent:self.name event:metric];
}

- (void)launch {
    @synchronized(self) {
        if (self.launched) {
            return;
        }
        self.launched = true;
    }

    __weak typeof(self) weakSelf = self;

    [self.launchOperation addExecutionBlock:^{
        [weakSelf reportMetric];
    }];

    [[NSOperationQueue mainQueue] addOperation:self.launchOperation];
}

- (NSArray *) eventsRelativeTime
{
    NSMutableArray<CKKSLaunchEvent *>* array = [NSMutableArray array];
    [self.events enumerateKeysAndObjectsUsingBlock:^(NSString * _Nonnull __unused name, CKKSLaunchEvent * _Nonnull event, BOOL * _Nonnull __unused stop) {
        [array addObject:event];
    }];
    [array sortUsingComparator:^NSComparisonResult(CKKSLaunchEvent * _Nonnull obj1, CKKSLaunchEvent *_Nonnull obj2) {
        return [obj1.date compare:obj2.date];
    }];
    NSDate *firstEvent = array[0].date;
    NSMutableArray<NSDictionary *> *result = [NSMutableArray array];
    [array enumerateObjectsUsingBlock:^(CKKSLaunchEvent * _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
        NSMutableDictionary *event = [NSMutableDictionary dictionary];
        event[@"name"] = obj.name;
        event[@"time"] = @([obj.date timeIntervalSinceDate:firstEvent]);
        if (obj.counter) {
            event[@"counter"] = @(obj.counter);
        }
        [result addObject:event];
    }];
    return result;
}

- (NSArray *) eventsByTime {
    @synchronized (self) {
        NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
        dateFormatter.dateFormat = @"yyyy-MM-dd'T'HH:mm:ss.SSSZ";

        NSMutableArray<NSString *>* array = [NSMutableArray array];
        [self.events enumerateKeysAndObjectsUsingBlock:^(NSString * _Nonnull name, CKKSLaunchEvent * _Nonnull event, BOOL * _Nonnull stop) {
            NSString *str = [NSString stringWithFormat:@"%@ - %@:%u", [dateFormatter stringFromDate:event.date], event.name, event.counter];
            [array addObject:str];
        }];

        [array sortUsingSelector:@selector(compare:)];

        for (NSString *attribute in self.attributes) {
            [array addObject:[NSString stringWithFormat:@"attr: %@: %@", attribute, [self.attributes[attribute] description]]];
        }
        return array;
    }
}

@end
