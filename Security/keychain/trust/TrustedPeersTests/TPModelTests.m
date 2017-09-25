/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#import <XCTest/XCTest.h>
#import <TrustedPeers/TrustedPeers.h>
#import "TPDummySigningKey.h"
#import "TPDummyDecrypter.h"
#import "TPDummyEncrypter.h"

@interface TPModelTests : XCTestCase

@property (nonatomic, strong) TPModel *model;
@property (nonatomic, strong) TPPolicyDocument *policyDocV1;
@property (nonatomic, strong) TPPolicyDocument *policyDocV2;
@property (nonatomic, strong) NSString *secretName;
@property (nonatomic, strong) NSData *secretKey;

@end

@implementation TPModelTests

- (TPModel *)makeModel
{
    id<TPDecrypter> decrypter = [TPDummyDecrypter dummyDecrypter];
    TPModel *model = [[TPModel alloc] initWithDecrypter:decrypter];
    [model registerPolicyDocument:self.policyDocV1];
    [model registerPolicyDocument:self.policyDocV2];
    return model;
}

- (void)setUp
{
    self.secretName = @"foo";
    TPDummyEncrypter *encrypter = [TPDummyEncrypter dummyEncrypterWithKey:[@"sekritkey" dataUsingEncoding:NSUTF8StringEncoding]];
    self.secretKey = encrypter.decryptionKey;
    NSData *redaction = [TPPolicyDocument redactionWithEncrypter:encrypter
                                                 modelToCategory:@[ @{ @"prefix": @"iCycle",  @"category": @"full" } ]
                                                categoriesByView:nil
                                           introducersByCategory:nil
                                                           error:NULL];
    
    self.policyDocV1
    = [TPPolicyDocument policyDocWithVersion:1
                             modelToCategory:@[
                                               @{ @"prefix": @"iPhone",  @"category": @"full" },
                                               @{ @"prefix": @"iPad",    @"category": @"full" },
                                               @{ @"prefix": @"Mac",     @"category": @"full" },
                                               @{ @"prefix": @"iMac",    @"category": @"full" },
                                               @{ @"prefix": @"AppleTV", @"category": @"tv" },
                                               @{ @"prefix": @"Watch",   @"category": @"watch" },
                                               ]
                            categoriesByView:@{
                                               @"WiFi":              @[ @"full", @"tv", @"watch" ],
                                               @"SafariCreditCards": @[ @"full" ],
                                               @"PCSEscrow":         @[ @"full" ]
                                               }
                       introducersByCategory:@{
                                               @"full":  @[ @"full" ],
                                               @"tv":    @[ @"full", @"tv" ],
                                               @"watch": @[ @"full", @"watch" ]
                                               }
                                  redactions:@{
                                               self.secretName: redaction
                                               }
                                    hashAlgo:kTPHashAlgoSHA256];

    self.policyDocV2
    = [TPPolicyDocument policyDocWithVersion:2
                             modelToCategory:@[
                                               @{ @"prefix": @"iCycle",  @"category": @"full" }, // new
                                               @{ @"prefix": @"iPhone",  @"category": @"full" },
                                               @{ @"prefix": @"iPad",    @"category": @"full" },
                                               @{ @"prefix": @"Mac",     @"category": @"full" },
                                               @{ @"prefix": @"iMac",    @"category": @"full" },
                                               @{ @"prefix": @"AppleTV", @"category": @"tv" },
                                               @{ @"prefix": @"Watch",   @"category": @"watch" },
                                               ]
                            categoriesByView:@{
                                               @"WiFi":              @[ @"full", @"tv", @"watch" ],
                                               @"SafariCreditCards": @[ @"full" ],
                                               @"PCSEscrow":         @[ @"full" ]
                                               }
                       introducersByCategory:@{
                                               @"full":  @[ @"full" ],
                                               @"tv":    @[ @"full", @"tv" ],
                                               @"watch": @[ @"full", @"watch" ]
                                               }
                                  redactions:@{}
                                    hashAlgo:kTPHashAlgoSHA256];

    self.model = [self makeModel];
}

- (TPPeerPermanentInfo *)makePeerWithMachineID:(NSString *)machineID
{
    return [self makePeerWithMachineID:machineID modelID:@"iPhone" epoch:1 key:machineID];
}

- (TPPeerPermanentInfo *)makePeerWithMachineID:(NSString *)machineID
                                       modelID:(NSString *)modelID
                                         epoch:(TPCounter)epoch
                                           key:(NSString *)key
{
    NSData *keyData = [key dataUsingEncoding:NSUTF8StringEncoding];
    id<TPSigningKey> trustSigningKey = [[TPDummySigningKey alloc] initWithPublicKeyData:keyData];
    TPPeerPermanentInfo *permanentInfo
    = [TPPeerPermanentInfo permanentInfoWithMachineID:machineID
                                              modelID:modelID
                                                epoch:epoch
                                      trustSigningKey:trustSigningKey
                                       peerIDHashAlgo:kTPHashAlgoSHA256
                                                error:NULL];
    [self.model registerPeerWithPermanentInfo:permanentInfo];
    
    TPPeerStableInfo *stableInfo = [self.model createStableInfoWithDictionary:@{}
                                                                policyVersion:self.policyDocV1.policyVersion
                                                                   policyHash:self.policyDocV1.policyHash
                                                                policySecrets:nil
                                                                forPeerWithID:permanentInfo.peerID
                                                                        error:NULL];
    [self.model updateStableInfo:stableInfo forPeerWithID:permanentInfo.peerID];
    return permanentInfo;
}

