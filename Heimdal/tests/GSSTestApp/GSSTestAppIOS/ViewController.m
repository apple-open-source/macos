//
//  ViewController.m
//  GSSTestApp
//
//  Copyright (c) 2013 Apple, Inc. All rights reserved.
//


#include "ViewController.h"

#if !TARGET_OS_TV
#import <SafariServices/SafariServices.h>
#endif
#import "WebbyViewController.h"
#import "FakeXCTest.h"

#if !TARGET_OS_TV
@interface ViewController () <SFSafariViewControllerDelegate>
@property (strong) SFSafariViewController *safariViewController;
@end
#endif

@implementation ViewController

- (void)viewDidLoad
{
    [super viewDidLoad];
    self.credentialsTableController = [CredentialTableController getGlobalController];
    self.credentialsTableView.delegate = self.credentialsTableController;
    self.credentialsTableView.dataSource = self.credentialsTableController;
    self.credentialsTableView.allowsMultipleSelectionDuringEditing = NO;
}

- (void)viewDidAppear:(BOOL)animated {
    [self.credentialsTableController addNotification:self];
    [super viewDidAppear:animated];
}

- (void)viewDidDisappear:(BOOL)animated {
    [self.credentialsTableController removeNotification:self];
    [super viewDidDisappear:animated];
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations
{
    return UIInterfaceOrientationMaskPortrait;
}

- (void)GSSCredentialChangeNotifification {
    [self.credentialsTableView reloadData];
}

- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {

    NSString *name = [segue identifier];

    if ([name isEqualToString:@"UIWebView"] || [name isEqualToString:@"WKWebView"]) {
        WebbyViewController *vc = [segue destinationViewController];
        vc.type = [segue identifier];
    }
}

- (IBAction)safariViewController:(id)sender {
#if !TARGET_OS_TV
    self.safariViewController = [[SFSafariViewController alloc] initWithURL:[NSURL URLWithString:@"http://dc03.ads.apple.com/"]];

    [self presentViewController:self.safariViewController animated:YES completion:^{
        NSLog(@"presented");
    }];
#endif
}

@end
