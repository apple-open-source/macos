#import <Foundation/Foundation.h>

#if OCTAGON
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/proto/generated_source/OTEscrowMoveRequestContext.h"
#endif

@implementation TrustedPeersHelperHealthCheckResult
- (instancetype)initWithPostRepairCFU:(bool)postRepairCFU
                        postEscrowCFU:(bool)postEscrowCFU
                         resetOctagon:(bool)resetOctagon
                           leaveTrust:(bool)leaveTrust
                               reroll:(bool)reroll
                          moveRequest:(OTEscrowMoveRequestContext* _Nullable)moveRequest
                   totalEscrowRecords:(uint64_t)totalEscrowRecords
             collectableEscrowRecords:(uint64_t)collectableEscrowRecords
               collectedEscrowRecords:(uint64_t)collectedEscrowRecords
 escrowRecordGarbageCollectionEnabled:(bool)escrowRecordGarbageCollectionEnabled
                       totalTlkShares:(uint64_t)totalTlkShares
                 collectableTlkShares:(uint64_t)collectableTlkShares
                   collectedTlkShares:(uint64_t)collectedTlkShares
     tlkShareGarbageCollectionEnabled:(bool)tlkShareGarbageCollectionEnabled
                           totalPeers:(uint64_t)totalPeers
                         trustedPeers:(uint64_t)trustedPeers
                     superfluousPeers:(uint64_t)superfluousPeers
                       peersCleanedup:(uint64_t)peersCleanedup
       superfluousPeersCleanupEnabled:(bool)superfluousPeersCleanupEnabled
{
    if ((self = [super init])) {
        _postRepairCFU = postRepairCFU;
        _postEscrowCFU = postEscrowCFU;
        _resetOctagon = resetOctagon;
        _leaveTrust = leaveTrust;
        _reroll = reroll;
        _moveRequest = moveRequest;
        _totalEscrowRecords = totalEscrowRecords;
        _collectableEscrowRecords = collectableEscrowRecords;
        _collectedEscrowRecords = collectedEscrowRecords;
        _escrowRecordGarbageCollectionEnabled = escrowRecordGarbageCollectionEnabled;
        _totalTlkShares = totalTlkShares;
        _collectableTlkShares = collectableTlkShares;
        _collectedTlkShares = collectedTlkShares;
        _tlkShareGarbageCollectionEnabled = tlkShareGarbageCollectionEnabled;
        _totalPeers = totalPeers;
        _trustedPeers = trustedPeers;
        _superfluousPeers = superfluousPeers;
        _peersCleanedup = peersCleanedup;
        _superfluousPeersCleanupEnabled = superfluousPeersCleanupEnabled;
    }
    return self;
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"<HealthCheckResult:"
                      " postRepairCFU: %@,"
                      " postEscrowCFU: %@,"
                       " resetOctagon: %@,"
                         " leaveTrust: %@,"
                             " reroll: %@,"
                     " moveRequest? %@,"
                 " totalEscrowRecords: %" PRIu64 ","
                     " collectableEscrowRecords: %" PRIu64 ","
             " collectedEscrowRecords: %" PRIu64 ","
                     " escrowRecordGarbageCollectionEnabled: %@,"
                     " totalTlkShares: %" PRIu64 ","
               " collectableTlkShares: %" PRIu64 ","
                 " collectedTlkShares: %" PRIu64 ","
                     " tlkShareGarbageCollectionEnabled: %@,"
                         " totalPeers: %" PRIu64 ","
                       " trustedPeers: %" PRIu64 ","
                   " superfluousPeers: %" PRIu64 ","
                     " peersCleanedup: %" PRIu64 ","
                     " superfluousPeersCleanupEnabled: %@>",
                     self.postRepairCFU ? @"true" : @"false",
                     self.postEscrowCFU ? @"true" : @"false",
                     self.resetOctagon ? @"true" : @"false",
                     self.leaveTrust ? @"true" : @"false",
                     self.reroll ? @"true" : @"false",
                     self.moveRequest,
                     self.totalEscrowRecords,
                     self.collectableEscrowRecords,
                     self.collectedEscrowRecords,
                     self.escrowRecordGarbageCollectionEnabled ? @"true" : @"false",
                     self.totalTlkShares,
                     self.collectableTlkShares,
                     self.collectedTlkShares,
                     self.tlkShareGarbageCollectionEnabled ? @"true" : @"false",
                     self.totalPeers,
                     self.trustedPeers,
                     self.superfluousPeers,
                     self.peersCleanedup,
                     self.superfluousPeersCleanupEnabled ? @"true" : @"false"
        ];
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super init])) {
        _postRepairCFU = [coder decodeBoolForKey:@"postRepairCFU"];
        _postEscrowCFU = [coder decodeBoolForKey:@"postEscrowCFU"];
        _resetOctagon = [coder decodeBoolForKey:@"resetOctagon"];
        _leaveTrust = [coder decodeBoolForKey:@"leaveTrust"];
        _reroll = [coder decodeBoolForKey:@"reroll"];
        _moveRequest = [coder decodeObjectOfClass:[OTEscrowMoveRequestContext class] forKey:@"moveRequest"];
        _totalEscrowRecords = [coder decodeInt64ForKey:@"totalEscrowRecords"];
        _collectableEscrowRecords = [coder decodeInt64ForKey:@"collectableEscrowRecords"];
        _collectedEscrowRecords = [coder decodeInt64ForKey:@"collectedEscrowRecords"];
        _escrowRecordGarbageCollectionEnabled = [coder decodeBoolForKey:@"escrowRecordGarbageCollectionEnabled"];
        _totalTlkShares = [coder decodeInt64ForKey:@"totalTlkShares"];
        _collectableTlkShares = [coder decodeInt64ForKey:@"collectableTlkShares"];
        _collectedTlkShares = [coder decodeInt64ForKey:@"collectedTlkShares"];
        _tlkShareGarbageCollectionEnabled = [coder decodeBoolForKey:@"tlkShareGarbageCollectionEnabled"];
        _totalPeers = [coder decodeInt64ForKey:@"totalPeers"];
        _trustedPeers = [coder decodeInt64ForKey:@"trustedPeers"];
        _superfluousPeers = [coder decodeInt64ForKey:@"superfluousPeers"];
        _peersCleanedup = [coder decodeInt64ForKey:@"peersCleanedup"];
        _superfluousPeersCleanupEnabled = [coder decodeBoolForKey:@"superfluousPeersCleanupEnabled"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeBool:self.postRepairCFU forKey:@"postRepairCFU"];
    [coder encodeBool:self.postEscrowCFU forKey:@"postEscrowCFU"];
    [coder encodeBool:self.resetOctagon forKey:@"resetOctagon"];
    [coder encodeBool:self.leaveTrust forKey:@"leaveTrust"];
    [coder encodeBool:self.reroll forKey:@"reroll"];
    [coder encodeObject:self.moveRequest forKey:@"moveRequest"];
    [coder encodeInt64:self.totalEscrowRecords forKey:@"totalEscrowRecords"];
    [coder encodeInt64:self.collectableEscrowRecords forKey:@"collectableEscrowRecords"];
    [coder encodeInt64:self.collectedEscrowRecords forKey:@"collectedEscrowRecords"];
    [coder encodeBool:self.escrowRecordGarbageCollectionEnabled forKey:@"escrowRecordGarbageCollectionEnabled"];
    [coder encodeInt64:self.totalTlkShares forKey:@"totalTlkShares"];
    [coder encodeInt64:self.collectableTlkShares forKey:@"collectableTlkShares"];
    [coder encodeInt64:self.collectedTlkShares forKey:@"collectedTlkShares"];
    [coder encodeBool:self.tlkShareGarbageCollectionEnabled forKey:@"tlkShareGarbageCollectionEnabled"];
    [coder encodeInt64:self.totalPeers forKey:@"totalPeers"];
    [coder encodeInt64:self.trustedPeers forKey:@"trustedPeers"];
    [coder encodeInt64:self.superfluousPeers forKey:@"superfluousPeers"];
    [coder encodeInt64:self.peersCleanedup forKey:@"peersCleanedup"];
    [coder encodeBool:self.superfluousPeersCleanupEnabled forKey:@"superfluousPeersCleanupEnabled"];
}

