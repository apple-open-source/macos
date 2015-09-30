//
//  ISProtectedItemsController.h
//  ISACLProtectedItems
//
//  Copyright (c) 2014 Apple. All rights reserved.
//

// rdar://problem/21142814
// Remove the "pop" below too when the code is changed to not use the deprecated interface
#pragma clang diagnostic push
#pragma clang diagnostic warning "-Wdeprecated-declarations"

#import <Preferences/Preferences.h>

#pragma clang diagnostic pop

@interface ISProtectedItemsController : PSListController

@end
