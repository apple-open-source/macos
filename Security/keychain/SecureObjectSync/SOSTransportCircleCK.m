//
//  SOSTransportCircleCK.m
//  Security
//
//  Created by Michelle Auricchio on 12/23/16.
//
//

#import <Foundation/Foundation.h>
#import "keychain/SecureObjectSync/SOSTransport.h"
#import "keychain/SecureObjectSync/SOSAccountPriv.h"
#import "SOSTransportCircleCK.h"

@implementation SOSCKCircleStorage

-(id) init
{
    self = [super init];
    if(self){
        SOSRegisterTransportCircle(self);
    }
    return self;
}

-(id) initWithAccount:(SOSAccount*)acct
{
    self = [super init];
    if(self)
    {
        self.account = acct;
    }
    return self;
}

-(CFIndex) getTransportType
{
    return kCK;
}
-(SOSAccount*) getAccount
{
    return self.account;
}

-(bool) expireRetirementRecords:(CFDictionaryRef) retirements err:(CFErrorRef *)error
{
    return true;
}

-(bool) flushChanges:(CFErrorRef *)error
{
    return true;
}
-(bool) postCircle:(CFStringRef)circleName circleData:(CFDataRef)circle_data err:(CFErrorRef *)error
{
    return true;
}
-(bool) postRetirement:(CFStringRef) circleName peer:(SOSPeerInfoRef) peer err:(CFErrorRef *)error
{
    return true;
}

-(CFDictionaryRef)handleRetirementMessages:(CFMutableDictionaryRef) circle_retirement_messages_table err:(CFErrorRef *)error
{
    return NULL;
}
-(CFArrayRef)CF_RETURNS_RETAINED handleCircleMessagesAndReturnHandledCopy:(CFMutableDictionaryRef) circle_circle_messages_table err:(CFErrorRef *)error
{
    return NULL;
}

@end
