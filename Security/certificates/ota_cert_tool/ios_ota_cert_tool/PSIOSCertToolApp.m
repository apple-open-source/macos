//
//  PSIOSCertToolApp.m
//  ios_ota_cert_tool
//
//  Created by James Murphy on 12/11/12.
//  Copyright (c) 2012 James Murphy. All rights reserved.
//

#import "PSIOSCertToolApp.h"
#import "PSCerts.h"
#import "PSUtilities.h"
#import "ValidateAsset.h"
#import <stdio.h>


@interface PSIOSCertToolApp (PrivateMethods)

- (void)usage;
- (NSString*)checkPath:(NSString*)name basePath:(NSString *)basePath isDirectory:(BOOL)isDirectory;
- (BOOL)outputPlistToPath:(NSString *)path withData:(id)data;
- (BOOL)buildManifest:(NSString *)path;
- (NSNumber*)getNextVersionNumber;
@end

@implementation PSIOSCertToolApp

@synthesize app_name = _app_name;
@synthesize root_directory = _root_directory;
@synthesize revoked_directory = _revoked_directory;
@synthesize distrusted_directory = _distrusted_directory;
@synthesize certs_directory = _certs_directory;
@synthesize ev_plist_path = _ev_plist_path;
@synthesize info_plist_path = _info_plist_path;
@synthesize top_level_directory = _top_level_directory;
@synthesize output_directory = _output_directory;


