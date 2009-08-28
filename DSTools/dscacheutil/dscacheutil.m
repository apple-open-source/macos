/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#import <OpenDirectory/NSOpenDirectory.h>
#import <Foundation/Foundation.h>
#import <DSlibinfoMIG.h>
#import <netdb.h>
#import <unistd.h>
#import <pwd.h>
#import <grp.h>
#import <fstab.h>
#import <arpa/inet.h>
#import <sysexits.h>
#import <membership.h>

char	*gProgName	= NULL;

id getCacheState( NSString *inKey )
{
	NSPropertyListFormat    format			= NSPropertyListXMLFormat_v1_0;
	ODNode					*odNode			= [ODNode nodeWithSession: [ODSession defaultSession] name: @"/Cache" error: nil];
	NSDictionary			*details		= [odNode nodeDetailsForKeys: [NSArray arrayWithObject: inKey] error: nil];
	NSString				*detailsString	= [[details objectForKey: inKey] lastObject];
	id						cacheInfo		= nil;
	
	if ( detailsString != nil )
	{
		NSData *tempData = [detailsString dataUsingEncoding: NSUTF8StringEncoding];
		
		cacheInfo = [NSPropertyListSerialization propertyListFromData: tempData
											         mutabilityOption: NSPropertyListMutableContainersAndLeaves
														       format: &format
											         errorDescription: nil];
		
		// change dates to strings
		if ( [cacheInfo isKindOfClass:[NSDictionary class]] )
		{
			for ( NSMutableDictionary *entry in [cacheInfo objectForKey: @"Entries"] )
			{
				id bestBefore = [entry objectForKey: @"Best Before"];
				id lastAccess = [entry objectForKey: @"Last Access"];
				
				if ( bestBefore != nil )
					[entry setObject: [bestBefore descriptionWithCalendarFormat: @"%m/%d/%y %H:%M:%S" timeZone: nil locale: nil] forKey: @"Best Before"];
				
				if ( lastAccess != nil )
					[entry setObject: [lastAccess descriptionWithCalendarFormat: @"%m/%d/%y %H:%M:%S" timeZone: nil locale: nil] forKey: @"Last Access"];
			}
		}
	}
	else
	{
		fprintf( stderr, "Unable to get details from the cache node\n" );
	}
	
	return cacheInfo;
}

void print_stringlist( const char *inTitle, char **inList )
{
	if ( inList != NULL && (*inList) != NULL )
	{
		printf( "%s", inTitle );
		while ( (*inList) != NULL )
		{
			printf( "%s ", (*inList) );
			inList++;
		}
		printf( "\n" );
	}
}

void print_passwd( struct passwd *entry )
{
	if ( entry != NULL )
	{
		if ( entry->pw_name != NULL )
			printf( "name: %s\n", entry->pw_name );
		if ( entry->pw_passwd != NULL )
			printf( "password: %s\n", entry->pw_passwd );
		printf( "uid: %d\n", entry->pw_uid );
		printf( "gid: %d\n", entry->pw_gid );
		if ( entry->pw_dir != NULL )
			printf( "dir: %s\n", entry->pw_dir );
		if ( entry->pw_shell != NULL )
			printf( "shell: %s\n", entry->pw_shell );
		if ( entry->pw_gecos != NULL )
			printf( "gecos: %s\n", entry->pw_gecos );
		printf( "\n" );
	}
}

void print_group( struct group *entry )
{
	if ( entry != NULL )
	{
		if ( entry->gr_name != NULL )
			printf( "name: %s\n", entry->gr_name );
		if ( entry->gr_passwd != NULL )
			printf( "password: %s\n", entry->gr_passwd );
		printf( "gid: %d\n", entry->gr_gid );
		
		print_stringlist( "users: ", entry->gr_mem );
		printf( "\n" );
	}
}

void print_service( struct servent *entry )
{
	if ( entry != NULL )
	{
		if ( entry->s_name != NULL )
			printf( "name: %s\n", entry->s_name );
		
		print_stringlist( "aliases: ", entry->s_aliases );
		
		if ( entry->s_proto != NULL )
			printf( "protocol: %s\n", entry->s_proto );
		
		printf( "port: %d\n", ntohs(entry->s_port) );
		
		printf( "\n" );
	}
}

