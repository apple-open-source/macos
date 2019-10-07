//
//  SOSAccountTrust.c
//  Security
//
#import "keychain/SecureObjectSync/SOSAccountPriv.h"
#import "keychain/SecureObjectSync/SOSAccountTrust.h"

@implementation SOSAccountTrust
@synthesize cachedOctagonEncryptionKey = _cachedOctagonEncryptionKey;
@synthesize cachedOctagonSigningKey = _cachedOctagonSigningKey;

+(instancetype)trust
{
    return [[SOSAccountTrust alloc]init];
}

-(id)init
{
    self = [super init];
    if(self)
    {
        self.retirees = [NSMutableSet set];
        self.fullPeerInfo = NULL;
        self.trustedCircle = NULL;
        self.departureCode = kSOSDepartureReasonError;
        self.expansion = [NSMutableDictionary dictionary];
    }
    return self;
}

-(id)initWithRetirees:(NSMutableSet*)r fpi:(SOSFullPeerInfoRef)fpi circle:(SOSCircleRef) trusted_circle
        departureCode:(enum DepartureReason)code peerExpansion:(NSMutableDictionary*)e
{

    self = [super init];
    if(self)
    {
        self.retirees = r;
        self.fullPeerInfo = fpi;
        self.trustedCircle = trusted_circle;
        self.departureCode = code;
        self.expansion = e;
    }
    return self;
}
- (void)dealloc {
    if(self) {
        CFReleaseNull(self->fullPeerInfo);
        CFReleaseNull(self->peerInfo);
        CFReleaseNull(self->trustedCircle);
        CFReleaseNull(self->_cachedOctagonSigningKey);
        CFReleaseNull(self->_cachedOctagonEncryptionKey);
    }
}

- (SOSPeerInfoRef) peerInfo {
    return SOSFullPeerInfoGetPeerInfo(self.fullPeerInfo);
}

- (NSString*) peerID {
    return (__bridge_transfer NSString*) CFRetainSafe(SOSPeerInfoGetPeerID(self.peerInfo));
}

@synthesize trustedCircle = trustedCircle;

- (void) setTrustedCircle:(SOSCircleRef) circle {
    CFRetainAssign(self->trustedCircle, circle);
}

@synthesize retirees = retirees;

-(void) setRetirees:(NSSet *)newRetirees
{
    self->retirees = newRetirees.mutableCopy;
}

@synthesize fullPeerInfo = fullPeerInfo;

- (void) setFullPeerInfo:(SOSFullPeerInfoRef) newIdentity {
    CFRetainAssign(self->fullPeerInfo, newIdentity);
}

@synthesize expansion = expansion;

-(void)setExpansion:(NSDictionary*) newExpansion
{
    self->expansion = newExpansion.mutableCopy;
}

@synthesize departureCode = departureCode;

-(void)setDepartureCode:(enum DepartureReason)newDepartureCode
{
    self->departureCode = newDepartureCode;
}

@end

