//
//  SFAnalytics-SFACollection.m
//  Security
//

#import <Foundation/Foundation.h>

#import "SFAnalytics+Internal.h"
#import "SFAnalyticsCollection.h"

#import "SECSFARules.h"
#import "SECSFAEventRule.h"
#import "SECSFAAction.h"
#import "SECSFAActionAutomaticBugCapture.h"
#import "SECSFAActionTapToRadar.h"
#import "SECSFAActionDropEvent.h"
#import "SECSFAVersionMatch.h"
#import "SECSFAEventFilter.h"
#import "SECSFAVersion.h"

#import "NSError+UsefulConstructors.h"

static NSString *kSFAErrorDomain = @"com.apple.SFAErrorDomain";
typedef NS_ERROR_ENUM(kSFAErrorDomain, kSFAErrorCode) {
    kSFAErrorsRulesMissing = 1,
    kSFAErrorTypeMissing,
    kSFAErrorRulesInvalidType,
    kSFAErrorMatchMissing,
    kSFAErrorSecondInvalid,
    kSFAErrorActionInvalidType,
    kSFAErrorActionInvalid,
    kSFAErrorRadarInvalidType,
    kSFAErrorTTRAttributeInvalidType,
    kSFAErrorABCAttributeInvalidType,
    kSFAErrorUnknownAction,
    kSFAErrorFailedToEncodeMatchStructure,
    kSFAErrorsPropsMissing,
    kSFAErrorPropsInvalidType,

};

@implementation SFAnalytics (SFACollection)

+ (SECSFAAction * _Nullable)parseAction:(NSDictionary *)action error:(NSError **)error {
    
    SECSFAAction *a = [[SECSFAAction alloc] init];

    if (![action isKindOfClass:[NSDictionary class]]) {
        if (error) {
            NSError *e = [NSError errorWithDomain:kSFAErrorDomain code:kSFAErrorActionInvalidType description:@"action invalid type"];
            *error = e;
        }
        return nil;
    }

    NSString *radarNumber = action[@"radarNumber"];
    if (radarNumber != nil && ![radarNumber isKindOfClass:[NSString class]]) {
        if (error) {
            NSError *e = [NSError errorWithDomain:kSFAErrorDomain code:kSFAErrorRadarInvalidType description:@"radarNumber invalid"];
            *error = e;
        }
        return nil;
    }
    a.radarnumber = radarNumber;
    NSString *actionType = action[@"actionType"];

    if ([actionType isEqual:@"ttr"]) {
        SECSFAActionTapToRadar *ttr = [[SECSFAActionTapToRadar alloc] init];
        if (ttr == nil) {
            return nil;
        }

        NSString *alert = action[@"alert"];
        NSString *componentID = action[@"componentID"];
        NSString *componentName = action[@"componentName"];
        NSString *componentVersion = action[@"componentVersion"];
        NSString *radarDescription = action[@"radarDescription"];

        if (![alert isKindOfClass:[NSString class]] ||
            ![componentID isKindOfClass:[NSString class]] ||
            ![componentName isKindOfClass:[NSString class]] ||
            ![componentVersion isKindOfClass:[NSString class]] ||
            ![radarDescription isKindOfClass:[NSString class]])
        {
            if (error) {
                NSError *e = [NSError errorWithDomain:kSFAErrorDomain code:kSFAErrorTTRAttributeInvalidType description:@"attribute invalid type"];
                *error = e;
            }
            return nil;
        }

        ttr.alert = alert;
        ttr.componentID = componentID;
        ttr.componentName = componentName;
        ttr.componentVersion = componentVersion;
        ttr.radarDescription = radarDescription;

        a.ttr = ttr;

    } else if ([actionType isEqual:@"abc"]) {
        SECSFAActionAutomaticBugCapture *abc = [[SECSFAActionAutomaticBugCapture alloc] init];

        if (radarNumber == nil) {
            return nil;
        }

        NSString *domain = action[@"domain"];
        NSString *type = action[@"type"];
        NSString *subtype = action[@"subtype"];
        if (![domain isKindOfClass:[NSString class]] ||
            ![type isKindOfClass:[NSString class]] ||
            (subtype != nil && ![subtype isKindOfClass:[NSString class]]))
        {
            if (error) {
                NSError *e = [NSError errorWithDomain:kSFAErrorDomain code:kSFAErrorABCAttributeInvalidType description:@"abc invalid type"];
                *error = e;
            }
            return nil;
        }

        abc.domain = domain;
        abc.type = type;
        abc.subtype = subtype;

        a.abc = abc;
    } else if ([actionType isEqual:@"drop"]) {
        SECSFAActionDropEvent *drop = [[SECSFAActionDropEvent alloc] init];
        drop.excludeEvent = [action[@"event"] boolValue];
        drop.excludeCount = [action[@"count"] boolValue];
        a.drop = drop;
    } else {
        if (error) {
            NSString *str = [NSString stringWithFormat:@"action unknown: %@", actionType];
            NSError *e = [NSError errorWithDomain:kSFAErrorDomain code:kSFAErrorUnknownAction description:str];
            *error = e;
        }
        return nil;
    }

    return a;
}


