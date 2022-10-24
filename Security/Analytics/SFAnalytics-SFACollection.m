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
        return nil;
    }
    for (NSDictionary *item in rules) {
        if (![item isKindOfClass:[NSDictionary class]]) {
            return nil;
        }

        NSString *eventType = item[@"eventType"];
        if (![eventType isKindOfClass:[NSString class]]) {
            return nil;
        }
        NSDictionary *match = item[@"match"];
        if (![match isKindOfClass:[NSDictionary class]]) {
            return nil;
        }
        NSNumber *repeatAfterSeconds = item[@"repeatAfterSeconds"];
        if (repeatAfterSeconds != nil && ![repeatAfterSeconds isKindOfClass:[NSNumber class]]) {
            return nil;
        }

        SECSFARule *rule = [[SECSFARule alloc] init];
        rule.eventType = eventType;
        rule.repeatAfterSeconds = [repeatAfterSeconds intValue];
        rule.match = [NSPropertyListSerialization dataWithPropertyList:match
                                                                format:NSPropertyListBinaryFormat_v1_0
                                                               options:0
                                                                 error:error];
        if (rule.match == nil) {
            return nil;
        }

        rule.action = [[SECSFAAction alloc] init];

        NSDictionary *action = item[@"action"];
        if (![action isKindOfClass:[NSDictionary class]]) {
            return nil;
        }

        NSString *radarNumber = action[@"radarNumber"];
        if (radarNumber != nil && ![radarNumber isKindOfClass:[NSString class]]) {
            return nil;
        }
        rule.action.radarnumber = radarNumber;
        NSString *actionType = action[@"actionType"];

        if ([actionType isEqual:@"ttr"]) {
            SECSFAActionTapToRadar *ttr = [[SECSFAActionTapToRadar alloc] init];
            if (radarNumber == nil) {
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
