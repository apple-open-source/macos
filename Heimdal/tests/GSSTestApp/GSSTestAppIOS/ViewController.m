//
//  ViewController.m
//  GSSTestApp
//
//  Created by Love Hörnquist Åstrand on 2013-06-07.
//  Copyright (c) 2013 Apple, Inc. All rights reserved.
//

#import "ViewController.h"
#import "WebbyViewController.h"
#import "FakeXCTest.h"

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

- (NSUInteger)supportedInterfaceOrientations
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

@end
