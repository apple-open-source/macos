#include <pwd.h>
#include <unistd.h>


#include <Foundation/Foundation.h>

#define	keylistPath			@"/usr/bin/klist"
#define kKadminPath			@"/usr/sbin/kadmin"
#define kKadminLocalPath	@"/usr/sbin/kadmin.local"
#define	kSetupPath			@"/usr/sbin/krbservicesetup"



char* ReplaceKerberosAddrs( const char *inValue, const char *inOldIP, const char *inNewIP, 
					const char *inOldHostName, const char *inNewHostName )
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSMutableDictionary* dict = NULL;
    NSDictionary* subDict = NULL;
    NSData* tempData;
	NSMutableArray* addressArray;
	NSString* oldIP = [NSString stringWithUTF8String: inOldIP];
	NSString* newIP = [NSString stringWithUTF8String: inNewIP];
	NSString* oldHost = @"";
	NSString* newHost = @"";
	if (inOldHostName != nil)
		oldHost = [NSString stringWithUTF8String: inOldHostName];
	if (inNewHostName != nil)
		newHost = [NSString stringWithUTF8String: inNewHostName];
	BOOL changed = false;
	int index = 0;
	char* returnStr = nil;

//	printf("Entering Kerberos update.  Plist:\n%s\n", inValue);
	tempData = [NSData dataWithBytes: inValue length: strlen(inValue)];
	dict = (NSMutableDictionary*)CFPropertyListCreateFromXMLData( NULL, (CFDataRef)tempData, kCFPropertyListMutableContainersAndLeaves, NULL);
	subDict = [dict objectForKey: @"edu.mit.kerberos"];
	if (subDict == nil) return nil;
	subDict = [subDict objectForKey: @"realms"];
	if (subDict == nil) return nil;

	NSEnumerator* enumerator = [subDict objectEnumerator];
	while (subDict = [enumerator nextObject])
	{
		addressArray = [subDict objectForKey: @"KDC_List"];
		if ((addressArray != nil) && [addressArray isKindOfClass: [NSMutableArray class]])
		{
			//printf("Updating KDC_list\n");
			while (index < [addressArray count])
			{
				NSString* oldValue = [addressArray objectAtIndex: index];
				if ((oldValue == nil) || ![oldValue isKindOfClass: [NSString class]])
					continue;
				if ([oldValue isEqualToString: oldIP])
				{
					[addressArray replaceObjectAtIndex: index withObject: newIP];
					changed = true;
				}
				else if ([oldValue isEqualToString: oldHost])
				{
					[addressArray replaceObjectAtIndex: index withObject: newHost];
					changed = true;
				}
				index++;
			}
		}
	
		addressArray = [subDict objectForKey: @"KADM_List"];
		if ((addressArray != nil) && [addressArray isKindOfClass: [NSMutableArray class]])
		{
			//printf("Updating KADM_List\n");
			index = 0;
			while (index < [addressArray count])
			{
				NSString* oldValue = [addressArray objectAtIndex: index];
				if ((oldValue == nil) || ![oldValue isKindOfClass: [NSString class]])
					continue;
				if ([oldValue isEqualToString: oldIP])
				{
					[addressArray replaceObjectAtIndex: index withObject: newIP];
					changed = true;
				}
				else if ([oldValue isEqualToString: oldHost])
				{
					[addressArray replaceObjectAtIndex: index withObject: newHost];
					changed = true;
				}
				index++;
			}
		}
	}
	
	if (changed)
	{
		NSNumber* genID = [dict objectForKey: @"generationID"];
		genID = [NSNumber numberWithInt: ([genID intValue] + 1)];
		[dict setObject: genID forKey: @"generationID"];
		tempData = (NSData*)CFPropertyListCreateXMLData(NULL, dict);
		returnStr = malloc([tempData length] + 1);
		[tempData getBytes: returnStr];
		returnStr[[tempData length]] = '\0';
		//printf("Have changes, new plist = \n%s\n", returnStr);
	}
	
	[pool release];
	return returnStr;
}

/* for bug #rdar://problem/4379238

	need to get a listing of the keytab entries that we manage
	delete them
	run sso_util configure with the new info.
	
*/

/*
	service types are: "afpserver,ftp,imap,pop,host,HTTP,http,smtp,ssh,smb,CIFS,xmpp,ipp,vpn,xgrid,ldap"
*/