void print_rpc( struct rpcent *entry )
{
	if ( entry != NULL )
	{
		if ( entry->r_name != NULL )
			printf( "name: %s\n", entry->r_name );
		
		print_stringlist( "aliases: ", entry->r_aliases );
		
		printf( "number: %d\n", entry->r_number );
		
		printf( "\n" );
	}
}

void print_proto( struct protoent *entry )
{
	if ( entry != NULL )
	{
		if ( entry->p_name != NULL )
			printf( "name: %s\n", entry->p_name );
		
		print_stringlist( "aliases: ", entry->p_aliases );
		
		printf( "number: %d\n", entry->p_proto );
		
		printf( "\n" );
	}
}

void print_mount( struct fstab *entry )
{
	if ( entry != NULL )
	{
		if ( entry->fs_spec != NULL )
			printf( "name: %s\n", entry->fs_spec );
		
		if ( entry->fs_file != NULL )
			printf( "file: %s\n", entry->fs_file );

		if ( entry->fs_vfstype != NULL )
			printf( "vfstype: %s\n", entry->fs_vfstype );

		if ( entry->fs_mntops != NULL )
			printf( "options: %s\n", entry->fs_mntops );
		
		if ( entry->fs_type != NULL )
			printf( "type: %s\n", entry->fs_type );
		
		if ( entry->fs_vfstype != NULL )
			printf( "vfstype: %s\n", entry->fs_vfstype );
		
		printf( "frequency: %d\n", entry->fs_freq );
		printf( "pass: %d\n", entry->fs_passno );
		
		printf( "\n" );
	}
}

void print_hostent( struct hostent *entry )
{
	if ( entry != NULL )
	{
		if ( entry->h_name != NULL )
			printf( "name: %s\n", entry->h_name );
		
		print_stringlist( "alias: ", entry->h_aliases );
		
		char	ipStr[INET6_ADDRSTRLEN];
		char	**addrPtr;
		char	*addressName = (entry->h_addrtype == AF_INET ? "ip_address" : "ipv6_address");
		
		for ( addrPtr = entry->h_addr_list; (*addrPtr) != NULL; addrPtr++ ) {
			if ( inet_ntop(entry->h_addrtype, (*addrPtr), ipStr, sizeof(ipStr)) )
				printf( "%s: %s\n", addressName, ipStr );
		}

		printf( "\n" );
	}
}