- (id)init:(int)argc withArguments:(const char**)argv
{
    if ((self = [super init]))
    {
		_app_name = [[NSString alloc] initWithUTF8String:argv[0]];
		_root_directory = nil;
		_revoked_directory = nil;
		_distrusted_directory = nil;
		_certs_directory = nil;
		_ev_plist_path = nil;
        _info_plist_path = nil;
		_top_level_directory = nil;
        _output_directory = nil;
        
        _roots = nil;
        _revoked = nil;
        _distrusted = nil;
        _certs = nil;
		_plist_name_array = [NSArray arrayWithObjects:@"roots.plist", @"revoked.plist", @"distrusted.plist", @"certs.plist", nil];
		      
        for (int iCnt = 1; iCnt < argc; iCnt++)
        {
            const char* arg = argv[iCnt];
            if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
            {
                [self usage];
                return nil;
            }
            else if (!strcmp(arg, "-r") || !strcmp(arg, "--roots_dir"))
            {
                if ((iCnt + 1) == argc)
                {
                    [self usage];
                    return nil;
                }
                
                _root_directory = [[NSString stringWithUTF8String:argv[iCnt + 1]] stringByExpandingTildeInPath];
                iCnt++;
                
            }
			else if (!strcmp(arg, "-k") || !strcmp(arg, "--revoked_dir"))
            {
                if ((iCnt + 1) == argc)
                {
                    [self usage];
                    return nil;
                }
                
                _revoked_directory = [[NSString stringWithUTF8String:argv[iCnt + 1]]  stringByExpandingTildeInPath];
                iCnt++;
            }
			else if (!strcmp(arg, "-d") || !strcmp(arg, "--distrusted_dir"))
            {
                if ((iCnt + 1) == argc)
                {
                    [self usage];
                    return nil;
                }
                
                _distrusted_directory = [[NSString stringWithUTF8String:argv[iCnt + 1]] stringByExpandingTildeInPath];
                iCnt++;
            }
			else if (!strcmp(arg, "-c") || !strcmp(arg, "--certs_dir"))
            {
                if ((iCnt + 1) == argc)
                {
                    [self usage];
                    return nil;
                }
                
                _certs_directory = [[NSString stringWithUTF8String:argv[iCnt + 1]] stringByExpandingTildeInPath];
                iCnt++;
            }
			else if (!strcmp(arg, "-e") || !strcmp(arg, "--ev_plist_path"))
            {
                if ((iCnt + 1) == argc)
                {
                    [self usage];
                    return nil;
                }
                
                _ev_plist_path = [[NSString stringWithUTF8String:argv[iCnt + 1]] stringByExpandingTildeInPath];
                iCnt++;
            }
            
            else if (!strcmp(arg, "-i") || !strcmp(arg, "--info_plist_path"))
            {
                if ((iCnt + 1) == argc)
                {
                    [self usage];
                    return nil;
                }
                
                _info_plist_path = [[NSString stringWithUTF8String:argv[iCnt + 1]] stringByExpandingTildeInPath];
                iCnt++;
            }
			else if (!strcmp(arg, "-t") || !strcmp(arg, "--top_level_directory"))
            {
                if ((iCnt + 1) == argc)
                {
                    [self usage];
                    return nil;
                }
                
                _top_level_directory = [[NSString stringWithUTF8String:argv[iCnt + 1]] stringByExpandingTildeInPath];
                iCnt++;
            }
            else if (!strcmp(arg, "-o") || !strcmp(arg, "--output_directory"))
            {
                if ((iCnt + 1) == argc)
                {
                    [self usage];
                    return nil;
                }
                
                _output_directory = [[NSString stringWithUTF8String:argv[iCnt + 1]] stringByExpandingTildeInPath];
                iCnt++;
            }
        }
        		
        if (nil == _root_directory)
        {
			_root_directory = [self checkPath:@"roots" basePath:_top_level_directory isDirectory:YES];
		 	if (nil == _root_directory)
			{
				[self usage];
				return nil;
        	}
		}
		
		if (nil == _revoked_directory)
		{
			_revoked_directory = [self checkPath:@"revoked" basePath:_top_level_directory isDirectory:YES];
		 	if (nil == _revoked_directory)
			{
				[self usage];
				return nil;
        	}
		}
		
		if (nil == _distrusted_directory)
		{
			_distrusted_directory = [self checkPath:@"distrusted" basePath:_top_level_directory isDirectory:YES];
		 	if (nil == _distrusted_directory)
			{
				[self usage];
				return nil;
        	}
		}
		
		if (nil == _certs_directory)
		{
			_certs_directory = [self checkPath:@"certs" basePath:_top_level_directory isDirectory:YES];
		 	if (nil == _certs_directory)
			{
				[self usage];
				return nil;
        	}
		}
		
		if (nil == _ev_plist_path)
		{
			_ev_plist_path = [self checkPath:@"EVRoots/EVRoots.plist" basePath:_top_level_directory isDirectory:NO];
		 	if (nil == _ev_plist_path)
			{
				[self usage];
				return nil;
        	}
		}
        if (nil == _info_plist_path)
        {
           _info_plist_path =  [self checkPath:@"assetData/Info.plist" basePath:_top_level_directory isDirectory:NO];
            if (nil == _info_plist_path)
			{
				[self usage];
				return nil;
        	}
        }
    }
    return self;
}

- (void)usage
{
	printf("%s usage:\n", [self.app_name UTF8String]);
	printf(" [-h, --help]          			\tPrint out this help message\n");
	printf(" [-r, --roots_dir]     			\tThe full path to the directory with the certificate roots\n");
	printf(" [-k, --revoked_dir]   			\tThe full path to the directory with the revoked certificates\n");
	printf(" [-d, --distrusted_dir] 		\tThe full path to the directory with the distrusted certificates\n");
	printf(" [-c, --certs_dir] 				\tThe full path to the directory with the cert certificates\n");
	printf(" [-e, --ev_plist_path] 			\tThe full path to the EVRoots.plist file\n");
    printf(" [-i, --info_plist_path])       \tThe full path to the Infor.plist file\n");
	printf(" [-t, --top_level_directory]	\tThe full path to the top level security_certificates directory\n");
	printf(" [-o, --output_directory]       \tThe full path to the directory to write out the results\n");
	printf("\n");
}

