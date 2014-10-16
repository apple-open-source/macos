//
//  CertificateToolApp.m
//  CertificateTool
//
//  Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
//

#import "CertificateToolApp.h"
#import "PSCerts.h"
#import "PSUtilities.h"
#import "PSAssetConstants.h"
#import "PSCertData.h"
#import "PSCert.h"
#import <Security/Security.h>
#import <CommonCrypto/CommonDigest.h>
#import <Security/SecCertificatePriv.h>
#import "AppleBaselineEscrowCertificates.h"


@interface CertificateToolApp (PrivateMethods)

- (void)usage;
- (NSString*)checkPath:(NSString*)name basePath:(NSString *)basePath isDirectory:(BOOL)isDirectory;

- (BOOL)buildEVRootsData:(NSDictionary *)certs;
- (BOOL)ensureDirectoryPath:(NSString *)dir_path;

@end

@implementation CertificateToolApp


@synthesize app_name = _app_name;
@synthesize root_directory = _root_directory;
@synthesize revoked_directory = _revoked_directory;
@synthesize distrusted_directory = _distrusted_directory;
@synthesize certs_directory = _certs_directory;
@synthesize evroot_config_path = _evroot_config_path;
@synthesize ev_plist_path = _ev_plist_path;
@synthesize info_plist_path = _info_plist_path;
@synthesize top_level_directory = _top_level_directory;
@synthesize output_directory = _output_directory;
@synthesize version_number_plist_path = _version_number_plist_path;
@synthesize version_number = _version_number;


