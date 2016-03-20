//
//  RegressionTestsViewController.m
//  GSSTestApp
//
//  Created by Love Hörnquist Åstrand on 2014-08-15.
//  Copyright (c) 2014 Apple, Inc. All rights reserved.
//

#import "RegressionTestsViewController.h"
#import "FakeXCTest.h"

@interface RegressionTestsViewController ()
@property (strong) dispatch_queue_t queue;

@end

static RegressionTestsViewController *me = NULL;

static int
callback(const char *fmt, va_list ap)
{
    if (me == NULL)
        return -1;

    char *output = NULL;

    vasprintf(&output, fmt, ap);

    dispatch_async(dispatch_get_main_queue(), ^{
        [me appendProgress:[NSString stringWithUTF8String:output] color:NULL];
        free(output);
    });

    return 0;
}

@implementation RegressionTestsViewController

- (void)viewDidLoad
{
    [super viewDidLoad];

    me = self;
    XFakeXCTestCallback = callback;

    self.queue = dispatch_queue_create("test-queue", NULL);

    [self runTests:self];
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations
{
    return UIInterfaceOrientationMaskPortrait;
}

- (IBAction)runTests:(id)sender
{
    [self.statusLabel setText:@"running"];
    [self.progressTextView setText:@""];

    dispatch_async(self.queue, ^{
        NSString *testStatus;
        int result = [XCTest runTests];

        if (result == 0)
            testStatus = @"all tests passed";
        else
            testStatus = [NSString stringWithFormat:@"%d tests failed", result];

        dispatch_async(dispatch_get_main_queue(), ^{
            [self.statusLabel setText:testStatus];
        });
    });
}

- (void)appendProgress:(NSString *)string color:(UIColor *)color {

    NSMutableAttributedString* str = [[NSMutableAttributedString alloc] initWithString:string];
    if (color)
        [str addAttribute:NSForegroundColorAttributeName value:color range:NSMakeRange(0, [str length])];

    NSTextStorage *textStorage = [self.progressTextView textStorage];

    [textStorage beginEditing];
    [textStorage appendAttributedString:str];
    [textStorage endEditing];
}

@end