static BOOL circleEquals(TPCircle *circle, NSArray<NSString*> *includedPeerIDs, NSArray<NSString*> *excludedPeerIDs)
{
    return [circle isEqualToCircle:[TPCircle circleWithIncludedPeerIDs:includedPeerIDs excludedPeerIDs:excludedPeerIDs]];
}

- (void)testModelBasics
{
    NSString *A = [self makePeerWithMachineID:@"aaa"].peerID;
    NSString *B = [self makePeerWithMachineID:@"bbb"].peerID;
    NSString *C = [self makePeerWithMachineID:@"ccc"].peerID;

    TPCircle *circle;
    
    // A trusts B, establishes clique
    circle = [self.model advancePeerWithID:A addingPeerIDs:@[B] removingPeerIDs:@[] createClique:^NSString *{
        return @"clique1";
    }];
    XCTAssert(circleEquals(circle, @[A, B], @[]));

    // B trusts A
    circle = [self.model advancePeerWithID:B addingPeerIDs:@[A] removingPeerIDs:@[] createClique:nil];
    XCTAssert(circleEquals(circle, @[A, B], @[]));

    // A trusts C
    circle = [self.model advancePeerWithID:A addingPeerIDs:@[C] removingPeerIDs:@[] createClique:nil];
    XCTAssert(circleEquals(circle, @[A, B, C], @[]));

    // C trusts A
    circle = [self.model advancePeerWithID:C addingPeerIDs:@[A] removingPeerIDs:@[] createClique:nil];
    XCTAssert(circleEquals(circle, @[A, B, C], @[]));

    // Updating B (B should now trust C)
    circle = [self.model advancePeerWithID:B addingPeerIDs:nil removingPeerIDs:nil createClique:nil];
    XCTAssert(circleEquals(circle, @[A, B, C], @[]));
    
    // Updating B again (should be no change)
    circle = [self.model advancePeerWithID:B addingPeerIDs:nil removingPeerIDs:nil createClique:nil];
    XCTAssert(circleEquals(circle, @[A, B, C], @[]));

    // A decides to exclude B
    circle = [self.model advancePeerWithID:A addingPeerIDs:nil removingPeerIDs:@[B] createClique:nil];
    XCTAssert(circleEquals(circle, @[A, C], @[B]));
    
    // Updating C (C should now exclude B)
    circle = [self.model advancePeerWithID:C addingPeerIDs:nil removingPeerIDs:nil createClique:nil];
    XCTAssert(circleEquals(circle, @[A, C], @[B]));

    // Updating B (B should now exclude itself and include nobody)
    circle = [self.model advancePeerWithID:B addingPeerIDs:nil removingPeerIDs:nil createClique:nil];
    XCTAssert(circleEquals(circle, @[], @[B]));

    // Updating B again (should be no change)
    circle = [self.model advancePeerWithID:B addingPeerIDs:nil removingPeerIDs:nil createClique:nil];
    XCTAssert(circleEquals(circle, @[], @[B]));
    
    // C decides to exclude itself
    circle = [self.model advancePeerWithID:C addingPeerIDs:nil removingPeerIDs:@[C] createClique:nil];
    XCTAssert(circleEquals(circle, @[], @[C]));

    // Updating C (should be no change)
    circle = [self.model advancePeerWithID:C addingPeerIDs:nil removingPeerIDs:nil createClique:nil];
    XCTAssert(circleEquals(circle, @[], @[C]));
    
    // Updating A (A should now exclude C)
    circle = [self.model advancePeerWithID:A addingPeerIDs:nil removingPeerIDs:nil createClique:nil];
    XCTAssert(circleEquals(circle, @[A], @[B, C]));
}

- (void)testPeerReplacement
{
    NSString *A = [self makePeerWithMachineID:@"aaa"].peerID;
    NSString *B = [self makePeerWithMachineID:@"bbb"].peerID;
    NSString *C = [self makePeerWithMachineID:@"ccc"].peerID;
    
    TPCircle *circle;

    // A trusts B, establishes clique. A is in a drawer.
    circle = [self.model advancePeerWithID:A addingPeerIDs:@[B] removingPeerIDs:@[] createClique:^NSString *{
        return @"clique1";
    }];
    XCTAssert(circleEquals(circle, @[A, B], @[]));

    // B trusts A
    circle = [self.model advancePeerWithID:B addingPeerIDs:@[A] removingPeerIDs:@[] createClique:nil];
    XCTAssert(circleEquals(circle, @[A, B], @[]));

    // B decides to replace itself with C.
    circle = [self.model advancePeerWithID:B addingPeerIDs:@[C] removingPeerIDs:@[B] createClique:nil];
    XCTAssert(circleEquals(circle, @[C], @[B]));
    
    // B should be able to update itself without forgetting it trusts C.
    circle = [self.model advancePeerWithID:B addingPeerIDs:nil removingPeerIDs:nil createClique:nil];
    XCTAssert(circleEquals(circle, @[C], @[B]));

    // When A wakes up, it should trust C instead of B.
    circle = [self.model advancePeerWithID:A addingPeerIDs:nil removingPeerIDs:nil createClique:nil];
    XCTAssert(circleEquals(circle, @[A, C], @[B]));
}