int query( char *inQueryCategory, char *inQueryKey, char *inQueryValue )
{
	if ( strcasecmp(inQueryCategory, "host") == 0 )
	{
		if ( inQueryKey == NULL )
		{
			struct hostent *entry;
			sethostent( 1 );
			
			while ( (entry = gethostent()) != NULL )
				print_hostent( entry );
			
			endhostent();
			return 0;
		}
		else if ( inQueryValue != NULL )
		{
			if ( strcasecmp(inQueryKey, "name") == 0 )
			{
				print_hostent( gethostbyname2(inQueryValue, AF_INET6) );
				print_hostent( gethostbyname2(inQueryValue, AF_INET) );
			}
			else if ( strcasecmp(inQueryKey, "ip_address") == 0 )
			{
				unsigned char	buffer[16];
				uint32_t		tempFamily  = AF_UNSPEC;

				if( inet_pton(AF_INET6, inQueryValue, buffer) == 1 )
					tempFamily = AF_INET6;
				if( tempFamily == AF_UNSPEC && inet_pton(AF_INET, inQueryValue, buffer) == 1 )
					tempFamily = AF_INET;
				
				if ( tempFamily != AF_UNSPEC )
					print_hostent( gethostbyaddr( buffer, sizeof(buffer), tempFamily) );
				else
					fprintf( stderr, "Invalid address provided\n" );
			}
		}
		else
		{
			fprintf( stderr, "query failed!\n" );
		}
	}
	else if ( strcasecmp(inQueryCategory, "user") == 0 )
	{
		if ( inQueryKey == NULL )
		{
			struct passwd *entry;
			setpwent();
			
			while ( (entry = getpwent()) != NULL )
				print_passwd( entry );
			
			endpwent();
		}
		else if ( inQueryValue != NULL )
		{
			if ( strcasecmp(inQueryKey, "name") == 0 )
				print_passwd( getpwnam(inQueryValue) );
			else if ( strcasecmp(inQueryKey, "uid") == 0 )
				print_passwd( getpwuid(atoi(inQueryValue)) );
		}
		else
		{
			fprintf( stderr, "query failed!\n" );
		}
	}
	else if ( strcasecmp(inQueryCategory, "group") == 0 )
	{
		if ( inQueryKey == NULL )
		{
			struct group *entry;
			setgrent();
			
			while ( (entry = getgrent()) != NULL )
				print_group( entry );
			
			endgrent();
		}
		else if ( inQueryValue != NULL )
		{
			if ( strcasecmp(inQueryKey, "name") == 0 )
				print_group( getgrnam(inQueryValue) );
			else if ( strcasecmp(inQueryKey, "gid") == 0 )
				print_group( getgrgid(atoi(inQueryValue)) );
		}
		else
		{
			fprintf( stderr, "query failed!\n" );
		}
	}
	else if ( strcasecmp(inQueryCategory, "service") == 0 )
	{
		if ( inQueryKey == NULL )
		{
			struct servent *entry;
			setservent( 1 );
			
			while ( (entry = getservent()) != NULL )
				print_service( entry );
			
			endservent();
		}
		else if ( inQueryValue != NULL )
		{
			if ( strcasecmp(inQueryKey, "name") == 0 )
				print_service( getservbyname(inQueryValue, NULL) );
			else if ( strcasecmp(inQueryKey, "port") == 0 )
				print_service( getservbyport(htons(atoi(inQueryValue)), NULL) );
		}
		else
		{
			fprintf( stderr, "query failed!\n" );
		}
	}
	else if ( strcasecmp(inQueryCategory, "rpc") == 0 )
	{
		if ( inQueryKey == NULL )
		{
			struct rpcent *entry;
			setrpcent( 1 );
			
			while ( (entry = getrpcent()) != NULL )
				print_rpc( entry );
			
			endrpcent();
		}
		else if ( inQueryValue != NULL )
		{
			if ( strcasecmp(inQueryKey, "name") == 0 )
				print_rpc( getrpcbyname(inQueryValue) );
			else if ( strcasecmp(inQueryKey, "number") == 0 )
				print_rpc( getrpcbynumber( atoi(inQueryValue)) );
		}
		else
		{
			fprintf( stderr, "query failed!\n" );
		}
	}
	else if ( strcasecmp(inQueryCategory, "protocol") == 0 )
	{
		if ( inQueryKey == NULL )
		{
			struct protoent *entry;
			setprotoent( 1 );
			
			while ( (entry = getprotoent()) != NULL )
				print_proto( entry );
			
			endprotoent();
		}
		else if ( inQueryValue != NULL )
		{
			if ( strcasecmp(inQueryKey, "name") == 0 )
				print_proto( getprotobyname(inQueryValue) );
			else if ( strcasecmp(inQueryKey, "number") == 0 )
				print_proto( getprotobynumber( atoi(inQueryValue)) );
		}
		else
		{
			fprintf( stderr, "query failed!\n" );
		}
	}
	else if ( strcasecmp(inQueryCategory, "mount") == 0 )
	{
		if ( inQueryKey == NULL )
		{
			struct fstab *entry;
			setfsent();
			
			while ( (entry = getfsent()) != NULL )
				print_mount( entry );
			
			endfsent();
		}
		else if ( inQueryValue != NULL )
		{
			if ( strcasecmp(inQueryKey, "name") == 0 )
				print_mount( getfsspec(inQueryValue) );
		}
		else
		{
			fprintf( stderr, "query failed!\n" );
		}
	}
	
	return 0;
}

int flushcache( void )
{
	char					reply[16384]	= { 0, };
    mach_msg_type_number_t	replyCnt		= 0;
    vm_offset_t				ooreply			= 0;
    mach_msg_type_number_t	ooreplyCnt		= 0;
	int32_t                 procno			= 0;
	security_token_t        userToken;
	mach_port_t				serverPort;
	
	if ( bootstrap_look_up(bootstrap_port, kDSStdMachDSLookupPortName, &serverPort) == KERN_SUCCESS )
	{
		if( libinfoDSmig_GetProcedureNumber(serverPort, "_flushcache", &procno, &userToken) == KERN_SUCCESS )
		{
			libinfoDSmig_Query( serverPort, procno, "", 0, reply, &replyCnt, &ooreply, &ooreplyCnt, &userToken );
			return 0;
		}
	}
	
	fprintf( stderr, "Flushcache failed, unable to talk to daemon\n" );
	
	return EIO;
}

