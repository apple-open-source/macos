//
//  GSSEmbeddedSampleAppDelegate.h
//  GSSEmbeddedSample
//
//  Created by Love Hörnquist Åstrand on 2011-03-15.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface GSSEmbeddedSampleAppDelegate : NSObject <UIApplicationDelegate, UITabBarControllerDelegate> {

}

@property (nonatomic, retain) IBOutlet UIWindow *window;

@property (nonatomic, retain) IBOutlet UITabBarController *tabBarController;

@end
