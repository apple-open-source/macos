/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#if __OBJC2__

#import "SFSignInAnalytics.h"
#import "SFSignInAnalytics+Internal.h"

#import <Analytics/SFAnalytics+Signin.h>
#import "SFAnalyticsDefines.h"
#import "SFAnalyticsSQLiteStore.h"
#import "SFAnalytics.h"

#import <os/log_private.h>
#import <mach/mach_time.h>
#import <utilities/SecFileLocations.h>
#import <utilities/debugging.h>
#import <utilities/SecCFWrappers.h>

//metrics database location
NSString* signinMetricsDatabase = @"signin_metrics";

//defaults write results location
static NSString* const SFSignInAnalyticsDumpLoggedResultsToLocation = @"/tmp/signin_results.txt";
static NSString* const SFSignInAnalyticsPersistedEventList = @"/tmp/signin_eventlist";

//analytics constants
static NSString* const SFSignInAnalyticsAttributeRecoverableError = @"recoverableError";
static NSString* const SFSignInAnalyticsAttributeErrorDomain = @"errorDomain";
static NSString* const SFSignInAnalyticsAttributeErrorCode = @"errorCode";
static NSString* const SFSignInAnalyticsAttributeErrorChain = @"errorChain";
static NSString* const SFSignInAnalyticsAttributeParentUUID = @"parentUUID";
static NSString* const SFSignInAnalyticsAttributeMyUUID = @"myUUID";
static NSString* const SFSignInAnalyticsAttributeSignInUUID = @"signinUUID";
static NSString* const SFSignInAnalyticsAttributeEventName = @"eventName";
static NSString* const SFSignInAnalyticsAttributeSubsystemName = @"subsystemName";
static NSString* const SFSignInAnalyticsAttributeBuiltDependencyChains = @"dependencyChains";

@implementation SFSIALoggerObject
+ (NSString*)databasePath {
    return [SFSIALoggerObject defaultAnalyticsDatabasePath:signinMetricsDatabase];
}

+ (instancetype)logger
{
    return [super logger];
}
@end


@interface SFSignInAnalytics ()
@property (nonatomic, copy) NSString *signin_uuid;
@property (nonatomic, copy) NSString *my_uuid;
@property (nonatomic, copy) NSString *parent_uuid;
@property (nonatomic, copy) NSString *category;
@property (nonatomic, copy) NSString *eventName;
@property (nonatomic, copy) NSString *persistencePath;

@property (nonatomic, strong) NSURL *persistedEventPlist;
@property (nonatomic, strong) NSMutableDictionary *eventDependencyList;
@property (nonatomic, strong) NSMutableArray *builtDependencyChains;

@property (nonatomic) BOOL canceled;
@property (nonatomic) BOOL stopped;

@property (nonatomic, strong) os_log_t logObject;

@property (nonatomic, strong) NSNumber *measurement;

@property (nonatomic, strong) dispatch_queue_t queue;

@property (nonatomic, strong) SFSignInAnalytics *root;
@property (nonatomic, strong) SFAnalyticsActivityTracker *tracker;

-(os_log_t) newLogForCategoryName:(NSString*) category;
-(os_log_t) logForCategoryName:(NSString*) category;

@end

static NSMutableDictionary *logObjects;
static const NSString* signInLogSpace = @"com.apple.security.wiiss";

@implementation SFSignInAnalytics

+ (BOOL)supportsSecureCoding {
    return YES;
}

-(os_log_t) logForCategoryName:(NSString*) category
{
    return logObjects[category];
}

-(os_log_t) newLogForCategoryName:(NSString*) category
{
    return os_log_create([signInLogSpace UTF8String], [category UTF8String]);
}

- (BOOL)writeDependencyList:(NSError**)error
{
    NSError *localError = nil;
    if (![NSPropertyListSerialization propertyList: self.root.eventDependencyList isValidForFormat: NSPropertyListXMLFormat_v1_0]){
        os_log_error(self.logObject, "can't save PersistentState as XML");
        return false;
    }

    NSData *data = [NSPropertyListSerialization dataWithPropertyList: self.root.eventDependencyList
                                                              format: NSPropertyListXMLFormat_v1_0 options: 0 error: &localError];
    if (data == nil){
        os_log_error(self.logObject, "error serializing PersistentState to xml: %@", localError);
        return false;
    }

    BOOL writeStatus = [data writeToURL:self.root.persistedEventPlist options: NSDataWritingAtomic error: &localError];
    if (!writeStatus){
        os_log_error(self.logObject, "error writing PersistentState to file: %@", localError);
    }
    if(localError && error){
        *error = localError;
    }

    return writeStatus;
}

