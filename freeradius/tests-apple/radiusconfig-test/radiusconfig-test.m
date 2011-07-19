#import <Foundation/Foundation.h>
#import <SystemConfiguration/SystemConfiguration.h>

#import <sys/types.h>
#import <sys/uio.h>
#import <pwd.h>
#import <signal.h>				// for signal handling
#import <string.h>				// for memset
#import <stdlib.h>				// for exit
#import <openssl/rc4.h>
#import <openssl/md5.h>
#import <sysexits.h>

#import <stdio.h>
#import <stdint.h>
#import <paths.h>

#import <time.h>
#import <unistd.h>
#import <regex.h>
#import <arpa/inet.h>
#import <netinet/in.h>
#import <sys/file.h>
#import <sys/stat.h>
#import <sys/socket.h>
#import <sys/sysctl.h>					// for struct kinfo_proc and sysctl()
#import <sys/types.h>
#import <netdb.h>
#import <ifaddrs.h>

#define kMaxPasswordServerWait		30
#define DONT_LOG					-2
#define LOG_ALL						-1

#define kIterCount					100

typedef enum CommandCode {
	kToolStandardTest
} CommandCode;

//--------------------------------------------------------------------------------------------------------
//	PROTOTYPES
//--------------------------------------------------------------------------------------------------------

void usage(const char *toolName);
int parse_args(int argc, const char * argv[], CommandCode *outCommandCode, NSString **outRemoteIPString);
int run(CommandCode commandCode, NSString *remoteIPString);
const char *nextIP(void);
int _runCommand(NSString* inCommandPath, NSArray* inArguments,
				NSString* inString, NSString** outString, NSString** errString);
int _runCommandWithPassword(NSString* inCommandPath, NSArray* inArguments, 
                           int inPasswordIndex, NSString* inString, 
                           NSString** outString, NSString** errString);
void LogStep(const char *message, int lapCount);
void LogMessage(NSString* message);
NSString* _getPrimaryIPv4Address();
BOOL StringIsAnIPAddress(const char *inAddrStr);

//--------------------------------------------------------------------------------------------------------
//	main
//--------------------------------------------------------------------------------------------------------

int main (int argc, const char * argv[])
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
	CommandCode commandCode = kToolStandardTest;
	NSString *remoteIPString = nil;
	
    int exitcode = parse_args(argc, argv, &commandCode, &remoteIPString);
	if (exitcode == EX_OK)
		exitcode = run(commandCode, remoteIPString);
	
	if (exitcode == EX_USAGE)
		usage(argv[0]);
	
    [pool release];
    return exitcode;
}


//--------------------------------------------------------------------------------------------------------
//	usage
//--------------------------------------------------------------------------------------------------------

void usage(const char *toolName)
{
	fprintf(stderr, "Usage:\t %s <remote-server-ip>\n", toolName);
}


//--------------------------------------------------------------------------------------------------------
//	parse_args
//--------------------------------------------------------------------------------------------------------

int parse_args(int argc, const char * argv[], CommandCode *outCommandCode, NSString **outRemoteIPString)
{
	//int argIndex;
	
	if ( outCommandCode == NULL || outRemoteIPString == NULL )
		return EX_SOFTWARE;
	
	*outCommandCode = kToolStandardTest;
	*outRemoteIPString = nil;
	
	return EX_OK;
}


//--------------------------------------------------------------------------------------------------------
//	run
//--------------------------------------------------------------------------------------------------------