NSArray	*GetKeytabEntries(void)
{
	NSTask			*theTask = [[NSTask	alloc] init];
	NSArray			*theArgs = [NSArray arrayWithObjects: @"-k", nil ];
	NSArray			*theList = nil;
	NSArray			*lineArray = nil;
	NSString		*line = nil;
	NSArray			*parsedLine = nil;
	NSString		*principal = nil;
	NSString		*service = nil;
	NSPipe			*stdOut = [[NSPipe alloc] init];
	NSString		*tmpOutput = nil;
	NSMutableSet	*principalSet = nil;
	NSSet			*serviceSet = nil;
	NSFileHandle	*handle = nil;
	int				status;
	unsigned		index;
	NSRange			range;
	
	
	[theTask setLaunchPath: keylistPath];
	[theTask setArguments: theArgs];
	
	[theTask setStandardOutput: stdOut];
	handle = [stdOut fileHandleForReading];
	
	[theTask launch];
	tmpOutput = [[NSString alloc] initWithData:[handle readDataToEndOfFile] encoding:NSASCIIStringEncoding];

	[theTask waitUntilExit];
	status = [theTask terminationStatus];
 
	[stdOut release];
	
	if (status == 0)
	{
		serviceSet = [NSSet setWithObjects: @"afpserver", @"ftp", @"imap", @"host", @"pop", @"HTTP", @"http", @"smtp", @"ssh", @"smb", @"cifs", @"CIFS", @"xmpp", @"XMPP", @"ipp", @"vpn", @"xgrid", @"ldap", nil];
		lineArray = [tmpOutput componentsSeparatedByString: @"\n"];
		principalSet = [NSMutableSet setWithCapacity: [lineArray count]];
		for(index = 3; index < [lineArray count]; index++)
		{
			
			line = [(NSString *)[lineArray objectAtIndex: index] stringByTrimmingCharactersInSet: [NSCharacterSet whitespaceCharacterSet]];
			//NSLog(@"processing %@", line);
			if((line == nil) || ([line length] == 0))
			{
				continue;
			}
			parsedLine = [line componentsSeparatedByString: @" "];
			if(parsedLine != nil)
			{
				// need to check against our service types
				//NSLog(@"\tparsing as %@", parsedLine);
				principal = [parsedLine objectAtIndex: 1];
				range = [principal rangeOfString:@"/" options:NSLiteralSearch];
				if(range.location != NSNotFound)
				{
					service = [principal substringToIndex: range.location];
					//NSLog(@"\tservice = %@", service);
					if([serviceSet containsObject: service] == YES)
					{
						[principalSet addObject: principal];
					}
				}
			}
		
		}
	
		theList = [principalSet allObjects];
	} else {

	}
	return theList;
}


int	RemovePrincipal(NSString *inPrincipal,  NSString *inRealm, NSString *inAdmin,  UInt8 *inPassword)
{
	NSTask			*theTask = [[NSTask	alloc] init];
	NSMutableArray	*theArgs = [NSMutableArray arrayWithObjects: @"-r", inRealm, nil];
	int				result = 0;
	NSPipe			*stdIn = [[NSPipe alloc] init];
	NSFileHandle 	*handleIn;
	UInt8			bytes[64];
	NSData			*passwordData = nil;
	NSString		*kadminPath = nil;
	
    if (inAdmin == nil)
	{
		kadminPath = kKadminLocalPath;
	}
	else
	{
		kadminPath = kKadminPath;

		[theArgs addObjectsFromArray: [NSArray arrayWithObjects: @"-p", inAdmin, nil]];

		strlcpy((char *)bytes, (const char *)inPassword, sizeof(bytes) - 1);
		strlcat((char *)bytes, "\n", sizeof(bytes));

		passwordData = [NSData dataWithBytes: (void *)bytes length: strlen((const char *)bytes)];

		handleIn = [stdIn fileHandleForWriting];
		[theTask setStandardInput: stdIn];
	}
	
	[theArgs addObjectsFromArray: [NSArray arrayWithObjects: @"-q", [NSString stringWithFormat: @"ktremove %@ all", inPrincipal], nil]];

	[theTask setLaunchPath: kadminPath];
	[theTask setArguments: theArgs];
	
	// launch the task, sleep then if the task is still running, write the password if needed.
	[theTask launch];

	if (inAdmin != nil)
	{
		sleep(1);
		if([theTask isRunning] == YES)
		{
			[handleIn writeData: passwordData];
		}
	}
	
	[theTask waitUntilExit];
	result = [theTask terminationStatus];
	[theTask release];
	[stdIn release];
	
	//NSLog(@"finished with the first operation result = %d", result);
	if(result != 0)
	{
		return result;
	}
	
	// now delete the principal
	theTask = [[NSTask	alloc] init];
	[theArgs replaceObjectAtIndex: [theArgs count] - 1 withObject: [NSString stringWithFormat: @"delprinc -force %@", inPrincipal]];
	stdIn = [[NSPipe alloc] init];
	
	handleIn = [stdIn fileHandleForWriting];	
	[theTask setLaunchPath: kadminPath];
	[theTask setArguments: theArgs];
	[theTask setStandardInput: stdIn];


	// launch the task, sleep then if the task is still running, write the password if needed.
	[theTask launch];
	if (inAdmin != nil)
	{
		sleep(1);
		if([theTask isRunning] == YES)
		{
			[handleIn writeData: passwordData];
		}
	}	

	[theTask waitUntilExit];
	result = [theTask terminationStatus];
	[theTask release];
	//NSLog(@"finished with the second operation result = %d", result);	
	return result;
}


