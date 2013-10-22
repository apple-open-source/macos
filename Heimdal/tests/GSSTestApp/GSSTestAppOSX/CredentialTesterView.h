//
//  CredentialTesterView.h
//  GSSTestApp
//
//  Created by Love Hörnquist Åstrand on 2013-07-03.
//  Copyright (c) 2013 Apple, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <GSS/GSS.h>

@interface CredentialTesterView : NSViewController

@property (retain) NSString *name;
@property (retain) NSDate *expire;

@property (assign) IBOutlet NSTextField *serverName;
@property (assign) IBOutlet NSTextField *iscStatus;

@property (assign) NSTabViewItem *tabViewItem;


- (id)initWithGSSCredential:(gss_cred_id_t)credential;

@end