int run(CommandCode commandCode, NSString *remoteIPString)
{
	int exitcode;
	int index;
	int taskCode;
	unsigned int randomIPIndex;
	struct timeval startTotal, endTotal;
	struct timeval startStep, endStep;
	struct timezone tz = {0};
	NSMutableArray *usedIPArray = [NSMutableArray arrayWithCapacity:0];
	NSString *ipString;
	NSString *nasListString = nil;
	char usedID[kIterCount + 1] = {0};
	
	gettimeofday(&startTotal, &tz);
	
	for ( index = 1; index <= kIterCount; index++ )
	{
		NSAutoreleasePool *pool = [NSAutoreleasePool new];
		
		gettimeofday(&startStep, &tz);
		
		srandom((unsigned long)startStep.tv_usec);
		taskCode = ((unsigned long)random() % 3);
		switch(taskCode)
		{
			case 0:
			case 1:
				// addclient
				ipString = [NSString stringWithUTF8String:nextIP()];
				exitcode = _runCommand(@"/usr/sbin/radiusconfig",
								[NSArray arrayWithObjects:@"-addclient", ipString, @"shortname", @"secret", nil],
								@"password\n", nil, nil);
				if ( exitcode != EX_OK )
				{
					LogMessage( [NSString stringWithFormat:@"Error: addclient returned %d", exitcode] );
					return EX_SOFTWARE;
				}
				[usedIPArray addObject:ipString];
				break;
				
			case 2:
				// removeclient
				if ([usedIPArray count] == 0)
					continue;
				randomIPIndex = ((unsigned long)random() % [usedIPArray count]);
				ipString = [usedIPArray objectAtIndex:randomIPIndex];
				exitcode = _runCommand(@"/usr/sbin/radiusconfig",
								[NSArray arrayWithObjects:@"-removeclient", ipString, nil],
								nil, nil, nil);
				if ( exitcode != EX_OK )
				{
					LogMessage( [NSString stringWithFormat:@"Error: addclient returned %d", exitcode] );
					return EX_SOFTWARE;
				}
				[usedIPArray removeObjectAtIndex:randomIPIndex];
				break;
		}
		gettimeofday(&endStep, &tz);
		
		LogMessage( [NSString stringWithFormat:@"Completed round %d", index] );
		[pool release];
	}
	
	gettimeofday(&endTotal, &tz);
	LogMessage( [NSString stringWithFormat:@"Net clients added: %d, total time: {%ld,%ld}", [usedIPArray count],
					endTotal.tv_sec - startTotal.tv_sec, endTotal.tv_usec - startTotal.tv_usec] );
	
	// get naslist
	exitcode = _runCommand(@"/usr/sbin/radiusconfig",
								[NSArray arrayWithObject:@"-naslistxml"],
								nil, &nasListString, nil);
	
	// get array from naslist
	NSPropertyListFormat format;
	NSString *errorString = nil;
	NSArray* nasArray = [NSPropertyListSerialization propertyListFromData:
		[nasListString dataUsingEncoding:NSUTF8StringEncoding]
		mutabilityOption:NSPropertyListImmutable format:&format errorDescription:&errorString];
	
	// make a mask array of used nas IDs
	int nasID;
	NSDictionary *nasDict = nil;
	NSEnumerator *enumerator = [nasArray objectEnumerator];
	while ( (nasDict = [enumerator nextObject]) != nil ) {
		nasID = [[nasDict objectForKey:@"id"] intValue];
		if ( nasID > 0 )
			usedID[nasID]++;
	}
	
	// count the gaps in the ID space
	char gaplist[kIterCount * 6] = {0,};
	char duplist[kIterCount * 6] = {0,};
	char buffer[256];
	int gapCount = 0;
	int dupCount = 0;
	int nasCount = [usedIPArray count];
	for ( index = 1; index <= nasCount; index++ ) {
		if ( usedID[index] == 0 ) {
			gapCount++;
			sprintf( buffer, " %d", index );
			strlcat( gaplist, buffer, sizeof(gaplist) );
		}
		else if ( usedID[index] > 1 ) {
			dupCount++;
			sprintf( buffer, " %d", index );
			strlcat( duplist, buffer, sizeof(duplist) );
		}
	}
	
	// report
	LogMessage( [NSString stringWithFormat:@"Number of gaps in the ID space (should be 0): %d", gapCount] );
	if (gapCount > 0)
		LogMessage( [NSString stringWithFormat:@"gaps:%s\n", gaplist] );
	LogMessage( [NSString stringWithFormat:@"Number of duplicates in the ID space (should be 0): %d", dupCount] );
	if (gapCount > 0)
		LogMessage( [NSString stringWithFormat:@"dups:%s", duplist] );
	
	return EX_OK;
}


//--------------------------------------------------------------------------------------------------------
//	nextIP
//--------------------------------------------------------------------------------------------------------

const char *nextIP(void)
{
	static int c = 1;
	static int d = 1;
	static char ipBuffer[256];

	sprintf(ipBuffer, "192.168.%d.%d", c, d);
	if (d == 255) {
		d = 1;
		c++;
	}
	else {
		d++;
	}
	
	return (const char *)ipBuffer;
}


//--------------------------------------------------------------------------------------------------------
//	_runCommand
//--------------------------------------------------------------------------------------------------------