- (void)testVoucher
{
    TPPeerPermanentInfo *aaa = [self makePeerWithMachineID:@"aaa"];
    TPPeerPermanentInfo *bbb = [self makePeerWithMachineID:@"bbb"];
    TPPeerPermanentInfo *ccc = [self makePeerWithMachineID:@"ccc"];
    
    NSString *A = aaa.peerID;
    NSString *B = bbb.peerID;
    NSString *C = ccc.peerID;
    
    TPCircle *circle;
    
    // A establishes clique.
    circle = [self.model advancePeerWithID:A addingPeerIDs:nil removingPeerIDs:nil createClique:^NSString *{
        return @"clique1";
    }];
    XCTAssert(circleEquals(circle, @[A], @[]));

    // B trusts A
    circle = [self.model advancePeerWithID:B addingPeerIDs:@[A] removingPeerIDs:@[] createClique:nil];
    XCTAssert(circleEquals(circle, @[A, B], @[]));
    
    // C trusts A
    circle = [self.model advancePeerWithID:C addingPeerIDs:@[A] removingPeerIDs:@[] createClique:nil];
    XCTAssert(circleEquals(circle, @[A, C], @[]));
    
    // B gets a voucher from A
    TPVoucher *voucher = [self.model createVoucherForCandidate:bbb withSponsorID:A error:NULL];
    XCTAssertNotNil(voucher);
    XCTAssertEqual(TPResultOk, [self.model registerVoucher:voucher]);
    
    // Updating C, it sees the voucher and now trusts B
    circle = [self.model advancePeerWithID:C addingPeerIDs:nil removingPeerIDs:nil createClique:nil];
    XCTAssert(circleEquals(circle, @[A, B, C], @[]));

    // Updating A, it sees the voucher (sponsored by A itself) and now trusts B.
    // (A updating its dynamicInfo also expires the voucher.)
    circle = [self.model advancePeerWithID:A addingPeerIDs:nil removingPeerIDs:nil createClique:nil];
    XCTAssert(circleEquals(circle, @[A, B], @[]));
}

- (void)testExpiredVoucher
{
    TPPeerPermanentInfo *aaa = [self makePeerWithMachineID:@"aaa"];
    TPPeerPermanentInfo *bbb = [self makePeerWithMachineID:@"bbb"];
    TPPeerPermanentInfo *ccc = [self makePeerWithMachineID:@"ccc"];
    TPPeerPermanentInfo *ddd = [self makePeerWithMachineID:@"ddd"];
    
    NSString *A = aaa.peerID;
    NSString *B = bbb.peerID;
    NSString *C = ccc.peerID;
    NSString *D = ddd.peerID;
    
    TPCircle *circle;
    
    // A establishes clique.
    circle = [self.model advancePeerWithID:A addingPeerIDs:nil removingPeerIDs:nil createClique:^NSString *{
        return @"clique1";
    }];
    XCTAssert(circleEquals(circle, @[A], @[]));
    
    // B trusts A
    circle = [self.model advancePeerWithID:B addingPeerIDs:@[A] removingPeerIDs:@[] createClique:nil];
    XCTAssert(circleEquals(circle, @[A, B], @[]));
    
    // C trusts A
    circle = [self.model advancePeerWithID:C addingPeerIDs:@[A] removingPeerIDs:@[] createClique:nil];
    XCTAssert(circleEquals(circle, @[A, C], @[]));
    
    // B gets a voucher from A (but doesn't register the voucher yet because A would notice it)
    TPVoucher *voucher = [self.model createVoucherForCandidate:bbb withSponsorID:A error:NULL];
    
    // A advances its clock by deciding to trust D
    circle = [self.model advancePeerWithID:A addingPeerIDs:@[D] removingPeerIDs:nil createClique:nil];
    XCTAssert(circleEquals(circle, @[A, D], @[]));

    // Register the voucher, which is now expired because A has advanced its clock
    [self.model registerVoucher:voucher];

    // Updating C, it ignores the expired voucher for B
    circle = [self.model advancePeerWithID:C addingPeerIDs:nil removingPeerIDs:nil createClique:nil];
    XCTAssert(circleEquals(circle, @[A, C, D], @[]));
}

- (void)testVoucherWithBadSignature
{
    TPPeerPermanentInfo *aaa = [self makePeerWithMachineID:@"aaa"];
    TPPeerPermanentInfo *bbb = [self makePeerWithMachineID:@"bbb"];
    
    NSString *A = aaa.peerID;
    NSString *B = bbb.peerID;
    
    TPCircle *circle;
    
    // A establishes clique.
    circle = [self.model advancePeerWithID:A addingPeerIDs:nil removingPeerIDs:nil createClique:^NSString *{
        return @"clique1";
    }];
    XCTAssert(circleEquals(circle, @[A], @[]));
    
    // B trusts A
    circle = [self.model advancePeerWithID:B addingPeerIDs:@[A] removingPeerIDs:@[] createClique:nil];
    XCTAssert(circleEquals(circle, @[A, B], @[]));
    
    // B gets a voucher from A, but signed by B's key
    TPVoucher *voucher = [TPVoucher voucherWithBeneficiaryID:B
                                                   sponsorID:A
                                                       clock:[self.model getDynamicInfoForPeerWithID:A].clock
                                             trustSigningKey:bbb.trustSigningKey
                                                       error:NULL];
    XCTAssertNotNil(voucher);
    XCTAssertEqual(TPResultSignatureMismatch, [self.model registerVoucher:voucher]);
}

