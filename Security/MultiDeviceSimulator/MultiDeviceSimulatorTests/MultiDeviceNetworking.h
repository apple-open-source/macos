//
//  MultiDeviceNetworking.h
//  Security
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

@interface MDNCounters : NSObject
//- (NSDictionary *)summary;
@end

@interface MultiDeviceNetworking : NSObject
- (instancetype)init;
- (NSXPCListenerEndpoint *)endpoint;
- (void)dumpKVSState;
- (void)dumpCounters;
- (void)disconnectAll;

- (void)setTestExpectation:(XCTestExpectation *)expectation forKey:(NSString *)key;
- (void)fulfill:(NSString *)key;
- (void)clearTestExpectations;

@end
