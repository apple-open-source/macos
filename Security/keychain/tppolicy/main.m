//
//  main.m
//  tppolicy
//
//  Created by Ben Williamson on 7/13/18.
//

#import <Foundation/Foundation.h>
#import <TrustedPeers/TrustedPeers.h>

int main(int argc, const char * argv[])
{
    NSArray<TPPolicyDocument*> *docs
    = @[
        [TPPolicyDocument policyDocumentWithVersion:1
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
                                         redactions:@{}
                                     keyViewMapping:@[]
                                           hashAlgo:kTPHashAlgoSHA256],
        [TPPolicyDocument policyDocumentWithVersion:2
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
                                     keyViewMapping:@[]
                                           hashAlgo:kTPHashAlgoSHA256],
        [TPPolicyDocument policyDocumentWithVersion:3
                                    modelToCategory:@[
                                                      @{ @"prefix": @"Watch7", @"category": @"full" }, // upgraded
                                                      @{ @"prefix": @"iCycle",  @"category": @"full" },
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
                                     keyViewMapping:@[]
                                           hashAlgo:kTPHashAlgoSHA256],
        ];

    for (TPPolicyDocument *doc in docs) {
        NSString *base64 = [doc.protobuf base64EncodedStringWithOptions:0];
        printf("policyVersion: %llu,\n", doc.policyVersion);
        printf("policyHash: \"%s\",\n", doc.policyHash.UTF8String);
        printf("policyData: \"%s\"\n", base64.UTF8String);
    }
    return 0;
}