- (void)testVoucherPolicy
{
    TPPeerPermanentInfo *aaa = [self makePeerWithMachineID:@"aaa" modelID:@"watch" epoch:1 key:@"aaa"];
    TPPeerPermanentInfo *bbb = [self makePeerWithMachineID:@"bbb"];
    
    NSString *A = aaa.peerID;
    
    // B is a phone trying to get a voucher from A which is a watch
    TPVoucher *voucher = [self.model createVoucherForCandidate:bbb withSponsorID:A error:NULL];
    XCTAssertNil(voucher);
}

- (void)testDynamicInfoReplay
{
    NSString *A = [self makePeerWithMachineID:@"aaa"].peerID;
    NSString *B = [self makePeerWithMachineID:@"bbb"].peerID;
    
    TPCircle *circle;
    
    // A establishes clique, trusts B.
    circle = [self.model advancePeerWithID:A addingPeerIDs:@[B] removingPeerIDs:nil createClique:^NSString *{
        return @"clique1";
    }];
    XCTAssert(circleEquals(circle, @[A, B], @[]));
    
    // Attacker snapshots A's dynamicInfo
    TPPeerDynamicInfo *dyn = [self.model getDynamicInfoForPeerWithID:A];
    
    // A excludes B
    circle = [self.model advancePeerWithID:A addingPeerIDs:nil removingPeerIDs:@[B] createClique:nil];
    XCTAssert(circleEquals(circle, @[A], @[B]));
    
    // Attacker replays the old snapshot
    XCTAssertEqual(TPResultClockViolation, [self.model updateDynamicInfo:dyn forPeerWithID:A]);
    
    circle = [self.model getCircleForPeerWithID:A];
    XCTAssert(circleEquals(circle, @[A], @[B]));
}

- (void)testPhoneApprovingWatch
{
    NSString *phoneA = [self makePeerWithMachineID:@"phoneA" modelID:@"iPhone7,1" epoch:1 key:@"phoneA"].peerID;
    NSString *watch = [self makePeerWithMachineID:@"watch" modelID:@"Watch1,1" epoch:1 key:@"watch"].peerID;
    
    TPCircle *circle;
    
    // phoneA establishes clique, trusts watch.
    circle = [self.model advancePeerWithID:phoneA addingPeerIDs:@[watch] removingPeerIDs:nil createClique:^NSString *{
        return @"clique1";
    }];
    XCTAssert(circleEquals(circle, @[phoneA, watch], @[]));
}

- (void)testWatchApprovingPhone
{
    NSString *phoneA = [self makePeerWithMachineID:@"phoneA" modelID:@"iPhone7,1" epoch:1 key:@"phoneA"].peerID;
    NSString *phoneB = [self makePeerWithMachineID:@"phoneB" modelID:@"iPhone7,1" epoch:1 key:@"phoneB"].peerID;
    NSString *watch = [self makePeerWithMachineID:@"watch" modelID:@"Watch1,1" epoch:1 key:@"watch"].peerID;
    
    TPCircle *circle;
    
    // phoneA establishes clique, trusts watch.
    circle = [self.model advancePeerWithID:phoneA addingPeerIDs:@[watch] removingPeerIDs:nil createClique:^NSString *{
        return @"clique1";
    }];
    XCTAssert(circleEquals(circle, @[phoneA, watch], @[]));
    
    // watch trusts phoneA and phoneB
    circle = [self.model advancePeerWithID:watch addingPeerIDs:@[phoneA, phoneB] removingPeerIDs:nil createClique:nil];
    XCTAssert(circleEquals(circle, @[phoneA, phoneB, watch], @[]));
    
    // phoneA updates, and it should ignore phoneB, so no change.
    circle = [self.model advancePeerWithID:phoneA addingPeerIDs:nil removingPeerIDs:nil createClique:nil];
    XCTAssert(circleEquals(circle, @[phoneA, watch], @[]));
}

- (void)testNilCreateClique
{
    NSString *A = [self makePeerWithMachineID:@"aaa"].peerID;
    
    TPCircle *circle;
    
    // Try to establish dynamicInfo without providing createClique
    circle = [self.model advancePeerWithID:A addingPeerIDs:nil removingPeerIDs:nil createClique:nil];
    XCTAssertNil(circle);
}

