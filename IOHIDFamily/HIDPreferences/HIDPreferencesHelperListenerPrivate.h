//
//  HIDPreferencesHelperListenerPrivate.h
//  IOHIDFamily
//
//  Created by AB on 10/22/19.
//

#import <HIDPreferences/HIDPreferencesHelperListener.h>

@interface HIDPreferencesHelperListener (HIDPreferencesHelperListenerPrivate)

-(void) removeClient:(HIDPreferencesHelperClient*) client;
@end
