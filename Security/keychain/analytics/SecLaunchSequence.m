//
//  SecLaunchSequence.m
//

#import <Security/SecLaunchSequence.h>
#import <utilities/SecCoreAnalytics.h>
#import <os/assumes.h>

enum { SecMaxLaunchEvents = 100 };

@interface SecLaunchEvent : NSObject <NSCopying>
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithName:(NSString *)name;
@property (strong) NSString *name; // "human friendly" name, included in metrics
@property (strong) NSDate *date; // first, or last time this event happend (depends on usecase)
@property (assign) unsigned counter; //times this event happened, zero indexed
@end


@interface SecLaunchSequence () {
    bool _firstLaunch;
}
@property (readwrite) bool launched;
@property (strong, readwrite) NSString* name;
@property (strong) NSMutableDictionary<NSString *,SecLaunchEvent *>* events; // key is uniqifier, event.name is "human friendly"
@property (strong) NSMutableDictionary<NSString *,id>* attributes;

@property (strong) NSBlockOperation *launchOperation;
@property (strong) NSMutableDictionary<NSString *, SecLaunchSequence *> *dependantLaunches;
@end


@implementation SecLaunchEvent

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
    SecLaunchEvent *copy = [[[self class] alloc] init];

    copy.name = [self.name copyWithZone:zone];
    copy.date = [self.date copyWithZone:zone];
    copy.counter = self.counter;

    return copy;
}

@end

@implementation SecLaunchSequence

- (instancetype)initWithRocketName:(NSString *)name {
    if ((self = [super init]) != NULL) {
        _name = name;
        _events = [NSMutableDictionary dictionary];
        _events[@"started"] = [[SecLaunchEvent alloc] initWithName:@"started"];
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

- (void)addDependantLaunch:(NSString *)name child:(SecLaunchSequence *)child
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
        if (self.events.count > SecMaxLaunchEvents) {
            return;
        }
        SecLaunchEvent *event = self.events[eventname];
        if (event) {
            event.counter++;
        } else {
            event = [[SecLaunchEvent alloc] initWithName:eventname];
        }
        self.events[eventname] = event;
    }
}

- (NSDictionary<NSString*,id>* _Nullable) metricsReport {
    NSMutableDictionary *metric = [NSMutableDictionary dictionary];
    if (self.launched == NO) {
        return nil;
    }

    @synchronized(self) {
        /* don't need to lock children, at this point, they have launched and will no longer mutate */
        [self.dependantLaunches enumerateKeysAndObjectsUsingBlock:^(NSString *_Nonnull childKey, SecLaunchSequence *_Nonnull child, BOOL * _Nonnull stop) {
            os_assert(child.launched);
            [child.events enumerateKeysAndObjectsUsingBlock:^(NSString * _Nonnull key, SecLaunchEvent * _Nonnull obj, BOOL * _Nonnull clientStop) {
                SecLaunchEvent *childEvent = [obj copy];
                childEvent.name = [NSString stringWithFormat:@"c:%@-%@", childKey, childEvent.name];
                self.events[[NSString stringWithFormat:@"c:%@-%@", childKey, key]] = childEvent;
            }];
            self.attributes[[NSString stringWithFormat:@"c:%@", childKey]] = child.attributes;
        }];

        SecLaunchEvent *event = [[SecLaunchEvent alloc] initWithName:(self.firstLaunch ? @"first-launch" : @"re-launch")];
        self.events[event.name] = event;

        metric[@"events"] = [self eventsRelativeTime];
        if (self.attributes.count) {
            metric[@"attributes"] = self.attributes;
        }

    }
    return metric;
}

- (void)launch {
    @synchronized(self) {
        if (self.launched) {
            return;
        }
        self.launched = true;
    }
}

- (NSArray *) eventsRelativeTime
{
    NSMutableArray<SecLaunchEvent *>* array = [NSMutableArray array];
    [self.events enumerateKeysAndObjectsUsingBlock:^(NSString * _Nonnull __unused name, SecLaunchEvent * _Nonnull event, BOOL * _Nonnull __unused stop) {
        [array addObject:event];
    }];
    [array sortUsingComparator:^NSComparisonResult(SecLaunchEvent * _Nonnull obj1, SecLaunchEvent *_Nonnull obj2) {
        return [obj1.date compare:obj2.date];
    }];
    NSDate *firstEvent = array[0].date;
    NSMutableArray<NSDictionary *> *result = [NSMutableArray array];
    [array enumerateObjectsUsingBlock:^(SecLaunchEvent * _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
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
        [self.events enumerateKeysAndObjectsUsingBlock:^(NSString * _Nonnull name, SecLaunchEvent * _Nonnull event, BOOL * _Nonnull stop) {
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
