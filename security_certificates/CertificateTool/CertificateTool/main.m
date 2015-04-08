//
//  main.m
//  CertificateTool
//
//  Copyright (c) 2012-2013 Apple Inc. All Rights Reserved.
//

#import <Foundation/Foundation.h>
#import "CertificateToolApp.h"
#import "ValidateAsset.h"

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
    
/* ============================================================
    This section is only used to help debug this tool
    Uncommenting out the HARDCODE line will allow for testing 
    this tool with having to run the BuildiOSAsset script
   ============================================================ */
//#define HARDCODE 1
    
#ifdef HARDCODE
    
    const char* myArgv[] =
    {
        "foo",
        "--top_level_directory",
        "/Volumes/Data/RestoreStuff/Branches/PR-14030167/security/certificates/CertificateTool/..",
        "--output_directory",
        "~/BuiltAssets"
    };
    
    int myArgc = (sizeof(myArgv) / sizeof(const char*));
    
    argc = myArgc;
    argv = myArgv;
#endif  // HARDCODE
    
    
    @autoreleasepool
    {
        CertificateToolApp* app = [[CertificateToolApp alloc] init:argc withArguments:argv];
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
                
        if (![app createManifest])
        {
            NSLog(@"Could not create the manifest");
            return -1;
        }
        
        
    }
    return 0;
}

