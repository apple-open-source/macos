//
//  ViewController.m
//  GSSTestApp
//
//  Created by Love Hörnquist Åstrand on 2013-06-07.
//  Copyright (c) 2013 Apple, Inc. All rights reserved.
//

#import "ViewController.h"


@interface ViewController ()
@property (strong) TestHarness *tests;
@property (strong) dispatch_queue_t queue;
@end

@implementation ViewController

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    self.tests = [[TestHarness alloc] init];
    self.tests.delegate = self;
    self.queue = dispatch_queue_create("test-queue", NULL);
    
    [self runTests:self];
}

- (NSUInteger)supportedInterfaceOrientations
{
    return UIInterfaceOrientationMaskPortrait;
}

- (IBAction)runTests:(id)sender
{
    [self.statusLabel setText:@"running"];
    [self.progressTextView setText:@""];
    
    dispatch_async(self.queue, ^{
	[self.tests runTests];
	dispatch_async(dispatch_get_main_queue(), ^{
	    [self.statusLabel setText:@""];
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

- (void)THPTestStart:(NSString *)name
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [self appendProgress:[NSString stringWithFormat:@"[TEST] %@\n",name] color:[UIColor purpleColor]];
    });
}
- (void)THPTestOutput:(NSString *)output
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [self appendProgress:[NSString stringWithFormat:@"%@\n", output] color:NULL];
    });

}
- (void)THPTestComplete:(NSString *)name status:(bool)status duration:(float)durataion
{
    dispatch_async(dispatch_get_main_queue(), ^{
        NSString *grade = status ? @"PASS" : @"FAIL";
        UIColor *color = status ? [UIColor greenColor] : [UIColor redColor];
        [self appendProgress:[NSString stringWithFormat:@"duration: %f\n", durataion] color:NULL];
        [self appendProgress:[NSString stringWithFormat:@"[%@] %@\n", grade, name] color:color];
    });

}
- (void)THPSuiteComplete:(bool)status
{
    dispatch_async(dispatch_get_main_queue(), ^{
        UIColor *color = status ? [UIColor greenColor] : [UIColor redColor];
        [self appendProgress:[NSString stringWithFormat:@"test %s\n", status ? "pass" : "fail"]  color:color];
    });
}



@end
