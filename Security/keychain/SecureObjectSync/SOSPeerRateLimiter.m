//
//  SOSPeerRateLimiter.m
//  SecureObjectSyncServer
//

#import <Foundation/Foundation.h>
#import <keychain/ckks/RateLimiter.h>
#import "keychain/SecureObjectSync/SOSPeerRateLimiter.h"

#include "keychain/SecureObjectSync/SOSPeer.h"
#include <utilities/SecCFError.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecCFWrappers.h>

//
// RateLimiting Code per Peer

@implementation PeerRateLimiter

@synthesize peerID = peerID;

-(NSDictionary*) setUpConfigForPeer
{
    NSData *configData = [@"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\
                          <!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\
                          <plist version=\"1.0\">\
                          <dict>\
                          <key>general</key>\
                          <dict>\
                          <key>maxStateSize</key>\
                          <integer>250</integer>\
                          <key>maxItemAge</key>\
                          <integer>3600</integer>\
                          <key>overloadDuration</key>\
                          <integer>1800</integer>\
                          <key>name</key>\
                          <string>SOS</string>\
                          <key>MAType</key>\
                          <string></string>\
                          </dict>\
                          <key>groups</key>\
                          <array>\
                                <dict>\
                                    <key>property</key>\
                                    <string>global</string>\
                                    <key>capacity</key>\
                                    <integer>1000</integer>\
                                    <key>rate</key>\
                                    <integer>10</integer>\
                                    <key>badness</key>\
                                    <integer>1</integer>\
                                </dict>\
                                <dict>\
                                    <key>property</key>\
                                    <string>accessGroup</string>\
                                    <key>capacity</key>\
                                    <integer>50</integer>\
                                    <key>rate</key>\
                                    <integer>900</integer>\
                                    <key>badness</key>\
                                    <integer>3</integer>\
                                </dict>\
                          </array>\
                          </dict>\
                          </plist>\
                          " dataUsingEncoding:NSUTF8StringEncoding];

    NSError *err = nil;
    return ([NSPropertyListSerialization propertyListWithData:configData options:NSPropertyListImmutable format:nil error:&err]);
}

-(instancetype)initWithPeer:(SOSPeerRef)peer
{
    self = [super initWithConfig:[self setUpConfigForPeer]];
    if(self){
        self.peerID = (__bridge NSString *)(SOSPeerGetID(peer));
        self.accessGroupRateLimitState = [[NSMutableDictionary alloc] init];
        self.accessGroupToTimer = [[NSMutableDictionary alloc]init];
        self.accessGroupToNextMessageToSend = [[NSMutableDictionary alloc]init];
    }
    return self;
}

-(enum RateLimitState) stateForAccessGroup:(NSString*) accessGroup
{
    enum RateLimitState stateForAccessGroup;
     NSNumber *state = [self.accessGroupRateLimitState objectForKey:accessGroup];
    if(state == nil)
    {
        //initialize access group state
        stateForAccessGroup = RateLimitStateCanSend;
        NSNumber *initialize = [[NSNumber alloc] initWithLong:stateForAccessGroup];
        [self.accessGroupRateLimitState setObject:initialize forKey:accessGroup];
    }else{
        stateForAccessGroup = [state intValue];
    }
    return stateForAccessGroup;
}
@end

@implementation KeychainItem

-(instancetype)initWithAccessGroup:(NSString *)accessGroup
{
    self = [super init];
    if(self){
        _accessGroup = accessGroup;
    }
    return self;
}

@end