- (NSString*)checkPath:(NSString*)name basePath:(NSString *)basePath isDirectory:(BOOL)isDirectory
{
	NSString* result = nil;
	if (nil == name)
	{
		return result;
	}
	
	NSFileManager* fileManager = [NSFileManager defaultManager];
	BOOL isDir = NO;
	
	if ([name hasPrefix:@"/"] || [name hasPrefix:@"~"])
	{
        name = [name hasPrefix:@"~"] ? [name stringByExpandingTildeInPath] : name;
		// This is a full path
		if (![fileManager fileExistsAtPath:name isDirectory:&isDir] || isDir != isDirectory)
		{
			NSLog(@"%@ is invalid", name);
			return result;
		}
		result = name;
	}
	else
	{
		NSString* full_path = nil;
		if (nil == basePath)
		{
			NSLog(@"%@ is not a full path but basePath is nil", name);
			return result;
		}
		
		full_path = [basePath stringByAppendingPathComponent:name];
		if (![fileManager fileExistsAtPath:full_path isDirectory:&isDir] || isDir != isDirectory)
		{
			NSLog(@"%@ is invalid", full_path);
			return result;
		}
		result = full_path;
	}
	return result;
}
			
	                                                                
- (BOOL)processCertificates
{
	BOOL result = NO;
   
    NSString* path = self.root_directory;
    PSCerts* root_certs = [[PSCerts alloc] initWithCertFilePath:path forBadCerts:NO];
    _roots = root_certs.certs;
    
    path = self.revoked_directory;
    PSCerts* revoked_certs = [[PSCerts alloc] initWithCertFilePath:path forBadCerts:YES];
    _revoked = revoked_certs.certs;
    
    path = self.distrusted_directory;
    PSCerts* distrusted_certs = [[PSCerts alloc] initWithCertFilePath:path forBadCerts:YES];
    _distrusted = distrusted_certs.certs;
    
    path = self.certs_directory;
    PSCerts* certs_certs = [[PSCerts alloc] initWithCertFilePath:path forBadCerts:NO];
    _certs = certs_certs.certs;
    
    result = (nil != _roots && nil != _revoked && nil != _distrusted && nil != _certs);
    return result;
}
    
- (BOOL)outputPlistsToDirectory
{
    BOOL result = NO;
    
    NSString* path = self.output_directory;

	NSFileManager* fileManager = [NSFileManager defaultManager];
	BOOL isDir = NO;
	NSError* error = nil;
	
	if (![fileManager fileExistsAtPath:path isDirectory:&isDir])
	{
		// The directory does not exist so make it.
		if (![fileManager createDirectoryAtPath:path withIntermediateDirectories:YES attributes:nil error:&error])
		{
			return result;
		}
		
	}
	
	// Now make all of the plists files
	if (nil == _roots || nil == _revoked || nil == _distrusted || nil == _certs)
	{
		return result;
	}
	
	NSArray* plist_data = [NSArray arrayWithObjects:_roots, _revoked, _distrusted, _certs, nil];
	NSDictionary* plist_input = [NSDictionary dictionaryWithObjects:plist_data forKeys:_plist_name_array];
	NSEnumerator* dict_enum = [plist_input keyEnumerator];
	
	for (NSString* file_name in dict_enum)
	{
		NSString* full_path = [path stringByAppendingPathComponent:file_name];
		NSArray* data = [plist_input objectForKey:file_name];
        
        NSMutableDictionary* rootObj = [NSMutableDictionary dictionaryWithCapacity:1];
        NSString* key_str = [file_name stringByDeletingPathExtension];
        [rootObj setObject:data forKey:key_str];
        
		if (![self outputPlistToPath:full_path withData:rootObj])
		{
			NSLog(@"Failed to write out plist for %@#", file_name);
		}
	}
    
    // Now output the EVRoots plist
    NSString* full_path = [path stringByAppendingPathComponent:@"EVRoots.plist"];
    if (![fileManager copyItemAtPath:_ev_plist_path toPath:full_path error:&error])
    {
        NSLog(@"Failed to copy the EVRoots.plist file");
    }
    
    // And the Info plist
    full_path = [path stringByAppendingPathComponent:@"Info.plist"];
    if (![fileManager copyItemAtPath:_info_plist_path toPath:full_path error:&error])
    {
        NSLog(@"Failed to copy the Info.plist file");
    }
    
	
	return [self buildManifest:path];
}