- (void)testCliqueConvergence
{
    NSString *A = [self makePeerWithMachineID:@"aaa"].peerID;
    NSString *B = [self makePeerWithMachineID:@"bbb"].peerID;
    
    TPCircle *circle;
    
    // A establishes clique1
    circle = [self.model advancePeerWithID:A addingPeerIDs:@[] removingPeerIDs:@[] createClique:^NSString *{
        return @"clique1";
    }];
    XCTAssert(circleEquals(circle, @[A], @[]));
    XCTAssert([[self.model getDynamicInfoForPeerWithID:A].clique isEqualToString:@"clique1"]);

    // B establishes clique2
    circle = [self.model advancePeerWithID:B addingPeerIDs:@[] removingPeerIDs:@[] createClique:^NSString *{
        return @"clique2";
    }];
    XCTAssert(circleEquals(circle, @[B], @[]));
    XCTAssert([[self.model getDynamicInfoForPeerWithID:B].clique isEqualToString:@"clique2"]);
    
    // A trusts B. A should now switch to clique2, which is later than clique1 in lexical order.
    circle = [self.model advancePeerWithID:A addingPeerIDs:@[B] removingPeerIDs:@[] createClique:nil];
    XCTAssert(circleEquals(circle, @[A, B], @[]));
    XCTAssert([[self.model getDynamicInfoForPeerWithID:A].clique isEqualToString:@"clique2"]);
}

- (void)testRemovalCounts
{
    NSString *A = [self makePeerWithMachineID:@"aaa"].peerID;
    NSString *B = [self makePeerWithMachineID:@"bbb"].peerID;
    NSString *C = [self makePeerWithMachineID:@"ccc"].peerID;
    
    // A establishes clique with B and C
    [self.model advancePeerWithID:A addingPeerIDs:@[B, C] removingPeerIDs:@[] createClique:^NSString *{
        return @"clique1";
    }];
    XCTAssertEqual(0ULL, [self.model getDynamicInfoForPeerWithID:A].removals);
    
    // B trusts A
    [self.model advancePeerWithID:B addingPeerIDs:@[A] removingPeerIDs:@[] createClique:nil];
    XCTAssertEqual(0ULL, [self.model getDynamicInfoForPeerWithID:B].removals);
    
    // A removes C
    [self.model advancePeerWithID:A addingPeerIDs:nil removingPeerIDs:@[C] createClique:nil];
    XCTAssertEqual(1ULL, [self.model getDynamicInfoForPeerWithID:A].removals);

    // B updates, and now shows 1 removal
    [self.model advancePeerWithID:B addingPeerIDs:nil removingPeerIDs:nil createClique:nil];
    XCTAssertEqual(1ULL, [self.model getDynamicInfoForPeerWithID:B].removals);
}

- (void)testCommunicatingModels
{
    TPPeerPermanentInfo *aaa = [self makePeerWithMachineID:@"aaa"];
    TPPeerPermanentInfo *bbb = [self makePeerWithMachineID:@"bbb"];
    TPPeerPermanentInfo *ccc = [self makePeerWithMachineID:@"ccc"];

    NSString *A = aaa.peerID;
    NSString *B = bbb.peerID;
    NSString *C = ccc.peerID;
    
    // A lives on self.model, where it trusts B and C
    [self.model advancePeerWithID:A addingPeerIDs:@[B, C] removingPeerIDs:nil createClique:^NSString *{
        return @"clique1";
    }];

    // B lives on model2, where it trusts A
    TPModel *model2 = [self makeModel];
    [model2 registerPeerWithPermanentInfo:aaa];
    [model2 registerPeerWithPermanentInfo:bbb];
    [model2 updateStableInfo:[self.model getStableInfoForPeerWithID:A] forPeerWithID:A];
    [model2 updateStableInfo:[self.model getStableInfoForPeerWithID:B] forPeerWithID:B];
    [model2 advancePeerWithID:B addingPeerIDs:@[A] removingPeerIDs:nil createClique:^NSString *{
        return @"clique1";
    }];
    
    // A's circle and dynamicInfo are transmitted from model to model2
    TPCircle *circle = [self.model getCircleForPeerWithID:A];
    TPPeerDynamicInfo *dyn = [self.model getDynamicInfoForPeerWithID:A];
    [model2 updateDynamicInfo:dyn forPeerWithID:A];
    [model2 registerCircle:circle];

    // B updates in model2, but C is not yet registered so is ignored.
    circle = [model2 advancePeerWithID:B addingPeerIDs:nil removingPeerIDs:nil createClique:nil];
    XCTAssert(circleEquals(circle, @[A, B], @[]));

    // Now C registers in model2
    [model2 registerPeerWithPermanentInfo:ccc];

    // B updates in model2, and now it trusts C.
    circle = [model2 advancePeerWithID:B addingPeerIDs:nil removingPeerIDs:nil createClique:nil];
    XCTAssert(circleEquals(circle, @[A, B, C], @[]));
}

