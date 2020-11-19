//
//  HIDPreferencesHelperProtocol.h
//  HIDPreferencesHelper
//
//  Created by AB on 10/15/19.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef void(^HIDXPCData)(id __nullable data);

@protocol HIDPreferencesProtocol <NSObject>

-(void) set:(NSString*) key value:(id __nullable) value user:(NSString*) user host:(NSString*) host domain:(NSString*) domain;

-(void) copy:(NSString*) key user:(NSString*) user host:(NSString*) host domain:(NSString*) domain reply:(HIDXPCData __nullable) reply;

-(void) synchronize:(NSString*) user host:(NSString*) host domain:(NSString*) domain;

-(void) copyMultiple:(NSArray* __nullable) keys user:(NSString*) user host:(NSString*) host domain:(NSString*) domain reply:(HIDXPCData __nullable) reply;

-(void) setMultiple:(NSDictionary* __nullable) keysToSet keysToRemove:(NSArray* __nullable) keysToRemove user:(NSString*)user host:(NSString*) host domain:(NSString*) domain;

-(void) copyDomain:(NSString*) key domain:(NSString*) domain reply:(HIDXPCData __nullable) reply;

-(void) setDomain:(NSString*) key value:(id __nullable) value domain:(NSString*) domain;

@end

NS_ASSUME_NONNULL_END
