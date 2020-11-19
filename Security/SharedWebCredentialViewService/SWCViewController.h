//
//  SWCViewController.h
//  SharedWebCredentialViewService
//
//  Copyright (c) 2014 Apple Inc. All Rights Reserved.
//

#import <UIKit/UIKit.h>
#if !TARGET_OS_TV
#import <SpringBoardUIServices/SBSUIRemoteAlertItemContentViewController.h>
#endif

@interface SWCViewController : SBSUIRemoteAlertItemContentViewController <UITableViewDelegate, UITableViewDataSource>

@end
