//
//  ISProtectedItemsController.m
//  ISACLProtectedItems
//
//  Copyright (c) 2014 Apple. All rights reserved.
//

#import "ISProtectedItemsController.h"
#import <spawn.h>

char * const pathToScrtiptFile = "/usr/local/bin/KeychainItemsAclTest.sh";

@implementation ISProtectedItemsController

- (NSArray *)specifiers
{
    if (!_specifiers) {
        _specifiers = [self loadSpecifiersFromPlistName:@"ISProtectedItems" target:self];
    }

    return _specifiers;
}

- (void)createBatchOfItems:(PSSpecifier *)specifier
{
    char * const argv[] = { pathToScrtiptFile,
                            "op=create",
                            NULL };

    posix_spawn(NULL, pathToScrtiptFile, NULL, NULL, argv, NULL);
}

- (void)deleteBatchOfItems:(PSSpecifier *)specifier
{
    char * const argv[] = { pathToScrtiptFile,
                            "op=delete",
                            NULL };

    posix_spawn(NULL, pathToScrtiptFile, NULL, NULL, argv, NULL);
}

@end
