//
//  SecuritydAssertHelper.m
//  SecuritydAssertHelper
//
//  Copyright (c) 2012-2013 Apple Inc. All Rights Reserved.
//

#import <Foundation/Foundation.h>
#include <unistd.h>
#import <MobileAsset/MobileAsset.h>

static NSString * const PKITrustDataAssetType = @"com.apple.MobileAsset.PKITrustServices.PKITrustData";


int main (int argc, const char * argv[])
{
    @autoreleasepool
    {
        ASAssetQuery* assetQuery = [[ASAssetQuery alloc] initWithAssetType:PKITrustDataAssetType];
        
        if (nil == assetQuery)
        {
            NSLog(@"Could not create an ASAssetQuery object");
            exit(EXIT_FAILURE);
        }
        
        NSError* error = nil;
        NSArray* foundAssets = nil;
        
        foundAssets = [assetQuery runQueryAndReturnError:&error];
        if (nil == foundAssets)
        {
            NSLog(@"running assetQuery fails: %@", [error localizedDescription]);
            exit(EXIT_FAILURE);
        }
        
        return 0;
    }
}
