//
//  httpserver.h
//  GSSTestApp
//
//  Copyright © 2015 Apple, Inc. All rights reserved.
//

#ifndef httpserver_h
#define httpserver_h

@interface GSSHTTPServer : NSObject 
- (void)start:(NSNumber *)servicePort;
@end

#endif /* httpserver_h */