int	RemovePrincipalArray(NSArray *inPrincipalArray, NSString *inRealm, NSString *inAdmin,  UInt8 *inPassword)
{
	unsigned	index = 0;
	NSString	*principal = nil;
	int			result = 0;
	
	for(index = 0; index < [inPrincipalArray count]; index++)
	{
		principal = [inPrincipalArray objectAtIndex: index];
		result = RemovePrincipal(principal, inRealm, inAdmin, inPassword);
		if(result != 0)	break;
	}
	return result;
}



/* 
	create the dictionary of new principals. 
	write it out to a secure temp file
	run krbservicesetup


*/



int	SetupNewConfig(NSArray	*inPrincipals,  NSString *inNewHostName, NSString *inRealm, NSString *inAdmin,  UInt8 *inPassword)
{
	NSTask				*theTask = [[NSTask alloc] init];
	NSArray				*theArgs = nil; 
	unsigned			index = 0;
	int					result = 0;
	NSDictionary		*serviceMap = nil;
	NSMutableDictionary	*theConfig = nil;
	NSMutableDictionary	*tmpDict = nil;
	NSMutableArray		*serviceArray = nil;
	NSString			*tmpString = nil;
	NSString			*serviceType = nil;
	NSString			*newPrincipal = nil;
	char				path[32];
	NSRange				range;

	// map serviceType to service name
	serviceMap = [NSDictionary dictionaryWithObjectsAndKeys: @"afp", @"afpserver", @"ftp", @"ftp", @"pop", @"pop", 
															@"HTTP", @"HTTP", @"http", @"http", @"ldap", @"ldap", 
															@"imap", @"imap", @"smtp", @"smtp", @"ipp", @"ipp",  
															@"xgrid", @"xgrid", @"vpn", @"vpn", @"XMPP", @"XMPP",  
															@"xmpp", @"xmpp", @"smb", @"cifs", @"cifs", @"smb",
															@"ssh", @"host", nil];  
	// create the new config dictionary
	theConfig = [NSMutableDictionary dictionaryWithCapacity:  4];
	serviceArray = [NSMutableArray arrayWithCapacity: [inPrincipals count]];
	[theConfig setObject: serviceArray forKey: @"Services"];
	
	// fill out the service array
	for(index = 0; index < [inPrincipals count]; index++)
	{
		tmpString = [inPrincipals objectAtIndex: index];
		range = [tmpString rangeOfString:@"/" options:NSLiteralSearch];
		
		if(range.location != NSNotFound)
		{
			serviceType = [tmpString substringToIndex: range.location];
			if ([serviceMap objectForKey: serviceType] == nil)
			{
				fprintf(stderr, "serviceType %s not found in serviceMap\n", [serviceType UTF8String]);
			}
			else
			{
				newPrincipal = [NSString stringWithFormat: @"%@/%@@%@", serviceType, inNewHostName, inRealm];
				//NSLog(@"original principal %@ new principal %@", tmpString, newPrincipal);
			
				tmpDict = [NSMutableDictionary dictionaryWithCapacity: 2];
				[tmpDict setObject: newPrincipal forKey: @"servicePrincipal"];
				[tmpDict setObject: [serviceMap objectForKey: serviceType]  forKey: @"serviceType"];
			
				[serviceArray addObject: tmpDict];
			}
		}
	}
	//NSLog(@"the config = %@", theConfig);
	
	// create the setup file in a temp dir
	memcpy(path,"/tmp/configure_XXXXXXXX", 23);
	path[23] = 0;
	if(mkdtemp(path) == NULL)
	{
		fprintf(stderr, "Cannot create temp dir %s errno = %d\n", path, errno);
		return errno;
	}
	
	tmpString = [NSString stringWithFormat: @"%s/setup", path];
	
	// write the config to the file
	
	if([theConfig writeToFile: tmpString atomically: NO] == NO)
	{
		fprintf(stderr, "Cannot create temp file %s/setup\n", path);
		return -1;
	}
	
	if (inAdmin == nil)
	{
		theArgs = [NSArray arrayWithObjects:  @"-r", inRealm, @"-x", @"-f", tmpString, nil];
	}
	else
	{
		theArgs = [NSArray arrayWithObjects:  @"-r", inRealm, @"-a", inAdmin, @"-p", [NSString stringWithUTF8String: (const char *)inPassword], @"-f", tmpString, nil];
	}

	[theTask setLaunchPath: kSetupPath];
	[theTask setArguments: theArgs];
	
	[theTask launch];
	
	[theTask waitUntilExit];
	result = [theTask terminationStatus];
	[theTask release];
	
	//NSLog(@"deleting %@  and removing %s", tmpString, path);
	
	// delete the temp file & dir
	unlink([tmpString UTF8String]);
	rmdir(path);

	return result;
}


