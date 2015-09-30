//
//  CredentialTableController.h
//  GSSTestApp
//
//  Created by Love Hörnquist Åstrand on 2014-08-16.
//  Copyright (c) 2014 Apple, Inc. All rights reserved.
//

#import <UIKit/UIKit.h>

@protocol GSSCredentialsChangeNotification <NSObject>
- (void)GSSCredentialChangeNotifification;
@end

@interface CredentialTableController : NSObject <UITableViewDelegate, UITableViewDataSource>

+ (CredentialTableController *)getGlobalController;

- (void)addNotification:(id<GSSCredentialsChangeNotification>)object;
- (void)removeNotification:(id<GSSCredentialsChangeNotification>)object;

@end
