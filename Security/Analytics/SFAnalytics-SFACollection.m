//
//  SFAnalytics-SFACollection.m
//  Security
//

#import <Foundation/Foundation.h>

#import "SFAnalytics+Internal.h"
#import "SFAnalyticsCollection.h"

#import "Analytics/Protobuf/SECSFARules.h"
#import "Analytics/Protobuf/SECSFAEventRule.h"
#import "Analytics/Protobuf/SECSFAAction.h"
#import "Analytics/Protobuf/SECSFAActionAutomaticBugCapture.h"
#import "Analytics/Protobuf/SECSFAActionTapToRadar.h"
#import "Analytics/Protobuf/SECSFAActionDropEvent.h"
#import "Analytics/Protobuf/SECSFAVersionMatch.h"
#import "Analytics/Protobuf/SECSFAEventFilter.h"
#import "Analytics/Protobuf/SECSFAVersion.h"

NSString *kSecSFAErrorDomain = @"com.apple.SFAErrorDomain";

@implementation SFAnalytics (SFACollection)

+ (NSError *)errorWithCode:(NSInteger)code
               description:(NSString*)description
{
    
    return [NSError errorWithDomain:kSecSFAErrorDomain code:code userInfo:@{
        NSLocalizedDescriptionKey: description,
    }];
}

+ (NSError *)errorWithCode:(NSInteger)code
               description:(NSString*_Nullable)description
                underlying:(NSError*_Nullable)underlyingError
{
    NSMutableDictionary *d = [NSMutableDictionary dictionary];
    d[NSLocalizedDescriptionKey] = description;
    d[NSUnderlyingErrorKey] = underlyingError;
    return [NSError errorWithDomain:kSecSFAErrorDomain code:code userInfo:d];
}

+ (SECSFAAction * _Nullable)parseAction:(NSDictionary *)action error:(NSError **)error {
    
    SECSFAAction *a = [[SECSFAAction alloc] init];

    if (![action isKindOfClass:[NSDictionary class]]) {
        if (error) {
            NSError *e = [self errorWithCode:kSecSFAErrorActionInvalidType description:@"action invalid type"];
            *error = e;
        }
        return nil;
    }

    NSString *radarNumber = action[@"radarNumber"];
    if (radarNumber != nil && ![radarNumber isKindOfClass:[NSString class]]) {
        if (error) {
            NSError *e = [self errorWithCode:kSecSFAErrorRadarInvalidType description:@"radarNumber invalid"];
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
                NSError *e = [self errorWithCode:kSecSFAErrorTTRAttributeInvalidType description:@"attribute invalid type"];
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
                NSError *e = [self errorWithCode:kSecSFAErrorABCAttributeInvalidType description:@"abc invalid type"];
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
            NSError *e = [self errorWithCode:kSecSFAErrorUnknownAction description:str];
            *error = e;
        }
        return nil;
    }

    return a;
}

+ (BOOL)requiredVersion:(SECSFAConfigVersion)requiredVersion
                  rules:(SECSFARules *)sfaRules
                 reason:(NSString *)reason
                  error:(NSError **)error {
    if (sfaRules.configVersion < requiredVersion) {
        if (error) {
            NSError *e = [self errorWithCode:kSecSFAErrorSecondInvalid
                                 description:[NSString stringWithFormat:@"rules config format version %d because %@",
                                              requiredVersion, reason]];
            *error = e;
        }
        return NO;
    }
    return YES;
}