void printDictFormatted( const char *inTitle, const char *inIndent, NSDictionary *inDictionary )
{
	NSArray	*allKeys = [[inDictionary allKeys] sortedArrayUsingSelector: @selector(caseInsensitiveCompare:)];
	
	// determine the max key length
	int	maxLen = 0;
	
	for ( NSString *key in allKeys )
	{
		int tempLen = strlen( [key UTF8String] );
		if ( tempLen > maxLen )
			maxLen = tempLen;
	}
	
	// now print them all out
	printf( "%s%s\n", inIndent, inTitle );
	
	for ( NSString *key in allKeys )
	{
		const char	*keyStr	= [key UTF8String];
		id			value	= [inDictionary objectForKey: key];
		int			iPadLen	= maxLen - strlen( keyStr );
		int			ii;
		char		padStr[maxLen + 1];
		
		if ( [value isKindOfClass: [NSArray class]] || [value isKindOfClass: [NSDictionary class]] )
			continue;
		
		memset( padStr, ' ', iPadLen );
		padStr[iPadLen] = '\0';
		
		printf( "%s    %s%s  - %s\n", inIndent, keyStr, padStr, ([value isKindOfClass:[NSString class]] ? [value UTF8String] : [[value stringValue] UTF8String]) );
	}
	
	printf( "\n" );
}

void printFormattedCacheEntry( char *formatString, NSDictionary *inEntry )
{
	// first print the high level info, then below it the keys
	NSString	*theType	= [inEntry objectForKey: @"Type"];
	NSString	*bestBefore = [inEntry objectForKey: @"Best Before"];
	NSString	*lastAccess	= [inEntry objectForKey: @"Last Access"];
	NSString	*hits		= [inEntry objectForKey: @"Hits"];
	NSString	*refs		= [inEntry objectForKey: @"Reference Count"];
	NSString	*ttl		= [inEntry objectForKey: @"TTL"];
	NSNumber	*negative	= [inEntry objectForKey: @"Negative Entry"];
	NSString	*node		= [inEntry valueForKeyPath: @"Validation Information.Name"];
	
	printf( formatString, [theType UTF8String], [bestBefore UTF8String], [lastAccess UTF8String], [hits UTF8String], 
		   [refs UTF8String], [ttl UTF8String], ([negative boolValue] ? "YES" : ""), (node ? [node UTF8String] : "") );
	
	for (NSString *key in [inEntry objectForKey: @"Keys"])
	{
		printf( "    %20s: %s\n", "Key", [key UTF8String] );
	}
	
	printf( "\n" );
}