+ (BOOL)parseRules:(NSArray *)rules format:(SECSFARules *)sfaRules error:(NSError **)error {
    if (![rules isKindOfClass:[NSArray class]]) {
        if (error) {
            NSError *e = [NSError errorWithDomain:kSFAErrorDomain code:kSFAErrorsRulesMissing description:@"rules key missing"];
            *error = e;
        }
        return NO;
    }
    for (NSDictionary *item in rules) {
        SECSFAEventClass eventClassInteger = SECSFAEventClass_Errors;
        
        if (![item isKindOfClass:[NSDictionary class]]) {
            NSError *e = [NSError errorWithDomain:kSFAErrorDomain code:kSFAErrorRulesInvalidType description:@"rules type invalid"];
            if (error) {
                *error = e;
            }
            return NO;
        }

        NSString *eventType = item[@"eventType"];
        if (![eventType isKindOfClass:[NSString class]]) {
            if (error) {
                NSError *e = [NSError errorWithDomain:kSFAErrorDomain code:kSFAErrorTypeMissing description:@"eventType missing"];
                *error = e;
            }
            return NO;
        }
        NSString *eventClass = item[@"eventClass"];
        if (eventClass != nil) {
            if (![eventClass isKindOfClass:[NSString class]]) {
                if (error) {
                    NSError *e = [NSError errorWithDomain:kSFAErrorDomain code:kSFAErrorTypeMissing description:@"eventType not a string"];
                    *error = e;
                }
                return NO;
            }
            if ([eventClass isEqual:@"all"]) {
                eventClassInteger = SECSFAEventClass_All;
            } else if ([eventClass isEqual:@"errors"]) {
                eventClassInteger = SECSFAEventClass_Errors;
            } else if ([eventClass isEqual:@"success"]) {
                eventClassInteger = SECSFAEventClass_Success;
            } else if ([eventClass isEqual:@"hardfail"]) {
                eventClassInteger = SECSFAEventClass_HardFailure;
            } else if ([eventClass isEqual:@"softfail"]) {
                eventClassInteger = SECSFAEventClass_SoftFailure;
            } else if ([eventClass isEqual:@"note"]) {
                eventClassInteger = SECSFAEventClass_Note;
            } else if ([eventClass isEqual:@"rockwell"]) {
                eventClassInteger = SECSFAEventClass_Rockwell;
            } else {
                if (error) {
                    NSError *e = [NSError errorWithDomain:kSFAErrorDomain
                                                     code:kSFAErrorTypeMissing
                                              description:[NSString stringWithFormat:@"unknown eventclass: %@", eventClass]];
                    *error = e;
                }
                return NO;
            }
        }
        NSDictionary *match = item[@"match"];
        
        if (![match isKindOfClass:[NSDictionary class]]) {
            NSError *e = [NSError errorWithDomain:kSFAErrorDomain code:kSFAErrorMatchMissing description:@"match missing"];
            if (error) {
                *error = e;
            }
            return NO;
        }
        NSNumber *repeatAfterSeconds = item[@"repeatAfterSeconds"];
        if (repeatAfterSeconds != nil && ![repeatAfterSeconds isKindOfClass:[NSNumber class]]) {
            if (error) {
                NSError *e = [NSError errorWithDomain:kSFAErrorDomain code:kSFAErrorSecondInvalid description:@"repeatAfterSeconds not number"];
                *error = e;
            }
            return NO;
        }
        NSString *processName = item[@"processName"];
        if (processName != nil && ![processName isKindOfClass:[NSString class]]) {
            if (error) {
                NSError *e = [NSError errorWithDomain:kSFAErrorDomain code:kSFAErrorSecondInvalid description:@"processName not string"];
                *error = e;
            }
            return NO;
        }
        
        NSNumber *matchOnFirstFailure = item[@"matchOnFirstFailure"];
        if (matchOnFirstFailure != nil && ![matchOnFirstFailure isKindOfClass:[NSNumber class]]) {
            if (error) {
                NSError *e = [NSError errorWithDomain:kSFAErrorDomain code:kSFAErrorSecondInvalid description:@"matchOnFirstFailure not number"];
                *error = e;
            }
            return NO;
        }


        NSError *matchError = nil;
        SECSFAEventRule *rule = [[SECSFAEventRule alloc] init];
        rule.eventType = eventType;
        if (eventClassInteger) {
            rule.eventClass = eventClassInteger;
        }
        rule.processName = processName;
        rule.repeatAfterSeconds = [repeatAfterSeconds intValue];
        rule.matchOnFirstFailure = [matchOnFirstFailure intValue];
        rule.match = [NSPropertyListSerialization dataWithPropertyList:match
                                                                format:NSPropertyListBinaryFormat_v1_0
                                                               options:0
                                                                 error:&matchError];
        if (rule.match == nil) {
            if (error) {
                *error = [NSError errorWithDomain:kSFAErrorDomain code:kSFAErrorFailedToEncodeMatchStructure description:@"plist encode failed" underlying:matchError];
            }
            return NO;
        }

        NSDictionary *action = item[@"action"];
        rule.action = [self parseAction:action error:error];
        if (rule.action == nil) {
            return NO;
        }

        [sfaRules addEventRules:rule];
    }
    
    return YES;
}