+ (BOOL)parseRules:(NSArray *)rules format:(SECSFARules *)sfaRules error:(NSError **)error {
    if (![rules isKindOfClass:[NSArray class]]) {
        if (error) {
            NSError *e = [self errorWithCode:kSecSFAErrorsRulesMissing description:@"rules key missing"];
            *error = e;
        }
        return NO;
    }
    for (NSDictionary *item in rules) {
        SECSFAEventClass eventClassInteger = SECSFAEventClass_Errors;
        
        if (![item isKindOfClass:[NSDictionary class]]) {
            NSError *e = [self errorWithCode:kSecSFAErrorRulesInvalidType description:@"rules type invalid"];
            if (error) {
                *error = e;
            }
            return NO;
        }

        NSString *eventType = item[@"eventType"];
        if (![eventType isKindOfClass:[NSString class]]) {
            if (error) {
                NSError *e = [self errorWithCode:kSecSFAErrorTypeMissing description:@"eventType missing"];
                *error = e;
            }
            return NO;
        }
        NSString *eventClass = item[@"eventClass"];
        if (eventClass != nil) {
            if (![eventClass isKindOfClass:[NSString class]]) {
                if (error) {
                    NSError *e = [self errorWithCode:kSecSFAErrorTypeMissing description:@"eventType not a string"];
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
                    NSError *e = [self errorWithCode:kSecSFAErrorTypeMissing
                                         description:[NSString stringWithFormat:@"unknown eventclass: %@", eventClass]];
                    *error = e;
                }
                return NO;
            }
        }
        NSDictionary *match = item[@"match"];
        
        if (![match isKindOfClass:[NSDictionary class]]) {
            NSError *e = [self errorWithCode:kSecSFAErrorMatchMissing description:@"match missing"];
            if (error) {
                *error = e;
            }
            return NO;
        }
        NSNumber *repeatAfterSeconds = item[@"repeatAfterSeconds"];
        if (repeatAfterSeconds != nil && ![repeatAfterSeconds isKindOfClass:[NSNumber class]]) {
            if (error) {
                NSError *e = [self errorWithCode:kSecSFAErrorSecondInvalid description:@"repeatAfterSeconds not number"];
                *error = e;
            }
            return NO;
        }
        NSString *processName = item[@"processName"];
        if (processName != nil && ![processName isKindOfClass:[NSString class]]) {
            if (error) {
                NSError *e = [self errorWithCode:kSecSFAErrorSecondInvalid description:@"processName not string"];
                *error = e;
            }
            return NO;
        }
        
        NSNumber *matchOnFirstFailure = item[@"matchOnFirstFailure"];
        if (matchOnFirstFailure != nil && ![matchOnFirstFailure isKindOfClass:[NSNumber class]]) {
            if (error) {
                NSError *e = [self errorWithCode:kSecSFAErrorSecondInvalid description:@"matchOnFirstFailure not number"];
                *error = e;
            }
            return NO;
        }
        
        NSArray *versionMatch = item[@"versions"];
        SECSFAVersionMatch *ruleVersions = nil;
        if ([versionMatch isKindOfClass:[NSArray class]]) {
            if (![self requiredVersion:SECSFAConfigVersion_version2 rules:sfaRules reason:@"versions on rule" error:error]) {
                return NO;
            }
            
            NSError *versionError = nil;
            ruleVersions = [[self class] parseVersions:versionMatch error:&versionError];
            if (ruleVersions == nil) {
                if (error) {
                    *error = versionError;
                }
                return NO;
            }
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
        rule.versions = ruleVersions;
        rule.match = [NSPropertyListSerialization dataWithPropertyList:match
                                                                format:NSPropertyListBinaryFormat_v1_0
                                                               options:0
                                                                 error:&matchError];
        if (rule.match == nil) {
            if (error) {
                *error = [self errorWithCode:kSecSFAErrorFailedToEncodeMatchStructure
                                 description:@"plist encode failed"
                                  underlying:matchError];
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

+ (SECSFAVersionMatch *)parseVersions:(NSArray *)versions error:(NSError **)error {
    if (![versions isKindOfClass:[NSArray class]]) {
        if (error) {
            NSError *e = [self errorWithCode:kSecSFAErrorsRulesMissing description:@"versions key missing"];
            *error = e;
        }
        return nil;
    }
    SECSFAVersionMatch *builds = [[SECSFAVersionMatch alloc] init];
    for (NSDictionary *item in versions) {
        if (![item isKindOfClass:[NSDictionary class]]) {
            NSError *e = [self errorWithCode:kSecSFAErrorRulesInvalidType description:@"versions type invalid"];
            if (error) {
                *error = e;
            }
            return nil;
        }
        NSString *version = item[@"version"];
        NSString *platform = item[@"platform"];
        if (![version isKindOfClass:[NSString class]] || ![platform isKindOfClass:[NSString class]]) {
            NSError *e = [self errorWithCode:kSecSFAErrorRulesInvalidType
                                 description:[NSString stringWithFormat:@"versions is string: %@", item]];
            if (error) {
                *error = e;
            }
            return nil;
        }

        SECSFAVersion *v = [SFAnalyticsCollection parseVersion:version platform:platform];
        if (v == nil) {
            NSError *e = [self errorWithCode:kSecSFAErrorRulesInvalidType
                                 description:[NSString stringWithFormat:@"versions not parsing: %@", item]];
            if (error) {
                *error = e;
            }
            return nil;
        }

        [builds addVersions:v];
    }
    
    return builds;
}

+ (BOOL)parseEventFilter:(NSDictionary *)events format:(SECSFARules *)sfaRules error:(NSError **)error {
    if (![events isKindOfClass:[NSDictionary class]]) {
        if (error) {
            NSError *e = [self errorWithCode:kSecSFAErrorsRulesMissing description:@"events key missing"];
            *error = e;
        }
        return NO;
    }
    __block NSError *e = nil;
    [events enumerateKeysAndObjectsUsingBlock:^(NSString* _Nonnull key, NSNumber* _Nonnull number, BOOL * _Nonnull stop) {
        if (![key isKindOfClass:[NSString class]] || ![number isKindOfClass:[NSNumber class]]) {
            e = [self errorWithCode:kSecSFAErrorRulesInvalidType description:@"events type invalid"];
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

static SECSFAConfigVersion currentVersion = SECSFAConfigVersion_version2;

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
    NSNumber *configVersion = sfaCollection[@"configVersion"];
    if (![configVersion isKindOfClass:[NSNumber class]]) {
        NSError *e = [self errorWithCode:kSecSFAErrorVersionMissing description:@"configVersion missing"];
        if (error) {
            *error = e;
        }
        return nil;
    }
    if ([configVersion intValue] > currentVersion) {
        NSString *desc = [NSString stringWithFormat:@"configVersion not understood %@, this tool knows about %d", configVersion, currentVersion];
        NSError *e = [self errorWithCode:kSecSFAErrorVersionMismatch description:desc];
        if (error) {
            *error = e;
        }
        return nil;
    }

    sfaRules.configVersion = [configVersion intValue];

    if (![self requiredVersion:SECSFAConfigVersion_version1 rules:sfaRules reason:@"base version" error:error]) {
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
        sfaRules.allowedBuilds = [self parseVersions:versions error:error];
        if (sfaRules.allowedBuilds == nil) {
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
