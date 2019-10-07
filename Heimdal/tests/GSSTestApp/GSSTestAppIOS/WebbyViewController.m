
//
//  WebbyViewController.m
//  GSSTestApp
//
//  Copyright (c) 2014 Apple, Inc. All rights reserved.
//

#import "WebbyViewController.h"

@interface WebbyViewController ()

@end

@implementation WebbyViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    [self.spinny setHidden:NO];

    self.urlTextField.text = @"http://dc03.ads.apple.com/";
    self.urlTextField.delegate = self;

    if ([self.type isEqualToString:@"WKWebView"]) {
        self.wk = [[WKWebView alloc] initWithFrame:[self.webbyView bounds]];
        self.wk.navigationDelegate = self;

        [self.webbyView addSubview:self.wk];
    } else {
        abort();
    }

    [self gotoURL:self.urlTextField.text];
}

- (IBAction)reload:(id)sender {
    if (self.wk)
        [self.wk reload];
}

- (IBAction)goBack:(id)sender {
    if (self.wk)
        [self.wk goBack];
}

- (void)startLoading {
    [self.spinny startAnimating];
    [self.reloadButton setHidden:YES];
    [self.spinny setHidden:NO];
}

- (void)doneLoading {
    [self.spinny stopAnimating];
    [self.reloadButton setHidden:NO];
    [self.spinny setHidden:YES];
}

#pragma mark - WKWebView

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation
{
    [self startLoading];
    self.urlTextField.text = [[self.wk URL] absoluteString];
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    [self doneLoading];
}

- (void)webView:(WKWebView *)webView didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    [self doneLoading];
}

- (void)gotoURL:(NSString *)url {
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:url]];

    if (self.wk)
        [self.wk loadRequest:request];
}

- (BOOL) textFieldShouldReturn:(UITextField *)textField{

    [textField resignFirstResponder];
    [self gotoURL:textField.text];
    return YES;
}

@end