- (instancetype)initWithSignInUUID:(NSString *)uuid category:(NSString *)category eventName:(NSString*)eventName
{
    self = [super init];
    if (self) {
        _signin_uuid = uuid;

        _my_uuid = uuid;
        _parent_uuid = uuid;
        _eventName = eventName;
        _category = category;
        _root = self;
        _canceled = NO;
        _stopped = NO;
        _builtDependencyChains = [NSMutableArray array];

        //make plist file containing uuid parent/child
        _persistencePath = [NSString stringWithFormat:@"%@-%@.plist", SFSignInAnalyticsPersistedEventList, eventName];
        _persistedEventPlist = [NSURL fileURLWithPath:_persistencePath isDirectory:NO];

        _eventDependencyList = [NSMutableDictionary dictionary];
        [_eventDependencyList setObject:[NSMutableArray array] forKey:_signin_uuid];

        _tracker = [[SFSIALoggerObject logger] logSystemMetricsForActivityNamed:eventName withAction:nil];
        [_tracker start];

        NSError* error = nil;
        if(![self writeDependencyList:&error]){
            os_log(self.logObject, "attempting to write dependency list: %@", error);
        }

        _queue = dispatch_queue_create("com.apple.security.SignInAnalytics", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);

        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            logObjects = [NSMutableDictionary dictionary];
        });
        @synchronized(logObjects){
            if(category){
                _logObject = [self logForCategoryName:category];

                if(!_logObject){
                    _logObject = [self newLogForCategoryName:category];
                    [logObjects setObject:_logObject forKey:category];
                }
            }
        }
    }
    return self;
}

-(instancetype) initChildWithSignInUUID:(NSString*)uuid andCategory:(NSString*)category andEventName:(NSString*)eventName
{
    self = [super init];
    if (self) {
        _signin_uuid = uuid;

        _my_uuid = uuid;
        _parent_uuid = uuid;
        _eventName = eventName;
        _category = category;
        _canceled = NO;
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_signin_uuid forKey:@"UUID"];
    [coder encodeObject:_category forKey:@"category"];
    [coder encodeObject:_parent_uuid forKey:@"parentUUID"];
    [coder encodeObject:_my_uuid forKey:@"myUUID"];
    [coder encodeObject:_measurement forKey:@"measurement"];
    [coder encodeObject:_eventName forKey:@"eventName"];
}

- (nullable instancetype)initWithCoder:(NSCoder *)decoder
{
    self = [super init];
    if (self) {
        _signin_uuid = [decoder decodeObjectOfClass:[NSString class] forKey:@"UUID"];
        _category = [decoder decodeObjectOfClass:[NSString class] forKey:@"category"];
        _parent_uuid = [decoder decodeObjectOfClass:[NSString class] forKey:@"parentUUID"];
        _my_uuid = [decoder decodeObjectOfClass:[NSString class] forKey:@"myUUID"];
        _measurement = [decoder decodeObjectOfClass:[NSString class] forKey:@"measurement"];
        _eventName = [decoder decodeObjectOfClass:[NSString class] forKey:@"eventName"];
        _queue = dispatch_queue_create("com.apple.security.SignInAnalytics", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);

        if(_signin_uuid == nil ||
           _category == nil ||
           _parent_uuid == nil){
            [decoder failWithError:[NSError errorWithDomain:@"securityd" code:errSecDecode userInfo:@{NSLocalizedDescriptionKey: @"Failed to decode SignInAnalytics object"}]];
            return nil;
        }
    }
    return self;
}