- (void)testCommunicatingModelsWithVouchers
{
    TPPeerPermanentInfo *aaa = [self makePeerWithMachineID:@"aaa"];
    TPPeerPermanentInfo *bbb = [self makePeerWithMachineID:@"bbb"];
    TPPeerPermanentInfo *ccc = [self makePeerWithMachineID:@"ccc"];
    
    NSString *A = aaa.peerID;
    NSString *B = bbb.peerID;
    NSString *C = ccc.peerID;
    
    // A lives on self.model, where it trusts B
    [self.model advancePeerWithID:A addingPeerIDs:@[B] removingPeerIDs:nil createClique:^NSString *{
        return @"clique1";
    }];
    
    // B lives on model2, where it trusts A
    TPModel *model2 = [self makeModel];
    [model2 registerPeerWithPermanentInfo:aaa];
    [model2 registerPeerWithPermanentInfo:bbb];
    [model2 updateStableInfo:[self.model getStableInfoForPeerWithID:A] forPeerWithID:A];
    [model2 updateStableInfo:[self.model getStableInfoForPeerWithID:B] forPeerWithID:B];
    [model2 advancePeerWithID:B addingPeerIDs:@[A] removingPeerIDs:nil createClique:^NSString *{
        return @"clique1";
    }];

    // A's circle and dynamicInfo are transmitted from model to model2
    TPCircle *circle = [self.model getCircleForPeerWithID:A];
    TPPeerDynamicInfo *dyn = [self.model getDynamicInfoForPeerWithID:A];
    [model2 updateDynamicInfo:dyn forPeerWithID:A];
    [model2 registerCircle:circle];

    // A writes a voucher for C, and it is transmitted to model2
    TPVoucher *voucher = [self.model createVoucherForCandidate:ccc withSponsorID:A error:NULL];
    [model2 registerVoucher:voucher];
    
    // B updates in model2, but C is not yet registered so is ignored.
    circle = [model2 advancePeerWithID:B addingPeerIDs:nil removingPeerIDs:nil createClique:nil];
    XCTAssert(circleEquals(circle, @[A, B], @[]));
    
    // Now C registers in model2
    [model2 registerPeerWithPermanentInfo:ccc];
    [model2 updateStableInfo:[self.model getStableInfoForPeerWithID:C] forPeerWithID:C];
    
    // B updates in model2, and now it trusts C.
    circle = [model2 advancePeerWithID:B addingPeerIDs:nil removingPeerIDs:nil createClique:nil];
    XCTAssert(circleEquals(circle, @[A, B, C], @[]));
}

- (void)testReregisterPeer
{
    TPPeerPermanentInfo *aaa = [self makePeerWithMachineID:@"aaa"];
    
    NSString *A = aaa.peerID;
    
    [self.model advancePeerWithID:A addingPeerIDs:nil removingPeerIDs:nil createClique:^NSString *{
        return @"clique1";
    }];
    
    // Registering the peer again should not overwrite its dynamicInfo or other state.
    [self.model registerPeerWithPermanentInfo:aaa];
    XCTAssertNotNil([self.model getDynamicInfoForPeerWithID:A]);
}

- (void)testPeerAccessors
{
    TPPeerPermanentInfo *aaa = [self makePeerWithMachineID:@"aaa"];
    
    NSString *A = aaa.peerID;
    
    XCTAssert([self.model hasPeerWithID:A]);
    
    TPPeerPermanentInfo *aaa2 = [self.model getPermanentInfoForPeerWithID:A];
    XCTAssertEqualObjects(aaa, aaa2);
    
    TPPeerStableInfo *info = [self.model createStableInfoWithDictionary:@{ @"hello": @"world" }
                                                          policyVersion:1
                                                             policyHash:@""
                                                          policySecrets:nil
                                                          forPeerWithID:A
                                                                  error:NULL];
    XCTAssertEqual(TPResultOk, [self.model updateStableInfo:info forPeerWithID:A]);

    XCTAssertEqualObjects([self.model getStableInfoForPeerWithID:A], info);

    [self.model deletePeerWithID:A];
    XCTAssertFalse([self.model hasPeerWithID:A]);
}

- (void)testCircleAccessors
{
    TPCircle *circle = [TPCircle circleWithIncludedPeerIDs:@[@"A, B"] excludedPeerIDs:nil];
    XCTAssertNil([self.model circleWithID:circle.circleID]);
    [self.model registerCircle:circle];
    XCTAssertNotNil([self.model circleWithID:circle.circleID]);
    [self.model deleteCircleWithID:circle.circleID];
    XCTAssertNil([self.model circleWithID:circle.circleID]);
}

- (void)testLatestEpoch
{
    NSString *A = [self makePeerWithMachineID:@"aaa" modelID:@"iPhone" epoch:0 key:@"aaa"].peerID;
    NSString *B = [self makePeerWithMachineID:@"bbb" modelID:@"iPhone" epoch:1 key:@"aaa"].peerID;
    NSString *C = [self makePeerWithMachineID:@"ccc" modelID:@"iPhone" epoch:2 key:@"aaa"].peerID;

    TPCounter epoch = [self.model latestEpochAmongPeerIDs:[NSSet setWithArray:@[A, B, C]]];
    XCTAssertEqual(epoch, 2ULL);
}

