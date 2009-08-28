//
//  JABLogger.h
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 7/16/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>

#include <syslog.h>

@interface JABLogger : NSObject {

	NSFileHandle *_outFile;
	NSFileHandle *_errFile;

	NSInteger _verboseFlag; // 1 = enable verbose/detailed output
}
@property(retain,readwrite) NSFileHandle *outFile;
@property(retain,readwrite) NSFileHandle *errFile;
@property(assign,readwrite) NSInteger verboseFlag;

+ (id) jabLogger;
+ (id) jabLoggerWithCommandOptions: (NSDictionary *) cmdOpts;

- (id) initWithCommandOptions: (NSDictionary *) cmdOpts;

- (void) logInfo: (NSString *) aMsg;

- (void) logMsgWithLevel: (int) level format: (NSString *) msgFmt, ...;

- (void) logStdOutMessage: (NSString *) aMsg;
- (void) logStdErrMessage: (NSString *) aMsg;

@end