int _runCommand(NSString* inCommandPath, NSArray* inArguments,
				NSString* inString, NSString** outString, NSString** errString)
{
    return _runCommandWithPassword(inCommandPath,inArguments, 
								   LOG_ALL, inString, outString, errString);
}

//--------------------------------------------------------------------------------------------------------
//	_runCommandWithPassword
//--------------------------------------------------------------------------------------------------------

int _runCommandWithPassword(NSString* inCommandPath, NSArray* inArguments, 
                           int inPasswordIndex, NSString* inString, 
                           NSString** outString, NSString** errString)
{
	NSAutoreleasePool *pool = [NSAutoreleasePool new];
	NSTask* aTask = [NSTask new];
	NSPipe* inPipe = nil;
	NSPipe* outPipe = nil;
	NSPipe* errPipe = nil;
	NSMutableData* outData = nil;
	NSMutableData* errData = [NSMutableData dataWithLength:0];
	NSString* aString = nil;
    NSArray* logArguments = nil;
	NSData* availData = nil;
	int result = 0;
	
	//NS_DURING
    if (inPasswordIndex < [inArguments count] && inPasswordIndex >= 0) {
        logArguments = [[inArguments mutableCopy] autorelease];
        [(NSMutableArray*)logArguments replaceObjectAtIndex:inPasswordIndex 
                                                 withObject:@"****"];
    } else if (inPasswordIndex == LOG_ALL) {
        logArguments = inArguments;
    }
    if (logArguments != nil) {
        LogMessage([NSString stringWithFormat:@"command: %@ %@",
                   inCommandPath,[logArguments componentsJoinedByString:@" "]]);
    }
	if (inString != nil) {
		inPipe = [NSPipe new];
		[aTask setStandardInput:inPipe];
	}
	if (outString != nil) {
		outPipe = [NSPipe new];
		[aTask setStandardOutput:outPipe];
		outData = [NSMutableData dataWithLength:0];
	}
	
	errPipe = [NSPipe new];
	if (errPipe != nil)
		[aTask setStandardError:errPipe];
	[aTask setLaunchPath:inCommandPath];
	[aTask setArguments:inArguments];
	[aTask launch];
	if (inString != nil) {
		[[inPipe fileHandleForWriting] writeData:[inString dataUsingEncoding:NSUTF8StringEncoding]];
		[[inPipe fileHandleForWriting] closeFile];
	}
	
	// loop and clean clogged pipes
	while ( [aTask isRunning] )
	{
		// poll every 2ms
		usleep(2000);
		
		NS_DURING
		if (outString != nil) {
			availData = [[outPipe fileHandleForReading] availableData];
			if (availData != nil)
				[outData appendData:availData];
		}
		if (errPipe != nil) {
			availData = [[errPipe fileHandleForReading] availableData];
			if (availData != nil)
				[errData appendData:availData];
		}
		NS_HANDLER
		NS_ENDHANDLER
	}
	
	// check exit code
	result = [aTask terminationStatus];
	[errData appendData:[[errPipe fileHandleForReading] availableData]];
	if ([errData length] > 0) {
		aString = [[NSString alloc] initWithData:errData encoding:NSUTF8StringEncoding];
		if ([aString length] > 0) {
			LogMessage([NSString stringWithFormat:@"%@ command output:\n%@",
					   [inCommandPath lastPathComponent],aString]);
		}
	}
	if (result != 0) {
		LogMessage([NSString stringWithFormat:@"%@ command failed with status %d",
			[inCommandPath lastPathComponent],result]);
	}
	if (outString != nil) {
		[outData appendData:[[outPipe fileHandleForReading] availableData]];
	}
/*
	NS_HANDLER
	LogMessage([NSString stringWithFormat:@"%@ command failed2 with exception %@",
		[inCommandPath lastPathComponent],localException]);
	if (result == 0) {
		result = -1;
	}
	NS_ENDHANDLER
*/
	if (errString != nil && aString != nil) {
		*errString = aString;
	}
	if (outString != nil && [outData length] > 0) {
		*outString = [[NSString alloc] initWithData:outData encoding:NSUTF8StringEncoding];
	}
	
	[aTask release];
	[inPipe release];
	[outPipe release];
	[errPipe release];
	
	[pool release];
	if (outString != nil)
		[*outString autorelease];
	if (errString != nil && *errString != nil)
		[*errString autorelease];
	
	return result;
}


//--------------------------------------------------------------------------------------------------------
//	LogStep
//--------------------------------------------------------------------------------------------------------

