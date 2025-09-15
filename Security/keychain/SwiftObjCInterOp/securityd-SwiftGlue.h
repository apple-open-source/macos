//
//  securityd-SwiftGlue.h
//  Security
//
//

#ifndef securityd_SwiftGlue_h
#define securityd_SwiftGlue_h
#if __has_include("securityd-Swift.h")
#import "securityd-Swift.h"
#elif __has_include("secd-Swift.h")
#import "secd-Swift.h"
#elif __has_include("KCSharingTests-Swift.h")
#import "KCSharingTests-Swift.h"
#else
#error "move including file to one of the target above in securityd-SwiftGlue.h"
#endif

#endif /* securityd_SwiftGlue_h */
