//
//  HIDPreferencesPrivate.h
//  IOHIDFamily
//
//  Created by AB on 10/9/19.
//

#pragma once

#import <Foundation/Foundation.h>

#define kHIDXPCTimeoutInSec 5
#define kHIDPreferencesServiceName "com.apple.hidpreferenceshelper"
#define kHIDPreferencesEntitlements "com.apple.hidpreferences.privileged"


typedef NS_ENUM(NSUInteger, HIDPreferencesRequestType) {
    kHIDPreferencesRequestTypeNone = 0,
    kHIDPreferencesRequestTypeSet,
    kHIDPreferencesRequestTypeSetMultiple,
    kHIDPreferencesRequestTypeCopy,
    kHIDPreferencesRequestTypeCopyMultiple,
    kHIDPreferencesRequestTypeSynchronize,
    kHIDPreferencesRequestTypeCopyDomain,
    kHIDPreferencesRequestTypeSetDomain,
    
};

#define kHIDPreferencesRequestType "Type"
#define kHIDPreferencesHost        "Host"
#define kHIDPreferencesUser        "User"
#define kHIDPreferencesDomain      "Domain"
#define kHIDPreferencesKey         "Key"
#define kHIDPreferencesValue       "Value"

#define kHIDPreferencesKeysToCopy   "KeysToCopy"
#define kHIDPreferencesKeysToSet    "KeysToSet"
#define kHIDPreferencesKeysToRemove "KeysToRemove"
