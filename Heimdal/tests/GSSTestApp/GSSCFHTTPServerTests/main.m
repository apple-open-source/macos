//
//  GSSCFHTTPServerTests
//
//  Copyright © 2015 Apple, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <err.h>
#import "httpserver.h"

int
main(int argc, char **argv)
{
    if (getuid() != 0)
        errx(1, "need to run as root");

    GSSHTTPServer *httpServer = [[GSSHTTPServer alloc] init];

    [NSThread detachNewThreadSelector:@selector(start:) toTarget:httpServer withObject:@(4774)];

    sleep(1);

    dispatch_main();

    return 0;
}