- (void)testPeerStatus
{
    NSString *A = [self makePeerWithMachineID:@"aaa" modelID:@"iPhone" epoch:0 key:@"aaa"].peerID;
    NSString *B = [self makePeerWithMachineID:@"bbb" modelID:@"iPhone" epoch:0 key:@"bbb"].peerID;
    NSString *C = [self makePeerWithMachineID:@"ccc" modelID:@"iPhone" epoch:0 key:@"ccc"].peerID;
    NSString *D = [self makePeerWithMachineID:@"ddd" modelID:@"iPhone" epoch:1 key:@"ddd"].peerID;
    NSString *E = [self makePeerWithMachineID:@"eee" modelID:@"iPhone" epoch:2 key:@"eee"].peerID;
    
    XCTAssertEqual([self.model statusOfPeerWithID:A], 0);

    [self.model advancePeerWithID:A addingPeerIDs:@[B, C] removingPeerIDs:@[] createClique:^NSString *{
        return @"clique1";
    }];
    XCTAssertEqual([self.model statusOfPeerWithID:A], 0);

    [self.model advancePeerWithID:B addingPeerIDs:@[A, C] removingPeerIDs:@[] createClique:nil];
    XCTAssertEqual([self.model statusOfPeerWithID:A], TPPeerStatusPartiallyReciprocated);

    [self.model advancePeerWithID:C addingPeerIDs:@[A] removingPeerIDs:@[] createClique:nil];
    XCTAssertEqual([self.model statusOfPeerWithID:A], TPPeerStatusPartiallyReciprocated | TPPeerStatusFullyReciprocated);

    [self.model advancePeerWithID:C addingPeerIDs:@[] removingPeerIDs:@[A] createClique:nil];
    XCTAssertEqual([self.model statusOfPeerWithID:A], TPPeerStatusPartiallyReciprocated | TPPeerStatusExcluded);

    [self.model advancePeerWithID:A addingPeerIDs:@[] removingPeerIDs:@[] createClique:nil];
    XCTAssertEqual([self.model statusOfPeerWithID:A], TPPeerStatusExcluded);
    
    [self.model advancePeerWithID:B addingPeerIDs:@[D] removingPeerIDs:@[] createClique:nil];
    XCTAssertEqual([self.model statusOfPeerWithID:B], TPPeerStatusPartiallyReciprocated | TPPeerStatusOutdatedEpoch);
    
    [self.model advancePeerWithID:C addingPeerIDs:@[E] removingPeerIDs:@[] createClique:nil];
    [self.model advancePeerWithID:B addingPeerIDs:@[] removingPeerIDs:@[] createClique:nil];
    XCTAssertEqual([self.model statusOfPeerWithID:B], TPPeerStatusPartiallyReciprocated | TPPeerStatusOutdatedEpoch | TPPeerStatusAncientEpoch);
}

- (void)testCalculateUnusedCircleIDs
{
    NSString *A = [self makePeerWithMachineID:@"aaa" modelID:@"iPhone" epoch:0 key:@"aaa"].peerID;
    NSString *B = [self makePeerWithMachineID:@"bbb" modelID:@"iPhone" epoch:0 key:@"bbb"].peerID;

    [self.model advancePeerWithID:A addingPeerIDs:@[B] removingPeerIDs:@[] createClique:^NSString *{
        return @"clique1";
    }];
    [self.model advancePeerWithID:B addingPeerIDs:@[B] removingPeerIDs:@[] createClique:nil];
    
    NSSet<NSString*>* unused;
    unused = [self.model calculateUnusedCircleIDs];
    XCTAssertEqualObjects(unused, [NSSet set]);
    
    NSString *circleID = [self.model getCircleForPeerWithID:A].circleID;

    [self.model advancePeerWithID:A addingPeerIDs:@[] removingPeerIDs:@[B] createClique:nil];

    unused = [self.model calculateUnusedCircleIDs];
    XCTAssertEqualObjects(unused, [NSSet setWithObject:circleID]);
}

- (void)testGetPeerIDsTrustedByPeerWithID
{
    NSString *A = [self makePeerWithMachineID:@"aaa" modelID:@"iPhone7,1" epoch:0 key:@"aaa"].peerID;
    NSString *B = [self makePeerWithMachineID:@"bbb" modelID:@"iPhone6,2" epoch:0 key:@"bbb"].peerID;
    NSString *C = [self makePeerWithMachineID:@"ccc" modelID:@"Watch1,1"  epoch:0 key:@"ccc"].peerID;
    [self makePeerWithMachineID:@"ddd" modelID:@"iPhone7,1" epoch:0 key:@"ddd"];
    
    [self.model advancePeerWithID:A addingPeerIDs:@[B, C] removingPeerIDs:@[] createClique:^NSString *{
        return @"clique1";
    }];
    
    // Everyone can access WiFi. Only full peers can access SafariCreditCards
    
    NSSet<NSString *>* peerIDs;
    NSSet<NSString *>* expected;
    
    peerIDs = [self.model getPeerIDsTrustedByPeerWithID:A toAccessView:@"WiFi" error:NULL];
    expected = [NSSet setWithArray:@[A, B, C]];
    XCTAssertEqualObjects(peerIDs, expected);
    
    peerIDs = [self.model getPeerIDsTrustedByPeerWithID:A toAccessView:@"SafariCreditCards" error:NULL];
    expected = [NSSet setWithArray:@[A, B]];
    XCTAssertEqualObjects(peerIDs, expected);
}

- (void)testVectorClock
{
    NSString *A = [self makePeerWithMachineID:@"aaa"].peerID;
    NSString *B = [self makePeerWithMachineID:@"bbb"].peerID;
    NSString *C = [self makePeerWithMachineID:@"ccc"].peerID;

    [self.model advancePeerWithID:A addingPeerIDs:@[B] removingPeerIDs:@[] createClique:^NSString *{
        return @"clique1";
    }];
    [self.model advancePeerWithID:B addingPeerIDs:@[A] removingPeerIDs:@[] createClique:nil];
    
    NSDictionary *dict;
    NSDictionary *expected;

    dict = [self.model vectorClock];
    expected = @{ A: @4, B: @5, C: @3 };
    XCTAssertEqualObjects(dict, expected);

    [self.model advancePeerWithID:C addingPeerIDs:@[A] removingPeerIDs:@[B] createClique:nil];
    [self.model advancePeerWithID:A addingPeerIDs:@[] removingPeerIDs:@[] createClique:nil];
    [self.model advancePeerWithID:B addingPeerIDs:@[] removingPeerIDs:@[] createClique:nil];
    
    dict = [self.model vectorClock];
    expected = @{ A: @7, B: @8, C: @6 };
    XCTAssertEqualObjects(dict, expected);
}

