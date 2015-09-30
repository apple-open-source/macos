//
//  DumpCacheViewController.m
//  GSSTestApp
//
//  Created by Love Hörnquist Åstrand on 2014-09-03.
//  Copyright (c) 2014 Apple, Inc. All rights reserved.
//

#import "DumpCacheViewController.h"
#include <Heimdal/heimcred.h>

@interface DumpCacheViewController ()

@end

@implementation DumpCacheViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view.
    [self dumpCredentials:(id)self];
}

- (IBAction)dumpCredentials:(id)sender {

    CFDictionaryRef status = HeimCredCopyStatus(NULL);
    if (status) {
        CFDataRef data = CFPropertyListCreateData(NULL, status, kCFPropertyListXMLFormat_v1_0, 0, NULL);
        CFRelease(status);
        if (data == NULL) {
            [self.dumpCacheTextView setText:@"failed to convert dictionary to a plist"];
        }
        NSString *string = [[NSString alloc] initWithData:(__bridge NSData *)data encoding:NSUTF8StringEncoding];

        [self.dumpCacheTextView setText:string];
        CFRelease(data);
    } else {
        [self.dumpCacheTextView setText:@"no credentials to dump\n"];
    }
}

@end
