//
//  FirstViewController.h
//  GSSEmbeddedSample
//
//  Created by Love Hörnquist Åstrand on 2011-03-15.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import <UIKit/UIKit.h>


@interface CredentialsViewController : UIViewController {
    dispatch_queue_t queue;
}
- (IBAction)addCredential:(id)sender;

@end