- (BOOL)outputPlistToPath:(NSString *)path withData:(id)data
{
	BOOL result = NO;
    NSError* error = nil;
    
    
	NSData* prop_data = [NSPropertyListSerialization dataWithPropertyList:data format:NSPropertyListXMLFormat_v1_0 options:0 error:&error];
	if (nil != prop_data)
	{
		result = [prop_data writeToFile:path atomically:NO];
	}
	return result;
}

- (NSNumber*)getNextVersionNumber
{
	// This needs to read from a file location and then update the 
	// version number.  For now just hard code and I'll fix this later
	NSNumber* result = nil;
	NSUInteger version = 100;
	result = [NSNumber numberWithUnsignedInteger:version];
	
	return result;
}
	

- (BOOL)buildManifest:(NSString *)path
{
	BOOL result = NO;
	NSMutableDictionary* manifest_dict = [NSMutableDictionary dictionary];
	
	NSNumber* versionNumber = [self getNextVersionNumber];
	[manifest_dict setObject:versionNumber forKey:@"Version"];
	
	for (NSString* plist_file_path in _plist_name_array)
	{
		NSString* full_path = [self checkPath:plist_file_path basePath:path isDirectory:NO];
		if (nil == full_path)
		{
			NSLog(@"Could not find the %@ file", plist_file_path);
			return result;
		}
	
		CFDataRef dataRef = [PSUtilities readFile:full_path];
		if (NULL == dataRef)
		{
			NSLog(@"Could not read the file %@", plist_file_path);
			return result;
		}
        
        
		
		NSString* plist_file_data = [PSUtilities digestAndEncode:dataRef useSHA1:NO];
		CFRelease(dataRef);
		if (nil == plist_file_data)
		{
			NSLog(@"Could not hash the file %@", plist_file_path);
			return result;
		}
		
		[manifest_dict setObject:plist_file_data forKey:plist_file_path];
	}
    
    // Now add the EVRoots plist
    NSString* evRoots_path = [self checkPath:@"EVRoots.plist" basePath:path isDirectory:NO];
    if (nil == evRoots_path)
    {
        NSLog(@"Could not find the EVRoots.plist file");
        return result;
    }
    
    CFDataRef dataRef = [PSUtilities readFile:evRoots_path];
    if (NULL == dataRef)
    {
        NSLog(@"Could not read the file %@", evRoots_path);
        return result;
    }
    
    NSString* ev_plist_file_data = [PSUtilities digestAndEncode:dataRef useSHA1:NO];
    CFRelease(dataRef);
    if (nil == ev_plist_file_data)
    {
        NSLog(@"Could not hash the file %@", ev_plist_file_data);
        return result;
    }
    
    [manifest_dict setObject:ev_plist_file_data forKey:@"EVRoots.plist"];

	
	NSString* full_path = [path stringByAppendingPathComponent:@"Manifest.plist"];
	if (![self outputPlistToPath:full_path withData:manifest_dict])
	{
		NSLog(@"Unable to write out the Manifest plist");
		return result;
	}
	
	CFDataRef manifest_file_data = [PSUtilities readFile:full_path];
	if (NULL == manifest_file_data)
	{
		NSLog(@"Unable to read in the Manifest plist");
		return result;
	}
	
	SecKeyRef signing_key = [PSUtilities getPrivateKeyWithName:@"Manifest Private Key"];
	
	if (NULL == signing_key)
	{
		NSLog(@"Unable to get the signing key");
		return result;
	}
	
	NSString* signature = [PSUtilities signAndEncode:manifest_file_data usingKey:signing_key useSHA1:YES];
	if (nil == signature)
	{
		NSLog(@"Unable to sign the manifest data");
		return result;
	}
	
	[manifest_dict setObject:signature forKey:@"Signature"];
	
	if (![self outputPlistToPath:full_path withData:manifest_dict])
	{
		NSLog(@"Unable to write out the Manifest plist");
		return result;
	}
	return YES;
}	
    
- (BOOL)validate
{
    BOOL result = NO;
    
    NSString* path = self.output_directory;
    
    result = (ValidateAsset([path UTF8String], 99)) ? NO : YES;
    
    return result;
}
@end