/* 
get the list of principals in the keytab
figure out the proper realm from the list of principals in the keytab,
prompt the user for the admin for that realm
if the admin name does not have the principal stuff, add it
prompt the user for the password for that user
remove the principals from the kdc & keytab
run sso_util configure
*/

int	UpdateKerberosPrincipals(const char* node, char  *inNewHostName)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSArray		*principals = GetKeytabEntries();
	NSString	*tmpStr = nil;
	NSString	*realm = nil;
	NSString	*admin = nil;
	int			result = 0;
	char		entryBuffer[64];
	char		*passwd = nil;
	int			len = 0;
	BOOL        useKadminLocal = NO;
	
	memset(entryBuffer, 0 , sizeof(entryBuffer));
	
	useKadminLocal = ((strncmp(node, "/Local", sizeof("/Local") - 1) == 0) ||
					  (strstr(node, "ldapi") != NULL));
	
	// figure out the realm
	if([principals count] < 1)	// nothing to do
	{
		[pool release];
		return 0;
	}

	tmpStr = [principals objectAtIndex: 0];
	realm = [tmpStr substringFromIndex: [tmpStr  rangeOfString:@"@" options:NSLiteralSearch].location+1];
	
	if (!useKadminLocal)
	{
		// get the admin name
		printf("Please enter the name of the administrator account for %s : ", [realm UTF8String]);
		
		if(fgets(entryBuffer, sizeof(entryBuffer), stdin) == nil)
		{
			[pool release];
			return -1;
		}
		// a '\n' char needs to be trimmed
		len = strlen(entryBuffer);
		if(len > 1)
		{
			entryBuffer[len -1] = 0;
		}
		tmpStr = [NSString stringWithUTF8String: entryBuffer];
	
		if([tmpStr  rangeOfString:@"@" options:NSLiteralSearch].location == NSNotFound)	// need to add the realm
		{
			admin = [NSString stringWithFormat: @"%@@%@", tmpStr, realm];
		} else {
			admin = tmpStr;
		}

		// and password
		tmpStr = [NSString stringWithFormat: @"Please enter the password for %@ :", admin];
		passwd = getpass([tmpStr UTF8String]);
		
		if(passwd == nil)
		{
			[pool release];
			return -1;
		}
	}

	// clear out the old
	result = RemovePrincipalArray(principals, realm, admin, (UInt8 *)passwd);
	if(result != 0)
	{
		if (passwd)
		{
			memset(passwd, 0 , _PASSWORD_LEN);
		}
		[pool release];
		return result;
	}
	// now create the new principals & set up the configuration
	//NSLog(@"Creating new Kerberos principals & keytab entries");
	result = SetupNewConfig(principals, [NSString stringWithUTF8String: inNewHostName], realm, admin, (UInt8 *)passwd);
	if (passwd)
	{
		memset(passwd, 0 , _PASSWORD_LEN);
	}
	
	[pool release];
	return result;
}
