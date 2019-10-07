#import "keychain/SecureObjectSync/SOSAccountPriv.h"
#include "keychain/SecureObjectSync/SOSTransport.h"

@implementation SOSCircleStorageTransport

@synthesize account = account;

-(id)init
{
    return [super init];

}
-(SOSCircleStorageTransport*) initWithAccount:(SOSAccount*)acct
{
    self = [super init];
    if(self){
        self.account = acct;
    }
    return self;
}

-(SOSAccount*)getAccount
{
    return self.account;
}

-(CFIndex)circleGetTypeID
{
    return kUnknown;
}
-(CFIndex)getTransportType
{
    return kUnknown;
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

-(bool) postRetirement:(CFStringRef) circleName peer:(SOSPeerInfoRef) peer err:(CFErrorRef *)error{
    return true;
}

-(CFDictionaryRef)handleRetirementMessages:(CFMutableDictionaryRef) circle_retirement_messages_table err:(CFErrorRef *)error
{
    return CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
}

-(CFArrayRef) handleCircleMessagesAndReturnHandledCopy:(CFMutableDictionaryRef) circle_circle_messages_table err:(CFErrorRef *)error
{
    return CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
}

@end