int cacheDump( char *inCategory, BOOL inBuckets, BOOL inEntries, BOOL isAdmin )
{
	NSMutableDictionary *cacheInfo		= getCacheState( (inEntries ? @"dsAttrTypeNative:LibinfoCacheDetails" : @"dsAttrTypeNative:LibinfoCacheOverview") );
	NSArray				*cacheEntries	= [cacheInfo objectForKey: @"Entries"];
	NSArray				*bucketInfo		= [cacheInfo objectForKey: @"Bucket Info"];
	NSDictionary		*entrySummary	= [cacheInfo objectForKey: @"Counts By Type"];
	
	[cacheInfo removeObjectsForKeys: [NSArray arrayWithObjects: @"Bucket Info", @"Entries", @"Default TTL", @"Hash Slots", @"Max Bucket Depth", 
																@"Policy Flags", @"Cache Cap", @"Counts By Type", nil]];
	
	if ( cacheInfo != nil )
	{
		printDictFormatted( "DirectoryService Cache Overview:", "", cacheInfo );
		printDictFormatted( "Entry count by category:", "    ", entrySummary );
		
		if ( inBuckets == YES && [bucketInfo count] != 0 )
		{
			printf( "Bucket Use Information:\n\n" );
			printf( "    Bucket  Depth    Bucket  Depth    Bucket  Depth    Bucket  Depth\n" );
			printf( "    ------  -----    ------  -----    ------  -----    ------  -----\n" );
			
			int iCount = 0;
			for ( NSDictionary *bucket in bucketInfo )
			{
				printf( "    %6d  %5d", [[bucket objectForKey: @"Bucket"] intValue], [[bucket objectForKey: @"Depth"] intValue] );
				
				if ( ++iCount == 4 )
				{
					printf( "\n" );
					iCount = 0;
				}
			}
			
			if ( iCount < 4 )
				printf( "\n\n" );
			else
				printf( "\n" );
		}
		
		if ( inEntries == YES && [cacheEntries count] != 0 )
		{
			NSString	*theType = (inCategory != NULL ? [NSString stringWithUTF8String: inCategory] : nil);
			char		*formatString	= "    %10s  %18s  %18s  %8s  %6s  %8s  %5s  %s\n";
			
			printf( "Cache entries (ordered as stored in the cache):\n\n" );
			printf( formatString, "Category", "Best Before", "Last Access", "Hits", "Refs", "TTL", "Neg", "DS Node" );
			printf( formatString, "----------", "------------------", "------------------", "--------", "------", "--------", "-----", "---------" );

			for (NSDictionary *entry in cacheEntries)
			{
				if ( theType != nil && [theType caseInsensitiveCompare: [entry objectForKey: @"Type"]] != NSOrderedSame )
					continue;
				
				// skip host entries if not admin
				if ( [[entry objectForKey: @"Type"] caseInsensitiveCompare: @"Host"] == NSOrderedSame && isAdmin == NO )
					continue;

				printFormattedCacheEntry( formatString, entry );
			}
		}
	}
	
	return 0;
}

int dumpStatistics( void )
{
	NSArray				*cacheInfo		= getCacheState( @"dsAttrTypeNative:Statistics" );
	
	if ( [cacheInfo count] > 0 )
	{
		NSMutableDictionary	*globalStats	= [cacheInfo objectAtIndex: 0];
		
		[globalStats removeObjectForKey: @"Category"];
		
		printDictFormatted( "Overall Statistics:", "", globalStats );
	}
	
	if ( [cacheInfo count] > 1 )
	{
		printf( "Statistics by procedure:\n\n" );
		printf( "             Procedure   Cache Hits   Cache Misses   External Calls\n" );
		printf( "    ------------------   ----------   ------------   --------------\n" );
		
		for ( NSMutableDictionary *stats in [[cacheInfo objectAtIndex: 1] objectForKey: @"subValues"] )
		{
			NSString	*total		= [stats objectForKey: @"Total Calls"];
			
			if ( [total intValue] != 0 )
			{
				NSString	*category	= [stats objectForKey: @"Category"];
				NSString	*misses		= [stats objectForKey: @"Cache Misses"];
				NSString	*hits		= [stats objectForKey: @"Cache Hits"];

				printf( "    %18s   %10s   %12s   %14s\n", [category UTF8String], [hits UTF8String], [misses UTF8String], [total UTF8String] );
			}
		}
	}
	
	printf( "\n" );
	
	return 0;
}

int dumpConfiguration( void )
{
	// here we just dump the search policy and some minor details about the cache
	NSError	*error	= nil;
	ODNode	*odNode = [ODNode nodeWithSession: [ODSession defaultSession] type: kODNodeTypeAuthentication error: &error];
	
	if ( odNode != nil )
	{
		NSArray *searchPolicy = [odNode subnodeNamesAndReturnError: &error];
		
		if ( searchPolicy != nil )
		{
			printf( "DirectoryService Cache search policy:\n" );
			
			for ( NSString *value in searchPolicy )
				printf( "    %s\n", [value UTF8String] );
			
			printf( "\n" );
		}
		else
		{
			fprintf( stderr, "Unable to open Search node to get search policy - %s\n", [[error description] UTF8String] );
		}
	}
	else
	{
		fprintf( stderr, "Unable to open Search node to get search policy - %s\n", [[error description] UTF8String] );
	}
	
	NSMutableDictionary *cacheInfo = getCacheState( @"dsAttrTypeNative:LibinfoCacheOverview");

	if ( cacheInfo != nil )
	{
		[cacheInfo removeObjectsForKeys: [NSArray arrayWithObjects: @"Buckets Used", @"Cache Size", @"Max Bucket Depth", @"Counts By Type", nil]];
		
		printDictFormatted( "Settings:", "", cacheInfo );
	}
	else
	{
		fprintf( stderr, "Unable to get cache configuration information\n" );
	}

	return 0;
}