- (id)init:(int)argc withArguments:(const char**)argv
{
    if ((self = [super init]))
    {
		_app_name = [[NSString alloc] initWithUTF8String:argv[0]];

        // set all of the directory paths to nil
		_root_directory = nil;
		_revoked_directory = nil;
		_distrusted_directory = nil;
		_certs_directory = nil;
        _evroot_config_path = nil;
		_ev_plist_path = nil;
        _info_plist_path = nil;
		_top_level_directory = nil;
        _output_directory = nil;
        _version_number_plist_path = nil;
        _version_number = nil;


		_certRootsData = nil;
		_blacked_listed_keys = nil;
        _gray_listed_keys = nil;

        _EVRootsData = [NSMutableDictionary dictionary];
		_derData = nil;


        // Parse the command line arguments and set up the directory paths
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
			else if (!strcmp(arg, "-e") || !strcmp(arg, "--evroot.config"))
            {
                if ((iCnt + 1) == argc)
                {
                    [self usage];
                    return nil;
                }

                _evroot_config_path = [[NSString stringWithUTF8String:argv[iCnt + 1]] stringByExpandingTildeInPath];
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
            else if (!strcmp(arg, "-v") || !strcmp(arg, "--version_number"))
            {
                if ((iCnt + 1) == argc)
                {
                    [self usage];
                    return nil;
                }

                NSString* temp_number_str = [NSString stringWithUTF8String:argv[iCnt + 1]];
                if (nil != temp_number_str)
                {
                    NSInteger value = [temp_number_str integerValue];
                    if (value > 0)
                    {
                        _version_number = [NSNumber numberWithInteger:value];
                    }
                }
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

		if (nil == _evroot_config_path)
		{
			_evroot_config_path = [self checkPath:@"EVRoots/evroot.config" basePath:_top_level_directory isDirectory:NO];
		 	if (nil == _evroot_config_path)
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
        if (nil == _version_number_plist_path)
        {
            _version_number_plist_path = [self checkPath:@"CertificateTool/CertificateTool/AssetVersion.plist" basePath:_top_level_directory isDirectory:NO];
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
	printf(" [-e, --evroot.config] 			\tThe full path to the evroot.config file\n");
    printf(" [-i, --info_plist_path])       \tThe full path to the Infor.plist file\n");
	printf(" [-t, --top_level_directory]	\tThe full path to the top level security_certificates directory\n");
	printf(" [-o, --output_directory]       \tThe full path to the directory to write out the results\n");
    printf(" [-v, --version_number]         \tThe version number of the asset\n");
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

/* --------------------------------------------------------------------------
    Read in the evroot.config file and create a dictionary on the cert file
    names and relate then to their EVRoot OIDs
   -------------------------------------------------------------------------- */
- (BOOL)buildEVRootsData:(NSDictionary *)certs
{
    BOOL result = NO;

    if (nil == _EVRootsData || nil == _evroot_config_path)
    {
        return result;
    }

    // Read file into memory it is not that big
    NSError* error = nil;
    NSData* fileData = [NSData dataWithContentsOfFile:self.evroot_config_path
                           options:NSDataReadingMappedIfSafe error:&error];
    if (nil == fileData)
    {
        return result;
    }

    // Turn the data into a string so that it can be edited
    NSMutableString* evconfig_data = [[NSMutableString alloc] initWithData:fileData
                                        encoding:NSUTF8StringEncoding];
    if (nil == evconfig_data)
    {
        return result;
    }

    // Use Regex to remove all of the comments
    NSRegularExpression* regex_comments =
        [NSRegularExpression regularExpressionWithPattern:@"^#.*\n"
                options:NSRegularExpressionAnchorsMatchLines error:&error];

    NSRange full_string_range = NSMakeRange(0, [evconfig_data length]);
    NSUInteger num_replacements =
        [regex_comments replaceMatchesInString:evconfig_data
                options:0 range:full_string_range withTemplate:@""];

    if (0 == num_replacements)
    {
        return result;
    }

    // Use Regex to remove all of the blank lines
    NSRegularExpression* regex_blankLines =
        [NSRegularExpression regularExpressionWithPattern:@"^\n"
            options:NSRegularExpressionAnchorsMatchLines error:&error];

    full_string_range = NSMakeRange(0, [evconfig_data length]);
    num_replacements = [regex_blankLines replaceMatchesInString:evconfig_data
                            options:0 range:full_string_range withTemplate:@""];

    if (0 == num_replacements)
    {
        return result;
    }

    // Break the single string into an array of lines.
    NSArray* strings = [evconfig_data componentsSeparatedByString:@"\n"];
    if (nil == strings)
    {
        return result;
    }

    // Process each line in the array
    for (NSString* aLine in strings)
    {
        if (nil == aLine || [aLine length] < 2)
        {
            continue;
        }
        NSRegularExpression* regex_oid_str = [NSRegularExpression regularExpressionWithPattern:@"^[[0-9]+.]+"
			options:NSRegularExpressionAnchorsMatchLines error:&error];

		full_string_range = NSMakeRange(0, [aLine length]);
		NSArray* oid_str_matchs = [regex_oid_str matchesInString:aLine options:0 range:full_string_range];
		NSTextCheckingResult* ck_result = [oid_str_matchs objectAtIndex:0];
		NSRange result_range = [ck_result rangeAtIndex:0];
		NSString* oid_str = [aLine substringToIndex:result_range.length];
		NSString* remainder_str = [aLine substringFromIndex:(result_range.length + 1)];
		NSArray* items = [remainder_str componentsSeparatedByString:@"\""];

        // The first item should be an OID string
        NSUInteger num_items = [items count];
        //NSString* oid_str = [items objectAtIndex:0];
        NSUInteger iCnt = 0;

		NSMutableArray* cert_digests = [NSMutableArray array];
        // loop through the names of all of the cert files
        for (iCnt = 1; iCnt < num_items; iCnt++)
        {
            NSString* cert_file_name = [items objectAtIndex:iCnt];
			if (cert_file_name == nil || [cert_file_name hasPrefix:@" "] || [cert_file_name length] < 2)
			{
				continue;
			}
			//NSLog(@"cert_file_name = %@", cert_file_name);

			// find the PSCert record for the file
			PSCert* aCert = [certs objectForKey:cert_file_name];
			if (nil != aCert)
			{
				[cert_digests addObject:aCert.certificate_hash];
			}
			else
			{
				NSLog(@"buildEVRootsData: could not find the cert for %@", cert_file_name);
			}
		}

        NSMutableArray* exisiting_certs = [_EVRootsData objectForKey:oid_str];
        if (nil != exisiting_certs)
        {
            [cert_digests addObjectsFromArray:exisiting_certs];
        }

		[_EVRootsData setObject:cert_digests forKey:oid_str];
    }

    result = YES;
    return result;
}

- (BOOL)ensureDirectoryPath:(NSString *)dir_path
{
    BOOL result = NO;

    if (nil == dir_path)
    {
        return result;
    }

    NSFileManager* fileManager = [NSFileManager defaultManager];
    NSError* error = nil;
    BOOL isDir = NO;

    if (![fileManager fileExistsAtPath:dir_path isDirectory:&isDir])
    {
        result = [fileManager createDirectoryAtPath:dir_path withIntermediateDirectories:YES attributes:nil error:&error];
        if (nil != error)
        {
            result = NO;
        }

    }
    else if (isDir)
    {
        result = YES;
    }

    return result;
}


- (BOOL)processCertificates
{
	BOOL result = NO;

    // From the roots directory, create the index and table data for the asset
    PSAssetFlags certFlags = isAnchor | hasFullCert;
    NSNumber* flags = [NSNumber numberWithUnsignedLong:certFlags];
    PSCerts* pscerts_roots = [[PSCerts alloc] initWithCertFilePath:self.root_directory withFlags:flags];
	_certRootsData = [[PSCertData alloc] initWithCertificates:pscerts_roots.certs];


    // From the black and gray listed certs create an array of the keys.
	NSMutableArray* gray_certs = [NSMutableArray array];
    certFlags = isGrayListed | hasFullCert;
    flags = [NSNumber numberWithUnsignedLong:certFlags];
    PSCerts* pscerts_gray = [[PSCerts alloc] initWithCertFilePath:self.distrusted_directory withFlags:flags];
    [gray_certs addObjectsFromArray:pscerts_gray.certs];

    _gray_listed_keys = [NSMutableArray array];
    for (PSCert* aCert in gray_certs)
	{
		[_gray_listed_keys addObject:aCert.public_key_hash];
	}

    NSMutableArray* black_certs = [NSMutableArray array];
    certFlags = isBlackListed | hasFullCert;
    flags = [NSNumber numberWithUnsignedLong:certFlags];
    PSCerts* pscerts_black = [[PSCerts alloc] initWithCertFilePath:self.revoked_directory withFlags:flags];
    [black_certs addObjectsFromArray:pscerts_black.certs];

	_blacked_listed_keys = [NSMutableArray array];
	for (PSCert* aCert in black_certs)
	{
		[_blacked_listed_keys addObject:aCert.public_key_hash];
	}

/*
	On iOS the intermediate certs are not used
    certFlags = hasFullCert;
    flags = [NSNumber numberWithUnsignedLong:certFlags];
    pscerts = [[PSCerts alloc] initWithCertFilePath:self.certs_directory withFlags:flags];
    [certs addObjectsFromArray:pscerts.certs];
*/
    // now create the evroots.plist data
    NSMutableDictionary* file_name_to_cert = [NSMutableDictionary dictionary];

    for (PSCert* aCert in pscerts_roots.certs)
    {
        NSString* just_file_name = [aCert.file_path lastPathComponent];
        [file_name_to_cert setObject:aCert forKey:just_file_name];
    }

	if (![self buildEVRootsData:file_name_to_cert])
	{
		NSLog(@"Unable to create the EVPlist data");
	}

    result = YES;
    return result;
}


- (BOOL)outputPlistsToDirectory
{
    BOOL result = NO;
	NSError* error = nil;
	NSString* path_str = nil;

	if (nil != _EVRootsData)
	{
        if (![self ensureDirectoryPath:self.output_directory])
        {
            NSLog(@"Error unable to ensure the output directory!");
			return result;
        }

		NSData* evroots_data = [NSPropertyListSerialization dataWithPropertyList:_EVRootsData
	                            format:NSPropertyListBinaryFormat_v1_0 /*NSPropertyListXMLFormat_v1_0*/ options:0
	                            error:&error];
		if (nil != error)
		{
			NSLog(@"Error converting out the evroot data into data: error %@", error);
			return result;
		}

		path_str = [self.output_directory stringByAppendingPathComponent:@"EVRoots.plist"];
		if (![evroots_data writeToFile:path_str options:0 error:&error])
		{
			NSLog(@"Error writing out the evroot.plist data: error %@", error);
			return result;
		}
	}

    if (nil != _gray_listed_keys)
    {
        NSData* graylist_roots_data = [NSPropertyListSerialization dataWithPropertyList:_gray_listed_keys
                                                                                  format:NSPropertyListBinaryFormat_v1_0 /*NSPropertyListXMLFormat_v1_0*/ options:0
                                                                                   error:&error];
		if (nil != error)
		{
			NSLog(@"Error converting out the gray listed keys into data: error %@", error);
			return result;
		}

		path_str = [self.output_directory stringByAppendingPathComponent:@"GrayListedKeys.plist"];
		if (![graylist_roots_data writeToFile:path_str options:0 error:&error])
		{
			NSLog(@"Error writing out the GrayListedKeys.plist data: error %@", error);
			return result;
		}

    }

	if (nil != _blacked_listed_keys)
	{
		NSData* blacklist_roots_data = [NSPropertyListSerialization dataWithPropertyList:_blacked_listed_keys
	                            format:NSPropertyListBinaryFormat_v1_0 /*NSPropertyListXMLFormat_v1_0*/ options:0
	                            error:&error];
		if (nil != error)
		{
			NSLog(@"Error converting out the blacked listed keys into data: error %@", error);
			return result;
		}

		path_str = [self.output_directory stringByAppendingPathComponent:@"Blocked.plist"];
		if (![blacklist_roots_data writeToFile:path_str options:0 error:&error])
		{
			NSLog(@"Error writing out the BlackListKeys.plist data: error %@", error);
			return result;
		}
	}

	NSData* index_data = _certRootsData.cert_index_data;
	path_str = [self.output_directory stringByAppendingPathComponent:@"certsIndex.data"];
	if (nil != index_data)
	{
		if (![index_data writeToFile:path_str options:0 error:&error])
		{
			NSLog(@"Error writing out the certsIndex data: error %@", error);
			return result;
		}
	}

	NSData* cert_table_data = _certRootsData.cert_table;
	path_str = [self.output_directory stringByAppendingPathComponent:@"certsTable.data"];
    if (nil != cert_table_data)
	{
		if (![cert_table_data writeToFile:path_str options:0 error:&error])
		{
			NSLog(@"Error writing out the certsTable data: error %@", error);
			return result;
		}
	}

    path_str = [self.output_directory stringByAppendingPathComponent:@"AssetVersion.plist"];

    NSFileManager* fileManager = [NSFileManager defaultManager];
    // check to see if the file exists;
    if ([fileManager fileExistsAtPath:path_str])
    {
        if (![fileManager removeItemAtPath:path_str error:&error])
        {
            NSLog(@"Unable to remove the older version of the AssetVersion.plist file!");
            return result;
        }
    }

    if (![[NSFileManager defaultManager] copyItemAtPath:self.version_number_plist_path toPath:path_str error:&error])
    {
        NSLog(@"Error copying over the AssetVersion.plist file: error %@", error);
        return result;
    }

    // Create a Dictionary to hold the escrow certificates and write that to disk
    int numProductionRoots = kNumberOfBaseLineEscrowRoots;
    NSData* productionCerts[numProductionRoots];
    struct RootRecord* pRootRecord = NULL;

    int iCnt;
    for (iCnt = 0; iCnt < numProductionRoots; iCnt++)
    {
        pRootRecord = kBaseLineEscrowRoots[iCnt];
        if (NULL != pRootRecord && pRootRecord->_length > 0 && NULL != pRootRecord->_bytes)
        {
           productionCerts[iCnt] = [NSData dataWithBytes:pRootRecord->_bytes length:pRootRecord->_length] ;
        }
    }
    NSArray* productionCertArray = [NSArray arrayWithObjects:productionCerts count:numProductionRoots];

    NSArray* valueArray = [NSArray arrayWithObjects:productionCertArray, nil];
    NSArray* keyArray = [NSArray arrayWithObjects:(__bridge NSString *)kSecCertificateProductionEscrowKey, nil];
    NSDictionary* escrowCertificates = [NSDictionary dictionaryWithObjects:valueArray forKeys:keyArray];

    
    int numProductionPCSRoots = kNumberOfBaseLinePCSEscrowRoots;
    NSData* productionPCSCerts[numProductionPCSRoots];
    struct RootRecord* pPCSRootRecord = NULL;
    
    for (iCnt = 0; iCnt < numProductionPCSRoots; iCnt++)
    {
        pPCSRootRecord = kBaseLinePCSEscrowRoots[iCnt];
        if (NULL != pPCSRootRecord && pPCSRootRecord->_length > 0 && NULL != pPCSRootRecord->_bytes)
        {
            productionPCSCerts[iCnt] = [NSData dataWithBytes:pPCSRootRecord->_bytes length:pPCSRootRecord->_length] ;
        }
    }
    NSArray* productionPCSCertArray = [NSArray arrayWithObjects:productionPCSCerts count:numProductionPCSRoots];
    
    
    NSArray* valuePCSArray = [NSArray arrayWithObjects:productionPCSCertArray, nil];
    NSArray* keyPCSArray = [NSArray arrayWithObjects:(__bridge NSString *)kSecCertificateProductionPCSEscrowKey, nil];
    NSDictionary* escrowPCSCertificates = [NSDictionary dictionaryWithObjects:valuePCSArray forKeys:keyPCSArray];
    
    NSMutableDictionary *mergedEscrowCertificates = [NSMutableDictionary dictionaryWithDictionary:escrowCertificates];
    [mergedEscrowCertificates addEntriesFromDictionary:escrowPCSCertificates];

    NSData* escrowCertPList = [NSPropertyListSerialization dataWithPropertyList:mergedEscrowCertificates
                                          format:NSPropertyListBinaryFormat_v1_0
                                          options:0
                                           error:&error];
    if (nil != error)
    {
        NSLog(@"Error creating the Escrow certificate Plist: error %@", error);
        return result;
    }

    NSString* outputEscrowFileName = [NSString stringWithFormat:@"%@.plist", (__bridge NSString *)kSecCertificateEscrowFileName];
    path_str = [self.output_directory stringByAppendingPathComponent:outputEscrowFileName];
    if (![escrowCertPList writeToFile:path_str options:0 error:&error])
    {
        NSLog(@"Error writing out the escrow certificate data: error %@", error);
        return result;
    }

    return YES;
}

- (BOOL)createManifest
{
    BOOL result = NO;

    if (nil == self.version_number_plist_path)
    {
        return result;
    }


    unsigned char hash_buffer[CC_SHA256_DIGEST_LENGTH];

    NSString* evroots_str = @"EVRoots.plist";
    NSString* blocked_str = @"Blocked.plist";
    NSString* graylistedkeys_str = @"GrayListedKeys.plist";
    NSString* certsIndex_str = @"certsIndex.data";
    NSString* certsTable_str = @"certsTable.data";
    NSString* assetVersion_str = @"AssetVersion.plist";
    NSString* escrowCertificate_str = [NSString stringWithFormat:@"%@.plist", (__bridge NSString *)kSecCertificateEscrowFileName];
    NSError* error = nil;

    NSInputStream* input_stream = [NSInputStream inputStreamWithFileAtPath:self.version_number_plist_path];
    [input_stream open];
    NSDictionary* version_number_dict = [NSPropertyListSerialization propertyListWithStream:input_stream options:0 format:nil error:&error];
    if (nil != error)
    {
        [input_stream close];
        NSLog(@"Error getting the version number info %@", error);
        return result;
    }
    [input_stream close];
    NSNumber* version_number = [version_number_dict objectForKey:@"VersionNumber"];


    NSArray* file_list = [NSArray arrayWithObjects:evroots_str, blocked_str, graylistedkeys_str, certsIndex_str, certsTable_str, assetVersion_str, escrowCertificate_str, nil];
    NSMutableDictionary* manifest_dict = [NSMutableDictionary dictionary];

    for (NSString* file_path in file_list)
    {
        NSString* full_path = [self.output_directory stringByAppendingPathComponent:file_path];
        NSData* hash_data = [NSData dataWithContentsOfFile:full_path options:0 error:&error];
        if (nil != error)
        {
            NSLog(@"Error getting the data for file %@", file_path);
            return result;
        }
        memset(hash_buffer, 0, CC_SHA256_DIGEST_LENGTH);
        CC_SHA256([hash_data bytes], (CC_LONG)[hash_data length] , hash_buffer);
        NSData* hash_value = [NSData dataWithBytes:hash_buffer length:CC_SHA256_DIGEST_LENGTH];
        [manifest_dict setObject:hash_value forKey:file_path];
    }

    // Add the version number to the manifest dictionary
    if (nil != version_number)
    {
        [manifest_dict setObject:version_number forKey:@"VersionNumber"];
    }

    NSData* manifest_property_list = [NSPropertyListSerialization dataWithPropertyList:manifest_dict format:NSPropertyListBinaryFormat_v1_0 options:0 error:&error];
    if (nil != error)
    {
        NSLog(@"Error converting the manifest_dict into a propertylist data object");
        return result;
    }


    NSString* path_str = [self.output_directory stringByAppendingPathComponent:@"manifest.data"];
    if (![manifest_property_list writeToFile:path_str options:0 error:&error])
    {
        NSLog(@"Error writing out the manifest data: error %@", error);
        return result;
    }
    result = YES;


    return result;
}


@end
