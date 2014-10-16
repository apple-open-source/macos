//
//  TestValidator.m
//  TestValidator
//
//  Created by James Murphy on 12/13/12.
//  Copyright 2012 James Murphy. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "ValidateAsset.h"

int main (int argc, const char * argv[])
{
    @autoreleasepool
    {
        NSLog(@"Starting the TestValidator");
        NSString* resource_path = [[NSBundle mainBundle] resourcePath];
        const char* asset_dir_path = [resource_path UTF8String];
        if (ValidateAsset(asset_dir_path, 99))
        {
            NSLog(@"The assert did not validate");
        }
        else
        {
            NSLog(@"The assert validated!");
        }
    
    }
    return 0;
}
