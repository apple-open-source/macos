//
//  SFAnalytics-SFACollection.m
//  Security
//

#import <Foundation/Foundation.h>

#import "SFAnalytics+Internal.h"

#import "SECSFARules.h"
#import "SECSFARule.h"
#import "SECSFAAction.h"
#import "SECSFAActionAutomaticBugCapture.h"
#import "SECSFAActionTapToRadar.h"
#import "SECSFAActionDropEvent.h"

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
};

@implementation SFAnalytics (SFACollection)

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
    if (![rules isKindOfClass:[NSArray class]]) {
        if (error) {
            NSError *e = [NSError errorWithDomain:kSFAErrorDomain code:kSFAErrorsRulesMissing description:@"rules key missing"];
            *error = e;
        }
        return nil;
    }
    for (NSDictionary *item in rules) {
        SECSFAEventClass eventClassInteger = SECSFAEventClass_Errors;
        
        if (![item isKindOfClass:[NSDictionary class]]) {
            NSError *e = [NSError errorWithDomain:kSFAErrorDomain code:kSFAErrorRulesInvalidType description:@"rules type invalid"];
            if (error) {
                *error = e;
            }
            return nil;
        }

        NSString *eventType = item[@"eventType"];
        if (![eventType isKindOfClass:[NSString class]]) {
            if (error) {
                NSError *e = [NSError errorWithDomain:kSFAErrorDomain code:kSFAErrorTypeMissing description:@"eventType missing"];
                *error = e;
            }
            return nil;
        }
        NSString *eventClass = item[@"eventClass"];
        if (eventClass != nil) {
            if (![eventClass isKindOfClass:[NSString class]]) {
                if (error) {
                    NSError *e = [NSError errorWithDomain:kSFAErrorDomain code:kSFAErrorTypeMissing description:@"eventType not a string"];
                    *error = e;
                }
                return nil;
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
                return nil;
            }
        }
        NSDictionary *match = item[@"match"];
        if (![match isKindOfClass:[NSDictionary class]]) {
            NSError *e = [NSError errorWithDomain:kSFAErrorDomain code:kSFAErrorMatchMissing description:@"match missing"];
            if (error) {
                *error = e;
            }
            return nil;
        }
        NSNumber *repeatAfterSeconds = item[@"repeatAfterSeconds"];
        if (repeatAfterSeconds != nil && ![repeatAfterSeconds isKindOfClass:[NSNumber class]]) {
            if (error) {
                NSError *e = [NSError errorWithDomain:kSFAErrorDomain code:kSFAErrorSecondInvalid description:@"repeatAfterSeconds missing"];
                *error = e;
            }
            return nil;
        }

        NSError *matchError = nil;
        SECSFARule *rule = [[SECSFARule alloc] init];
        rule.eventType = eventType;
        if (eventClassInteger) {
            rule.eventClass = eventClassInteger;
        }
        rule.repeatAfterSeconds = [repeatAfterSeconds intValue];
        rule.match = [NSPropertyListSerialization dataWithPropertyList:match
                                                                format:NSPropertyListBinaryFormat_v1_0
                                                               options:0
                                                                 error:&matchError];
        if (rule.match == nil) {
            if (error) {
                *error = [NSError errorWithDomain:kSFAErrorDomain code:kSFAErrorFailedToEncodeMatchStructure description:@"plist encode failed" underlying:matchError];
            }
            return nil;
        }

        rule.action = [[SECSFAAction alloc] init];

        NSDictionary *action = item[@"action"];
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
        rule.action.radarnumber = radarNumber;
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

            rule.action.ttr = ttr;

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

            rule.action.abc = abc;
        } else if ([actionType isEqual:@"drop"]) {
            SECSFAActionDropEvent *drop = [[SECSFAActionDropEvent alloc] init];
            drop.excludeEvent = [action[@"event"] boolValue];
            drop.excludeCount = [action[@"count"] boolValue];
            rule.action.drop = drop;
        } else {
            if (error) {
                NSString *str = [NSString stringWithFormat:@"action unknown: %@", actionType];
                NSError *e = [NSError errorWithDomain:kSFAErrorDomain code:kSFAErrorUnknownAction description:str];
                *error = e;
            }
            return nil;
        }

        [sfaRules addRules:rule];
    }

    NSData *data = [sfaRules data];
    if (data == NULL) {
        return nil;
    }

    return [data compressedDataUsingAlgorithm:NSDataCompressionAlgorithmLZFSE
                                        error:error];
}

@end
