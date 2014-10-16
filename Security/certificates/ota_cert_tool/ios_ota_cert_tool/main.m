//
//  main.m
//  ios_ota_cert_tool
//
//  Created by James Murphy on 12/11/12.
//  Copyright (c) 2012 James Murphy. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "PSIOSCertToolApp.h"

/*
 printf("%s usage:\n", [self.app_name UTF8String]);
 printf(" [-h, --help]          			\tPrint out this help message\n");
 printf(" [-r, --roots_dir]     			\tThe full path to the directory with the certificate roots\n");
 printf(" [-k, --revoked_dir]   			\tThe full path to the directory with the revoked certificates\n");
 printf(" [-d, --distrusted_dir] 		\tThe full path to the directory with the distrusted certificates\n");
 printf(" [-c, --certs_dir] 				\tThe full path to the directory with the cert certificates\n");
 printf(" [-e, --ev_plist_path] 			\tThe full path to the EVRoots.plist file\n");
 printf(" [-t, --top_level_directory]	\tThe full path to the top level security_certificates directory\n");
 printf(" [-o, --output_directory]       \tThe full path to the directory to write out the results\n");
 printf("\n");
*/

int main(int argc, const char * argv[])
{
//#define HARDCODE 1
    
#ifdef HARDCODE
    
    const char* myArgv[] =
    {
        "foo",
        "--top_level_directory",
        "~/PR-10636667/security/certificates",
        "--output_directory",
        "~/cert_out"
    };
    
    int myArgc = (sizeof(myArgv) / sizeof(const char*));
    
    argc = myArgc;
    argv = myArgv;
#endif  // HARDCODE

    
    @autoreleasepool
    {
        PSIOSCertToolApp* app = [[PSIOSCertToolApp alloc] init:argc withArguments:argv];
        if (![app processCertificates])
        {
            NSLog(@"Could not process the certificate directories");
            return -1;
        }
        
        if (![app outputPlistsToDirectory])
        {
            NSLog(@"Could not output the plists");
            return -1;
        }
        
        if (![app validate])
        {
            NSLog(@"Could not validate the output plists");
            return -1;
        }
        
    }
    return 0;
}