- (void)testICycleApprovingPhoneWithNewPolicy
{
    NSString *phoneA = [self makePeerWithMachineID:@"phoneA" modelID:@"iPhone7,1" epoch:1 key:@"phoneA"].peerID;
    NSString *phoneB = [self makePeerWithMachineID:@"phoneB" modelID:@"iPhone7,1" epoch:1 key:@"phoneB"].peerID;
    NSString *icycle = [self makePeerWithMachineID:@"icycle" modelID:@"iCycle1,1" epoch:1 key:@"icycle"].peerID;
    
    TPCircle *circle;
    
    // phoneA establishes clique, trusts icycle
    circle = [self.model advancePeerWithID:phoneA addingPeerIDs:@[icycle] removingPeerIDs:nil createClique:^NSString *{
        return @"clique1";
    }];
    XCTAssert(circleEquals(circle, @[phoneA, icycle], @[]));
    
    // icycle trusts phoneA and phoneB
    circle = [self.model advancePeerWithID:icycle addingPeerIDs:@[phoneA, phoneB] removingPeerIDs:nil createClique:nil];
    XCTAssert(circleEquals(circle, @[phoneA, phoneB, icycle], @[]));
    
    // phoneA updates, and it doesn't know iCycles can approve phones, so it should ignore phoneB, so no change.
    circle = [self.model advancePeerWithID:phoneA addingPeerIDs:nil removingPeerIDs:nil createClique:nil];
    XCTAssert(circleEquals(circle, @[phoneA, icycle], @[]));

    // icycle presents a new policy that says iCycles can approve phones
    TPPeerStableInfo *stableInfo = [self.model createStableInfoWithDictionary:@{}
                                                                policyVersion:self.policyDocV2.policyVersion
                                                                   policyHash:self.policyDocV2.policyHash
                                                                policySecrets:nil
                                                                forPeerWithID:icycle
                                                                        error:NULL];
    [self.model updateStableInfo:stableInfo forPeerWithID:icycle];

    // phoneA updates again, sees the new policy that says iCycles can approve phones, and now trusts phoneB
    circle = [self.model advancePeerWithID:phoneA addingPeerIDs:nil removingPeerIDs:nil createClique:nil];
    XCTAssert(circleEquals(circle, @[phoneA, phoneB, icycle], @[]));
}

- (void)testICycleApprovingPhoneWithRedactedPolicy
{
    NSString *phoneA = [self makePeerWithMachineID:@"phoneA" modelID:@"iPhone7,1" epoch:1 key:@"phoneA"].peerID;
    NSString *phoneB = [self makePeerWithMachineID:@"phoneB" modelID:@"iPhone7,1" epoch:1 key:@"phoneB"].peerID;
    NSString *icycle = [self makePeerWithMachineID:@"icycle" modelID:@"iCycle1,1" epoch:1 key:@"icycle"].peerID;
    
    TPCircle *circle;
    
    // phoneA establishes clique, trusts icycle
    circle = [self.model advancePeerWithID:phoneA addingPeerIDs:@[icycle] removingPeerIDs:nil createClique:^NSString *{
        return @"clique1";
    }];
    XCTAssert(circleEquals(circle, @[phoneA, icycle], @[]));
    
    // icycle trusts phoneA and phoneB
    circle = [self.model advancePeerWithID:icycle addingPeerIDs:@[phoneA, phoneB] removingPeerIDs:nil createClique:nil];
    XCTAssert(circleEquals(circle, @[phoneA, phoneB, icycle], @[]));
    
    // phoneA updates, and it doesn't know iCycles can approve phones, so it should ignore phoneB, so no change.
    circle = [self.model advancePeerWithID:phoneA addingPeerIDs:nil removingPeerIDs:nil createClique:nil];
    XCTAssert(circleEquals(circle, @[phoneA, icycle], @[]));
    
    // icycle presents a new policy that says iCycles can approve phones
    TPPeerStableInfo *stableInfo = [self.model createStableInfoWithDictionary:@{}
                                                                policyVersion:self.policyDocV1.policyVersion
                                                                   policyHash:self.policyDocV1.policyHash
                                                                policySecrets:@{
                                                                                self.secretName: self.secretKey
                                                                                }
                                                                forPeerWithID:icycle
                                                                        error:NULL];
    [self.model updateStableInfo:stableInfo forPeerWithID:icycle];
    
    // phoneA updates again, sees the new policy that says iCycles can approve phones, and now trusts phoneB
    circle = [self.model advancePeerWithID:phoneA addingPeerIDs:nil removingPeerIDs:nil createClique:nil];
    XCTAssert(circleEquals(circle, @[phoneA, phoneB, icycle], @[]));
}

@end
