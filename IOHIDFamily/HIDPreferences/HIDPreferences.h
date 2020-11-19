//
//  HIDPreferences.h
//  HIDPreferences
//
//  Created by AB on 10/7/19.
//



#import <Foundation/Foundation.h>

FOUNDATION_EXPORT double HIDPreferencesVersionNumber;
FOUNDATION_EXPORT const unsigned char HIDPreferencesVersionString[];

#import <HIDPreferences/HIDPreferencesCAPI.h>
#import <HIDPreferences/HIDPreferencesProtocol.h>

NS_ASSUME_NONNULL_BEGIN


@interface HIDPreferences : NSObject <HIDPreferencesProtocol>
@end


NS_ASSUME_NONNULL_END
