#if OCTAGON

#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/ot/OctagonStateMachineObservers.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/ot/OTConstants.h"

@implementation OctagonStateTransitionPathStep

- (instancetype)initAsSuccess
{
    if((self = [super init])) {
        _successState = YES;
        _followStates = @{};
    }
    return self;
}
- (instancetype)initWithPath:(NSDictionary<OctagonState*, OctagonStateTransitionPathStep*>*)followStates
{
    if((self = [super init])) {
        _successState = NO;
        _followStates = followStates;
    }
    return self;
}

- (OctagonStateTransitionPathStep*)nextStep:(OctagonState*)stateStep
{
    // If stateStep matches a followState, return it. Otherwise, return nil.
    return self.followStates[stateStep];
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"<OSTPath(%@)>", self.followStates.allKeys];
}

+ (OctagonStateTransitionPathStep*)success
{
    return [[OctagonStateTransitionPathStep alloc] initAsSuccess];
}

+ (OctagonStateTransitionPathStep*)pathFromDictionary:(NSDictionary<OctagonState*, id>*)pathDict
{
    NSMutableDictionary<OctagonState*, OctagonStateTransitionPathStep*>* converted = [NSMutableDictionary dictionary];
    for(id key in pathDict.allKeys) {
        id obj = pathDict[key];

        if([obj isKindOfClass:[OctagonStateTransitionPathStep class]]) {
            converted[key] = obj;
        } else if([obj isKindOfClass:[NSDictionary class]]) {
            converted[key] = [OctagonStateTransitionPathStep pathFromDictionary:(NSDictionary*)obj];
        }
    }

    if([converted count] == 0) {
        return [[OctagonStateTransitionPathStep alloc] initAsSuccess];
    }

    return [[OctagonStateTransitionPathStep alloc] initWithPath:converted];
}
@end

#pragma mark - OctagonStateTransitionPath

@implementation OctagonStateTransitionPath
- (instancetype)initWithState:(OctagonState*)initialState
                     pathStep:(OctagonStateTransitionPathStep*)pathStep
{
    if((self = [super init])) {
        _initialState = initialState;
        _pathStep = pathStep;
    }
    return self;
}

- (OctagonStateTransitionPathStep*)asPathStep
{
    return [[OctagonStateTransitionPathStep alloc] initWithPath:@{
        self.initialState: self.pathStep,
    }];
}

+ (OctagonStateTransitionPath* _Nullable)pathFromDictionary:(NSDictionary<OctagonState*, id>*)pathDict
{
    for(id key in pathDict.allKeys) {
        id obj = pathDict[key];

        if([obj isKindOfClass:[OctagonStateTransitionPathStep class]]) {
            return [[OctagonStateTransitionPath alloc] initWithState:key
                                                            pathStep:obj];
        } else if([obj isKindOfClass:[NSDictionary class]]) {
            return [[OctagonStateTransitionPath alloc] initWithState:key
                                                            pathStep:[OctagonStateTransitionPathStep pathFromDictionary:obj]];
        }
    }
    return nil;
}
@end


#pragma mark - OctagonStateTransitionWatcher

@interface OctagonStateTransitionWatcher ()
@property BOOL active;
@property BOOL completed;
@property (nullable) OctagonStateTransitionPathStep* remainingPath;
@property NSOperationQueue* operationQueue;
@property bool timeoutCanOccur;
@property dispatch_queue_t queue;
@end

@implementation OctagonStateTransitionWatcher

- (instancetype)initNamed:(NSString*)name
              serialQueue:(dispatch_queue_t)queue
              path:(OctagonStateTransitionPath*)pathBeginning
{
    if((self = [super init])) {
        _name = name;
        _intendedPath = pathBeginning;
        _remainingPath = [pathBeginning asPathStep];

        _result = [CKKSResultOperation named:[NSString stringWithFormat:@"watcher-%@", name] withBlock:^{}];
        _operationQueue = [[NSOperationQueue alloc] init];

        _queue = queue;

        _timeoutCanOccur = true;

        _active = NO;
        _completed = NO;
    }
    return self;
}

- (NSString*)description {
    return [NSString stringWithFormat:@"<OctagonStateTransitionWatcher(%@): remaining: %@, result: %@>",
            self.name,
            self.remainingPath,
            self.result];
}

- (void)onqueueHandleTransition:(CKKSResultOperation<OctagonStateTransitionOperationProtocol>*)attempt
{
    dispatch_assert_queue(self.queue);

    // Early-exit to make error handling better
    if(self.remainingPath == nil || self.completed) {
        return;
    }

    if(self.active) {
        [self onqueueProcessTransition:attempt];

    } else {
        if([attempt.nextState isEqualToString:self.intendedPath.initialState]) {
            self.active = YES;
            [self onqueueProcessTransition:attempt];
        }
    }
}

- (instancetype)timeout:(dispatch_time_t)timeout
{
    WEAKIFY(self);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, timeout), self.queue, ^{
        STRONGIFY(self);
        if(self.timeoutCanOccur) {
            self.timeoutCanOccur = false;

            NSString* description = [NSString stringWithFormat:@"Operation(%@) timed out waiting to start for [%@]",
                                     self.name,
                                     self.remainingPath];

            self.result.error = [NSError errorWithDomain:CKKSResultErrorDomain
                                                 code:CKKSResultTimedOut
                                          description:description
                                           underlying:nil];
            [self onqueueStartFinishOperation];

        }
    });

    return self;
}

- (void)onqueueProcessTransition:(CKKSResultOperation<OctagonStateTransitionOperationProtocol>*)attempt
{
    dispatch_assert_queue(self.queue);

    if(self.remainingPath == nil || self.completed) {
        return;
    }


    OctagonStateTransitionPathStep* nextPath = [self.remainingPath nextStep:attempt.nextState];

    if(nextPath) {
        self.remainingPath = nextPath;
        if(self.remainingPath.successState) {
            // We're done!
            [self onqueueStartFinishOperation];
        }

    } else {
        // We're off the path. Error and finish.
        if(attempt.error) {
            self.result.error = attempt.error;
        } else {
            self.result.error = [NSError errorWithDomain:OctagonErrorDomain
                                                    code:OTErrorUnexpectedStateTransition
                                             description:[NSString stringWithFormat:@"state became %@, was expecting %@", attempt.nextState, self.remainingPath]];
        }
        [[CKKSAnalytics logger] logUnrecoverableError:self.result.error
                                             forEvent:OctagonEventStateTransition
                                       withAttributes:@{
                                                        @"name" : self.name,
                                                        @"intended": [self.remainingPath.followStates allKeys],
                                                        @"became" : attempt.nextState,
                                                        }];

        [self onqueueStartFinishOperation];
    }
}

- (void)onqueueStartFinishOperation {
    dispatch_assert_queue(self.queue);

    self.timeoutCanOccur = false;
    [self.operationQueue addOperation:self.result];
    self.active = false;
    self.completed = TRUE;
}

@end

#endif