void usage( void )
{
	printf( "Usage: dscacheutil -h\n"
		    "       dscacheutil -q category [-a key value]\n"
			"       dscacheutil -cachedump [-buckets] [-entries [category]]\n"
			"       dscacheutil -configuration\n"
			"       dscacheutil -flushcache\n"
			"       dscacheutil -statistics\n" );
}

int main( int argc, char *argv[] )
{
	int		ch;
	int		ii;
	BOOL	bQuery			= NO;
	BOOL	bCacheDump		= NO;
	BOOL	bEntries		= NO;
	BOOL	bBuckets		= NO;
	char	*pQueryCategory	= NULL;
	char	*pQueryKey		= NULL;
	char	*pQueryValue	= NULL;
	char	*pCategory		= NULL;

	// first get our program name
	char *slash = strrchr( argv[0], '/' );
	if ( slash != NULL )
		gProgName = ++slash;
	else
		gProgName = argv[0];
	
	while ( (ch = getopt(argc, argv, "dDq:a:hs:c:f:b:e:")) != -1 )
	{
		switch (ch)
		{
			// these are lookupd options we don't support
			case 'd':
			case 'D':
				if ( strcmp(gProgName, "lookupd") == 0 )
				{
					printf( "Lookupd no longer exists in this version of the OS. Only a limited number of\n"
						    "legacy options are available.  Please use dscacheutil as the lookupd tool\n"
						    "may go away in the future.\n\n" );
				}
				usage();
				return EX_USAGE;
			case 'e':
				if ( strcmp(optarg, "ntries") != 0 )
				{
					usage();
					return EX_USAGE;
				}

				if ( argv[optind] != NULL && argv[optind][0] != '-' )
				{
					pCategory = argv[optind];
					optind++;
				}
				bEntries = YES;
				break;
			case 'b':
				if ( strcmp(optarg, "uckets") != 0 )
				{
					usage();
					return EX_USAGE;
				}
				bBuckets = YES;
				break;
			case 'c':
				if ( strcmp(optarg, "achedump") == 0 )
				{
					bCacheDump = YES;
					continue;
				}
				else if ( strcmp(optarg, "onfiguration") == 0 )
					return dumpConfiguration();
				usage();
				return EX_USAGE;
			case 'f':
				if ( strcmp(optarg, "lushcache") != 0 )
				{
					usage();
					return EX_USAGE;
				}
				return flushcache();
			case 'q':
				bQuery = YES;
				pQueryCategory = optarg;
				break;
			case 'a':
				pQueryKey = optarg;
				if ( argv[optind] == NULL || argv[optind][0] == '-' )
				{
					usage();
					return EX_USAGE;
				}
				pQueryValue = argv[optind];
				optind++;
				break;
			case 's':
				if ( strcmp(optarg, "tatistics") != 0 )
				{
					usage();
					return EX_USAGE;
				}
				return dumpStatistics();
			case 'h':
				usage();
				return 0;
			case '?':
				return EX_USAGE;
			default:
				break;
		}
	}
	
	if ( bQuery == YES )
		return query( pQueryCategory, pQueryKey, pQueryValue );
	else if ( bCacheDump == YES ) {
		
		int isAdmin = 0;
		
		// require user to be part of admin group to view host lookups
		if ( (pCategory == NULL || strcasecmp(pCategory, "host") == 0) && geteuid() != 0 ) {
			uuid_t userUU;
			
			mbr_uid_to_uuid( getuid(), userUU );
			if ( mbr_check_membership_by_id(userUU, 80, &isAdmin) == 0 && isAdmin == 0 && pCategory != NULL ) {
				// TODO: we could prompt user for username/password of an admin user, but not really necessary
				printf( "Viewing host entries requires administrator privileges.\n" );
				return EX_NOPERM;
			}
		}

		return cacheDump( pCategory, bBuckets, bEntries, isAdmin );
	}
	
	usage();
	
	return EX_USAGE;
}