#define kDashStr	"------------------------------------------------------------"
void LogStep(const char *message, int lapCount)
{
	int idx, len = strlen(message);
	int statusStrLen = 0;
	int prefixSpaces = 0;
	char statusStr[256];
	
	if (lapCount > 0) {
		statusStrLen = sprintf(statusStr, "Round %d", lapCount);
		prefixSpaces = (MAX(sizeof(kDashStr) - 1, len) / 2) - (statusStrLen / 2);
	}
	
	printf("\n");
	printf(kDashStr);
	for (idx = sizeof(kDashStr) - 1; idx < len; idx++)
		printf("-");
	printf("\n");
	
	if (lapCount > 0) {
		while (prefixSpaces-- > 0)
			printf(" ");
		printf("%s\n\n", statusStr);
	}
	
	printf("%s\n", message);
	printf(kDashStr);
	for (idx = sizeof(kDashStr) - 1; idx < len; idx++)
		printf("-");
	printf("\n");
}


//--------------------------------------------------------------------------------------------------------
//	LogMessage
//--------------------------------------------------------------------------------------------------------

void LogMessage(NSString* message)
{
	if ([message hasSuffix:@"\n"]) {
		message = [message substringToIndex:[message length] - 1];
	}
	fprintf(stderr,"%s\n",[message UTF8String]);
}

//--------------------------------------------------------------------------------------------------------
//	 _getPrimaryIPv4Address
//--------------------------------------------------------------------------------------------------------

NSString* _getPrimaryIPv4Address()
{
	SCDynamicStoreRef session = NULL;
	session = SCDynamicStoreCreate(NULL, (CFStringRef)@"slapconfig", NULL, NULL);
	if (session == NULL)
		return nil;

	NSString* primaryIntKey = (NSString*)SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
		(CFStringRef)kSCDynamicStoreDomainState, (CFStringRef)kSCEntNetIPv4); 
	[primaryIntKey autorelease];
	if (primaryIntKey == nil)
		return nil;

	NSDictionary* primaryIntDict = (NSDictionary*)SCDynamicStoreCopyValue(session, (CFStringRef)primaryIntKey);
	CFRelease(session);
	session = NULL;
	[primaryIntDict autorelease];
	if( primaryIntDict == nil || ![primaryIntDict isKindOfClass:[NSDictionary class]] )
		return nil;

	NSString* interfaceName = (NSString*)CFDictionaryGetValue((CFDictionaryRef)primaryIntDict,
		kSCDynamicStorePropNetPrimaryInterface);
	if (interfaceName == nil)
		return nil;
	const char* interfaceNameCStr = [interfaceName UTF8String];

	NSString* primaryIPV4Address = nil;
	struct ifaddrs *ifap = NULL;
	if (getifaddrs(&ifap) != 0)
		return nil;

	struct ifaddrs *ifa;
	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next)
	{
		char host[NI_MAXHOST];
		char serv[NI_MAXSERV];
		if (ifa->ifa_name == NULL) continue;
		if (ifa->ifa_addr == NULL) continue;
		if (strncmp(ifa->ifa_name, "lo", 2) == 0) continue;
		if (ifa->ifa_addr->sa_family == AF_INET ||
			ifa->ifa_addr->sa_family == AF_INET6)
		if( strcmp( ifa->ifa_name, interfaceNameCStr ) == 0 )
		{
			if (getnameinfo(ifa->ifa_addr, ifa->ifa_addr->sa_len,
				host, sizeof(host), serv, sizeof(serv),
				NI_NUMERICHOST | NI_NUMERICSERV) == 0)
			{
				NSString* ipAddress = [NSString stringWithUTF8String: host];

				/* Skip link-local IPv6 addresses */
				if (ifa->ifa_addr->sa_family == AF_INET6 &&
					[ipAddress hasPrefix: @"fe80:"]) continue;
				
				primaryIPV4Address = ipAddress;
				break;
			}
			continue;
		}
	}
	freeifaddrs(ifap);

	return primaryIPV4Address;
}


//--------------------------------------------------------------------------------------------------------
//	StringIsAnIPAddress()
//--------------------------------------------------------------------------------------------------------

BOOL StringIsAnIPAddress(const char *inAddrStr)
{
	char ptonResult[sizeof(struct in_addr) + 500];
	
	return (inet_pton(AF_INET, inAddrStr, &ptonResult) == 1);
}