+ (BOOL)parseVersions:(NSArray *)versions format:(SECSFARules *)sfaRules error:(NSError **)error {
    if (![versions isKindOfClass:[NSArray class]]) {
        if (error) {
            NSError *e = [NSError errorWithDomain:kSFAErrorDomain code:kSFAErrorsRulesMissing description:@"versions key missing"];
            *error = e;
        }
        return NO;
    }
    SECSFAVersionMatch *builds = [[SECSFAVersionMatch alloc] init];
    for (NSDictionary *item in versions) {
        if (![item isKindOfClass:[NSDictionary class]]) {
            NSError *e = [NSError errorWithDomain:kSFAErrorDomain code:kSFAErrorRulesInvalidType description:@"versions type invalid"];
            if (error) {
                *error = e;
            }
            return NO;
        }
        NSString *version = item[@"version"];
        NSString *platform = item[@"platform"];
        if (![version isKindOfClass:[NSString class]] || ![platform isKindOfClass:[NSString class]]) {
            NSError *e = [NSError errorWithDomain:kSFAErrorDomain
                                             code:kSFAErrorRulesInvalidType
                                      description:[NSString stringWithFormat:@"versions it string: %@", item]];
            if (error) {
                *error = e;
            }
            return NO;
        }

        SECSFAVersion *v = [SFAnalyticsCollection parseVersion:version platform:platform];
        if (v == nil) {
            NSError *e = [NSError errorWithDomain:kSFAErrorDomain
                                             code:kSFAErrorRulesInvalidType
                                      description:[NSString stringWithFormat:@"versions not parsing: %@", item]];
            if (error) {
                *error = e;
            }
            return NO;
        }

        [builds addVersions:v];
    }
    if (builds.versions.count > 0) {
        sfaRules.allowedBuilds = builds;
    }

    return YES;
}

+ (BOOL)parseEventFilter:(NSDictionary *)events format:(SECSFARules *)sfaRules error:(NSError **)error {
    if (![events isKindOfClass:[NSDictionary class]]) {
        if (error) {
            NSError *e = [NSError errorWithDomain:kSFAErrorDomain code:kSFAErrorsRulesMissing description:@"events key missing"];
            *error = e;
        }
        return NO;
    }
    __block NSError *e = nil;
    [events enumerateKeysAndObjectsUsingBlock:^(NSString* _Nonnull key, NSNumber* _Nonnull number, BOOL * _Nonnull stop) {
        if (![key isKindOfClass:[NSString class]] || ![number isKindOfClass:[NSNumber class]]) {
            e = [NSError errorWithDomain:kSFAErrorDomain code:kSFAErrorRulesInvalidType description:@"events type invalid"];
            *stop = YES;
            return;
        }
        SECSFAEventFilter *event = [[SECSFAEventFilter alloc] init];
        event.event = key;
        long percent = [number integerValue];
        if (percent <= 0) {
            event.dropRate = 100;
        } else if (percent >= 100) {
            event.dropRate = 0;
        } else {
            event.dropRate = 100 - percent;
        }
        [sfaRules addEventFilter:event];
    }];
    if (e) {
        if (error) {
            *error = e;
        }
        return NO;
    }
    return YES;
}


+ (NSData *)encodeSFACollection:(NSData *)json error:(NSError **)error
{
    SECSFARules *sfaRules = [[SECSFARules alloc] init];
    if (sfaRules == nil) {
        return nil;
    }
    
    NSDictionary *sfaCollection = [NSJSONSerialization JSONObjectWithData:json options:0 error:error];
    if (![sfaCollection isKindOfClass:[NSDictionary class]]) {
        return nil;
    }
    NSArray *rules = sfaCollection[@"rules"];
    if (rules) {
        if (![self parseRules:rules format:sfaRules error:error]) {
            return nil;
        }
    }
    
    NSArray *versions = sfaCollection[@"versions"];
    if (versions) {
        if (![self parseVersions:versions format:sfaRules error:error]) {
            return nil;
        }
    }
    
    NSDictionary *eventFilter = sfaCollection[@"eventFilter"];
    if (eventFilter) {
        if (![self parseEventFilter:eventFilter format:sfaRules error:error]) {
            return nil;
        }
    }

    NSData *data = [sfaRules data];
    if (data == NULL) {
        return nil;
    }

    return [data compressedDataUsingAlgorithm:NSDataCompressionAlgorithmLZFSE
                                        error:error];
}

@end