- (SFSignInAnalytics*)newSubTaskForEvent:(NSString*)eventName
{
    SFSignInAnalytics *newSubTask = [[SFSignInAnalytics alloc] initChildWithSignInUUID:self.signin_uuid andCategory:self.category andEventName:self.eventName];
    if(newSubTask){
        newSubTask.my_uuid = [NSUUID UUID].UUIDString;
        newSubTask.parent_uuid = self.my_uuid;
        newSubTask.signin_uuid = self.signin_uuid;

        newSubTask.category = self.category;
        newSubTask.eventName = [eventName copy];
        newSubTask.root = self.root;
        newSubTask.canceled = NO;
        newSubTask.stopped = NO;

        newSubTask.queue = dispatch_queue_create("com.apple.security.SignInAnalytics", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
        newSubTask.tracker = [[SFSIALoggerObject logger] logSystemMetricsForActivityNamed:eventName withAction:nil];
        [newSubTask.tracker start];

        @synchronized(_eventDependencyList){
            NSMutableArray *parentEntry = [newSubTask.root.eventDependencyList objectForKey:newSubTask.parent_uuid];
            
            //add new subtask entry to parent event's list
            [parentEntry addObject:newSubTask.my_uuid];
            [newSubTask.root.eventDependencyList setObject:parentEntry forKey:newSubTask.parent_uuid];
            
            //create new array list for this new subtask incase it has subtasks
            [newSubTask.root.eventDependencyList setObject:[NSMutableArray array] forKey:newSubTask.my_uuid];
            NSError* error = nil;
            if(![newSubTask writeDependencyList:&error]){
                os_log(self.logObject, "attempting to write dependency list: %@", error);
            }
        }
    }

    return newSubTask;
}

- (void)logRecoverableError:(NSError*)error
{

    if (error == nil){
        os_log_error(self.logObject, "attempting to log a nil error for event:%@", self.eventName);
        return;
    }

    os_log_error(self.logObject, "%@", error);

    NSMutableDictionary* eventAttributes = [NSMutableDictionary dictionary];

    [eventAttributes setValuesForKeysWithDictionary:@{
                                                      SFSignInAnalyticsAttributeRecoverableError : @(YES),
                                                      SFSignInAnalyticsAttributeErrorDomain : error.domain,
                                                      SFSignInAnalyticsAttributeErrorCode : @(error.code),
                                                      SFSignInAnalyticsAttributeMyUUID : self.my_uuid,
                                                      SFSignInAnalyticsAttributeParentUUID : self.parent_uuid,
                                                      SFSignInAnalyticsAttributeSignInUUID : self.signin_uuid,
                                                      SFSignInAnalyticsAttributeEventName : self.eventName,
                                                      SFSignInAnalyticsAttributeSubsystemName : self.category
                                                      }];

    [[SFSIALoggerObject logger] logSoftFailureForEventNamed:self.eventName withAttributes:eventAttributes];

}

- (void)logUnrecoverableError:(NSError*)error
{
    if (error == nil){
        os_log_error(self.logObject, "attempting to log a nil error for event:%@", self.eventName);
        return;
    }

    os_log_error(self.logObject, "%@", error);

    NSMutableDictionary* eventAttributes = [NSMutableDictionary dictionary];

    [eventAttributes setValuesForKeysWithDictionary:@{
                                                      SFSignInAnalyticsAttributeRecoverableError : @(NO),
                                                      SFSignInAnalyticsAttributeErrorDomain : error.domain,
                                                      SFSignInAnalyticsAttributeErrorCode : @(error.code),
                                                      SFSignInAnalyticsAttributeMyUUID : self.my_uuid,
                                                      SFSignInAnalyticsAttributeParentUUID : self.parent_uuid,
                                                      SFSignInAnalyticsAttributeSignInUUID : self.signin_uuid,
                                                      SFSignInAnalyticsAttributeEventName : self.eventName,
                                                      SFSignInAnalyticsAttributeSubsystemName : self.category
                                                      }];

    [[SFSIALoggerObject logger] logHardFailureForEventNamed:self.eventName withAttributes:eventAttributes];
}

-(void)cancel
{
    dispatch_sync(self.queue, ^{
        [self.tracker cancel];
        self.canceled = YES;
        os_log(self.logObject, "canceled timer for %@", self.eventName);
    });
}

- (void)stopWithAttributes:(NSDictionary<NSString*, id>*)attributes
{
    dispatch_sync(self.queue, ^{

        if(self.canceled || self.stopped){
            return;
        }

        self.stopped = YES;

        [self.tracker stop];

        NSMutableDictionary *mutableAttributes = nil;

        if(attributes){
            mutableAttributes = [NSMutableDictionary dictionaryWithDictionary:attributes];
        }
        else{
            mutableAttributes = [NSMutableDictionary dictionary];
        }
        mutableAttributes[SFSignInAnalyticsAttributeMyUUID] = self.my_uuid;
        mutableAttributes[SFSignInAnalyticsAttributeParentUUID] = self.parent_uuid;
        mutableAttributes[SFSignInAnalyticsAttributeSignInUUID] = self.signin_uuid;
        mutableAttributes[SFSignInAnalyticsAttributeEventName] = self.eventName;
        mutableAttributes[SFSignInAnalyticsAttributeSubsystemName] = self.category;

        [mutableAttributes enumerateKeysAndObjectsUsingBlock:^(NSString* key, id obj, BOOL * stop) {
            os_log(self.logObject, "event: %@, %@  :  %@", self.eventName, key, obj);
        }];
        
        [[SFSIALoggerObject logger] logSuccessForEventNamed:self.eventName];
        [[SFSIALoggerObject logger] logSoftFailureForEventNamed:self.eventName withAttributes:mutableAttributes];
    });
}

-(BOOL) writeResultsToTmp {

    bool shouldWriteResultsToTemp = NO;
    CFBooleanRef toTmp = (CFBooleanRef)CFPreferencesCopyValue(CFSTR("DumpResultsToTemp"),
                                                                CFSTR("com.apple.security"),
                                                                kCFPreferencesAnyUser, kCFPreferencesAnyHost);
    if(toTmp && CFGetTypeID(toTmp) == CFBooleanGetTypeID()){
        if(toTmp == kCFBooleanFalse){
            os_log(self.logObject, "writing results to splunk");
            shouldWriteResultsToTemp = NO;
        }
        if(toTmp == kCFBooleanTrue){
            os_log(self.logObject, "writing results to /tmp");
            shouldWriteResultsToTemp = YES;
        }
    }

    CFReleaseNull(toTmp);
    return shouldWriteResultsToTemp;
}

- (void)processEventChainForUUID:(NSString*)uuid dependencyChain:(NSString*)dependencyChain
{
    NSString* newChain = dependencyChain;

    NSArray* children = [self.root.eventDependencyList objectForKey:uuid];
    for (NSString* child in children) {
        newChain = [NSString stringWithFormat:@"%@, %@", dependencyChain, child];
        [self processEventChainForUUID:child dependencyChain:newChain];
    }
    if([children count] == 0){
        [self.root.builtDependencyChains addObject:newChain];
        os_log(self.logObject, "current dependency chain list: %@", newChain);
    }
}

- (void)signInCompleted
{
    //print final
    os_log(self.logObject, "sign in complete");
    NSError* error = nil;

    //create dependency chains and log them
    [self processEventChainForUUID:self.root.my_uuid dependencyChain:self.root.signin_uuid];
    //write to database
    if([self.root.builtDependencyChains count] > 0){
        NSDictionary* eventAttributes =  @{SFSignInAnalyticsAttributeBuiltDependencyChains : self.root.builtDependencyChains};
        [[SFSIALoggerObject logger] logSoftFailureForEventNamed:SFSignInAnalyticsAttributeBuiltDependencyChains withAttributes:eventAttributes];
    }

    BOOL writingToTmp = [self writeResultsToTmp];
    if(writingToTmp){ //writing sign in analytics to /tmp
        os_log(self.logObject, "logging to /tmp");

        NSData* eventData = [NSKeyedArchiver archivedDataWithRootObject:[[SFSIALoggerObject logger].database allEvents] requiringSecureCoding:YES error:&error];
        if(eventData){
            [eventData writeToFile:SFSignInAnalyticsDumpLoggedResultsToLocation options:0 error:&error];

            if(error){
                os_log_error(self.logObject, "error writing to file [%@], error:%@", SFSignInAnalyticsDumpLoggedResultsToLocation, error);
            }else{
                os_log(self.logObject, "successfully wrote sign in analytics to:%@", SFSignInAnalyticsDumpLoggedResultsToLocation);
            }
        }else{
            os_log_error(self.logObject, "collected no data");
        }
    }else{ //writing to splunk
        os_log(self.logObject, "logging to splunk");
    }
    
    //remove dependency list
    BOOL removedPersistedDependencyList = [[NSFileManager defaultManager] removeItemAtPath:self.persistencePath error:&error];

    if(!removedPersistedDependencyList || error){
        os_log(self.logObject, "encountered error when attempting to remove persisted event list: %@", error);
    }

}

@end
#endif