- (NSDictionary*)dictionaryRepresentation {
    NSMutableDictionary *ret = [[NSMutableDictionary alloc] init];

    ret[@"postRepairCFU"] = @(self.postRepairCFU);
    ret[@"postEscrowCFU"] = @(self.postEscrowCFU);
    ret[@"resetOctagon"] = @(self.resetOctagon);
    ret[@"leaveTrust"] = @(self.leaveTrust);
    ret[@"reroll"] = @(self.reroll);
    if (self.moveRequest != nil) {
        ret[@"moveRequest"] = [self.moveRequest dictionaryRepresentation];
    }
    ret[@"totalEscrowRecords"] = @(self.totalEscrowRecords);
    ret[@"collectableEscrowRecords"] = @(self.collectableEscrowRecords);
    ret[@"collectedEscrowRecords"] = @(self.collectedEscrowRecords);
    ret[@"escrowRecordGarbageCollectionEnabled"] = @(self.escrowRecordGarbageCollectionEnabled);
    ret[@"totalTlkShares"] = @(self.totalTlkShares);
    ret[@"collectableTlkShares"] = @(self.collectableTlkShares);
    ret[@"collectedTlkShares"] = @(self.collectedTlkShares);
    ret[@"tlkShareGarbageCollectionEnabled"] = @(self.tlkShareGarbageCollectionEnabled);
    ret[@"totalPeers"] = @(self.totalPeers);
    ret[@"trustedPeers"] = @(self.trustedPeers);
    ret[@"superfluousPeers"] = @(self.superfluousPeers);
    ret[@"peersCleanedup"] = @(self.peersCleanedup);
    ret[@"superfluousPeersCleanupEnabled"] = @(self.superfluousPeersCleanupEnabled);

    return ret;
}

@end
