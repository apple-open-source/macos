//
//  WebbyViewController.h
//  GSSTestApp
//
//  Copyright (c) 2014 Apple, Inc. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

@interface WebbyViewController : UIViewController <UITextFieldDelegate, WKNavigationDelegate>
@property (weak, nonatomic) IBOutlet UITextField *urlTextField;
@property (weak, nonatomic) IBOutlet UIView *webbyView;

@property (strong, nonatomic) NSString *type;
@property (strong, nonatomic) WKWebView *wk;
@property (weak, nonatomic) IBOutlet UIActivityIndicatorView *spinny;
@property (weak, nonatomic) IBOutlet UIButton *reloadButton;

@end
