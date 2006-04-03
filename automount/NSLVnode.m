#import "NSLVnode.h"
#import "Controller.h"
#import "AMString.h"
#import "AMMap.h"
#import "automount.h"
#import "log.h"
#import <syslog.h>
#import <string.h>
#import <stdlib.h>
#import <stdio.h>
#import <netdb.h>
#import "nfs_prot.h"
#import "Server.h"
#import <sys/types.h>
#import <sys/stat.h>
#import <netinet/in.h>
#import <arpa/inet.h>
#import <netdb.h>
#import <unistd.h>
#import <stdlib.h>
#import <string.h>
#import <sys/socket.h>
#import <net/if.h>
#import <sys/ioctl.h>
#import <rpc/types.h>
#import <rpc/xdr.h>
#import <rpc/auth.h>
#import <rpc/clnt.h>
#import <rpc/svc.h>
#import <errno.h>
#import "nfs_prot.h"
#import "automount.h"
#import "log.h"
#import "mount.h"
#import "AMString.h"
#import "NSLUtil.h"
#import "vfs_sysctl.h"
#import <CoreServices/CoreServices.h>
#import <CoreServices/CoreServicesPriv.h>

extern char *gLookupTarget;

/* Re-enumerate folders at least once an hour: */

/* Refresh all directories at least once every 10 hours: */
//#define NSL_REFRESH_RATE (10*60*60)
#define NSL_REFRESH_RATE 0
#define NSL_ERROR_PERSISTENCE_DURATION 10
#define DONTINVALIDATEINTERMEDIATECONTAINERS 1
#define LIMITCHANGENOTIFICATIONS 1
#define COUNTGENERATIONS 1

extern BOOL doServerMounts;

static String *generateLinkTarget(Vnode *v);
static void setupLink(Vnode *v);

static char gLocalHostName[] = "localhost";
static char gInvalidateCue[] = ".._invalidatecache";

static char gAutomountInitializedTagFile[] = "/var/run/automount.initialized";
static BOOL gAutomountInitialized = NO;

extern BOOL gUserLoggedIn;

/*
 * We ignore servers and neighborhoods with these names, as other parts of
 * OS X expect network resource to be put there explicitly by automounter
 * maps.
 */
static const char *ignore_names[] = {
	"Applications",
	"Developer",
	"Library",
	"Servers",
	"Users",
	NULL
};

void NSLVnodeNewSearchResultAlert(SearchContextPtr callContext)
{
	char request_code[1];
	
	request_code[0] = REQ_PROCESS_RESULTS;
	if (gWakeupFDs[1] != -1) {
		(void)write(gWakeupFDs[1], request_code, sizeof(request_code));
	};
}


BOOL AutomounterInitializationComplete( void ) {
	struct stat sb;
	
	if (!gAutomountInitialized && (stat(gAutomountInitializedTagFile, &sb) == 0)) {
		gAutomountInitialized = YES;
	}
	return gAutomountInitialized;
}



@implementation NSLVnode

- (NSLVnode *)init
{
	self = [super init];
	if (self) {
		generation = 0;
		apparentName = NULL;
		NSLObjectType = kNetworkObjectTypeNone;
		fixedEntry = NO;
		censorContents = NO;
		havePopulated = NO;
		
		INIT_SEARCHRESULTLIST(&neighborhoodSearchResults, NSLObject.neighborhood);
		neighborhoodSearchStarted = NO;
		neighborhoodSearchComplete = NO;
		
		INIT_SEARCHRESULTLIST(&servicesSearchResults, NSLObject.neighborhood);
		servicesSearchStarted = NO;
		servicesSearchComplete = NO;
		
		lastNotification.tv_sec = 0;
		lastNotification.tv_usec = 0;
		lastSeen.tv_sec = 0;
		lastSeen.tv_usec = 0;
		currentContentGeneration = 0;
	};
	return self;
}



- (NSLVnode *)newNeighborhoodWithName:(String *)neighborhoodNameString neighborhood:(NSLNeighborhood)neighborhood
{
	NSLVnode *v;

	v = [[NSLVnode alloc] init];
	if (v == nil) {
		sys_msg(debug, LOG_ERR, "NSLVnode.newNeighborhoodWithName: Failed to allocate NSLVnode; aborting.");
		goto Error_Exit;
	};
	
	[v setMap:map];
	[v setApparentName:neighborhoodNameString];
	[v setNSLObject:neighborhood type:kNetworkNeighborhood];
	[controller registerVnode:v];
	[self addChild:v];
	[v deferContentGeneration];		/* Must be called after 'addChild' to ensure proper 'path' */

	return v;

Error_Exit:
	if (v) [v release];
	return nil;
}



- (NSLVnode *)newServiceWithName:(String *)newServiceName service:(NSLServiceRef)service serviceURL:(char *)serverURL
{
	NSLVnode *v;
	Server *newserver = nil;
	String *serversrc = nil;
	String *serviceURL = nil;

	v = [[NSLVnode alloc] init];
	if (v == nil) {
		sys_msg(debug, LOG_ERR, "NSLVnode.newServerWithName: Failed to allocate NSLVnode; aborting.");
		goto Error_Exit;
	};
	
	[v setVfsType:[String uniqueString:"url"]];
	[v setType:NFLNK];
	[v setMode:(S_ISVTX | 0755 | NFSMODE_LNK)];
	[v setMap:map];
	[v setApparentName:newServiceName];

	if (serverURL) {
		serviceURL = [String uniqueString:serverURL];
		if (serviceURL == nil) {
			sys_msg(debug, LOG_ERR, "NSLVnode.initAsServerWithName: Failed to allocate serviceURL; aborting.");
			goto Error_Exit;
		};
	};
	[v setUrlString:serviceURL];
	
	newserver = [controller serverWithName:newServiceName];
	if (newserver == nil) {
		sys_msg(debug, LOG_ERR, "NSLVnode.initAsServerWithName: Failed to allocate controller; aborting.");
		goto Error_Exit;
	};
	
	if ([newserver isLocalHost])
	{
		serversrc = [String uniqueString:"/"];
		[v setMounted:YES];
		[v setFakeMount:YES];
	} else {
		serversrc = [String uniqueString:"*"];
	};
	if (serversrc == nil) {
		sys_msg(debug, LOG_ERR, "NSLVnode.initAsServerWithName: Failed to allocate serversrc; aborting.");
		goto Error_Exit;
	};
	/* [v setupOptions:opts]; */
	[v setServerDepth:0];
	
	[controller registerVnode:v];

	// sys_msg(debug, LOG_DEBUG, "NSLVnode.newServiceWithName: Adding new offspring %s (%s)...", [[v apparentName] value], [[v link] value]);
	
	[self addChild:v];
	
	[v setNSLObject:service type:kNetworkServer];
	[v setServer:newserver];
	[v setSource:serversrc];

	/* Information derived from NSL is not necessarily trustworthy: */
	[v addMntArg:MNT_NOSUID];
	[v addMntArg:MNT_NODEV];
	
	/* This relies on the superlink/child/map information to construct its path and must be done last... */
	if ([[self map] mountStyle] == kMountStyleParallel) setupLink(v);

	return v;
	
Error_Exit:
	if (serversrc) [serversrc release];
	if (newserver) [newserver release];
	if (serviceURL) [serviceURL release];
	if (v) [v release];
	return nil;
}



- (NSLVnode *)newSymlinkWithName:(String *)newSymlink target:(char *)target
{
	String *localLinkSpec;
	String *targetString;
	NSLVnode *v;

	localLinkSpec = [String uniqueString:gLocalHostName];
	if (localLinkSpec == nil) {
		sys_msg(debug, LOG_ERR, "NSLVnode.newSymlinkWithName: Failed to allocate localLinkSpec; aborting.");
		goto Error_Exit;
	};
	
	targetString = [String uniqueString:target];
	if (targetString == nil) {
		sys_msg(debug, LOG_ERR, "NSLVnode.newSymlinkWithName: Failed to allocate targetString; aborting.");
		goto Error_Exit;
	};
	
	v = [self newServiceWithName:localLinkSpec service:nil serviceURL:nil];
	if (v == nil) {
		sys_msg(debug, LOG_ERR, "NSLVnode.newSymlinkWithName: Failed to allocate NSLVnode; aborting.");
		goto Error_Exit;
	};
	
	[v setMode:([v mode] & ~S_ISVTX)];		/* Turn off the sticky bit: this is not a special mount-trigger link */
	[v setApparentName:newSymlink];
	[v setLink:targetString];
	[v setFixedEntry:YES];

	return v;

Error_Exit:
	return nil;
}



- (void)dealloc
{
	[self stopSearchesInProgress];
	[self freeNSLObject];
	if (apparentName) [apparentName release];
	
	[super dealloc];
}



- (void)RequestFinderNotificationFor:(char *)directoryPath using:(CFMessagePortRef)requestMessagePort
{
	CFDataRef notificationRequestData;
	SInt32 result;
	
	if (requestMessagePort == NULL) {
		sys_msg(debug, LOG_DEBUG, "Ignoring request for Finder notification for changes to '%s'...", directoryPath);
		return;
	};
	
	if (lastNotification.tv_sec == attributes.mtime.seconds &&
		lastNotification.tv_usec == attributes.mtime.useconds)
	{
		sys_msg(debug, LOG_DEBUG, "Ignoring duplicate request for Finder notification for '%s'...", directoryPath);
		return;
	}
	
	sys_msg(debug, LOG_DEBUG, "Requesting Finder notification for changes to '%s'...", directoryPath);
	
	notificationRequestData = CFDataCreate(kCFAllocatorDefault, directoryPath, strlen(directoryPath) + 1);
	if (notificationRequestData) {
		result = CFMessagePortSendRequest( requestMessagePort,		/* target message port */
										0,							/* message ID */
										notificationRequestData,	/* data to be sent */
										0,							/* send timeout (Sec.) */
										0,							/* receive timeout (Sec.) */
										NULL,						/* reply mode */
										NULL);						/* return data */
		if (result == kCFMessagePortSuccess) {
			lastNotification.tv_sec = attributes.mtime.seconds;
			lastNotification.tv_usec = attributes.mtime.useconds;
		} else {
			sys_msg(debug, LOG_ERR, "Failed request for Finder notification; result = %d.", result);
		};
		
		CFRelease(notificationRequestData);
	}
}



- (void)considerFinderNotification
{
	if (LIMITCHANGENOTIFICATIONS && (lastNotification.tv_sec == attributes.mtime.seconds)) {
		sys_msg(debug, LOG_DEBUG, "considerFinderNotification: Deferring repeat notification for '%s'...", [[self path] value]);
		/* Set a time for the number of microseconds until mtime would tick over to the next whole second
		   and leave well enough alone for now: */
		ualarm(1000000 - attributes.mtime.useconds, 0);
	} else {
		[self RequestFinderNotificationFor:[[self path] value] using:[(NSLMap *)[self map] notificationMessagePort]];
    }
}


- (void)triggerDeferredNotifications:(SearchContext *)searchContext
{
	if (attributes.mtime.seconds < lastNotification.tv_sec)
	{
		sys_msg(debug, LOG_DEBUG, "triggerDeferredNotifications: mtime < lastNotification for '%s'?", [[self path] value]);
		return;
	}
	
	/*
	 * If the node changed since the last notification was sent, so send another one now.
	 *
	 * Be careful how the times are compared since they are essentially a double-precision number.  The comparison above
	 * means that mtime >= lastNotification.  Without that test, the second comparison below would also need to check
	 * attributes.mtime.seconds == lastNotification.tv_sec.
	 */
	if ((attributes.mtime.seconds > lastNotification.tv_sec) || (attributes.mtime.useconds > lastNotification.tv_usec)) {	
		[self RequestFinderNotificationFor:[[self path] value] using:[(NSLMap *)[self map] notificationMessagePort]];
	}
}



- (void)markDirectoryChanged
{
	[super markDirectoryChanged];
	[self considerFinderNotification];
}



- (void)invalidateRecursively:(BOOL)invalidateDescendants notifyFinder:(BOOL)notifyFinder;
{
	struct timeval now;
	int offspring_count, neighborhood_offspring;

	if (!havePopulated) return;
	
	sys_msg(debug, LOG_DEBUG, "NSLVnode.invalidateRecursively: invalidating '%s'...", [[self path] value]);
	
	offspring_count = [[self children] count];
	neighborhood_offspring = 0;
	
	if (invalidateDescendants) {
		int i;
		NSLVnode *offspring;
		
		for (i = 0; i < offspring_count; ++i) {
			offspring = [[self children] objectAtIndex:i];
			if ([offspring getobjectType] == kNetworkNeighborhood) {
				++neighborhood_offspring;
				[offspring invalidateRecursively:YES notifyFinder:notifyFinder];
			};
		};
	};

#if DONTINVALIDATEINTERMEDIATECONTAINERS
	if ((offspring_count > 0) && (neighborhood_offspring == offspring_count)) {
		/* This object contains only other directories: no need to invalidate it... */
		return;
	};
#endif

	/* This will trigger a refresh on the next reference: */
	lastUpdate.tv_sec = 0;
	lastUpdate.tv_usec = 0;
	
	do {
		gettimeofday(&now, NULL);
	} while ((now.tv_sec == attributes.atime.seconds) && (now.tv_usec == attributes.atime.useconds));
	
	attributes.atime.seconds = now.tv_sec;
	attributes.atime.useconds = now.tv_usec;
	attributes.mtime = attributes.atime;
	attributes.ctime = attributes.atime;

	if (notifyFinder) {
		[self RequestFinderNotificationFor:[[self path] value] using:[(NSLMap *)[self map] notificationMessagePort]];
	};
	
	sys_msg(debug, LOG_DEBUG, "NSLVnode.invalidateRecursively: invalidation of '%s' complete.", [[self path] value]);
}



- (unsigned long)getGeneration
{
	return generation;
}



- (void)setGeneration:(unsigned long)newGeneration
{
	generation = newGeneration;
}






- (String *)apparentName
{
	return apparentName;
}



- (String *)makeExternalName:(String *)n
{
	char *externalName;
	char *cp, *np;
	String *newExternalName;
	
	externalName = (char *)malloc(strlen([n value]) + 1);
	if (externalName == NULL) {
		sys_msg(debug, LOG_ERR, "Failed to allocate memory for external representation of '%s'", [n value]);
		return [n retain];
	};
	
	for (cp = [n value], np = externalName; *cp; cp++) {
		*np++ = (*cp == '/') ? ':' : *cp;
	};
	*np = (char)0;
	sys_msg(debug, LOG_DEBUG, "\tnew external name is '%s'", externalName);
	
	newExternalName = [String uniqueString:externalName];
	free(externalName);
	if (newExternalName == NULL) {
		sys_msg(debug, LOG_ERR, "Failed to allocate memory for external String* representation of '%s'", [n value]);
		newExternalName = [n retain];
	};
	
	return newExternalName;
}



- (void)setApparentName:(String *)n
{
	[n retain];
	[apparentName release];
	apparentName = n;
	
	if (!strchr([n value], '/')) {
		[self setName:n];
	} else {
		String *newExternalName = [self makeExternalName:n];
		[self setName:newExternalName];
		[newExternalName release];
	};
}



- (NetworkObjectType)getobjectType {
	return NSLObjectType;
}



- (void)freeNSLObject {
	switch (NSLObjectType) {
		case kNetworkNeighborhood:
			if (NSLObject.neighborhood) NSLXReleaseNeighborhoodResult(NSLObject.neighborhood);
			NSLObject.neighborhood = NULL;
			break;
			
		case kNetworkServer:
			if (NSLObject.service) NSLXReleaseServiceResult(NSLObject.service);
			NSLObject.service = NULL;
			break;
			
		default:
			break;
	};
	
	NSLObjectType = kNetworkObjectTypeNone;
}



- (void)setNSLObject:(const void *)object type:(NetworkObjectType)objecttype {
	
	[self freeNSLObject];
	
	NSLObjectType = objecttype;
	
	/*
	 * The following code does not copy the object being passed in; instead it is
	 * assumed here that the object being passed in is either persistent without
	 * any refcount from this pointer or else is a copy that will only be freed above
	 * on the next change of NSLObject:
	 */
	switch (objecttype) {
		case kNetworkNeighborhood:
			NSLObject.neighborhood = (NSLNeighborhood)object;
			break;
		
		case kNetworkServer:
			NSLObject.service = (NSLServiceRef)object;
			break;
		
		default:
			break;
	};
}

- (BOOL)fixedEntry
{
	return fixedEntry;
}



- (void)setFixedEntry:(BOOL)newFixedEntryStatus
{
	fixedEntry = newFixedEntryStatus;
}



- (BOOL)censorContents
{
	return censorContents;
}



- (void)setCensorContents:(BOOL)newCensorContentsStatus
{
	censorContents = newCensorContentsStatus;
}



- (void)depopulateDescendants:(BOOL)depopulateDescendants destroyEmptyNeighborhoods:(BOOL)destroyEmptyNeighborhoods
{
	int target_offspring = 0;
	NSLVnode *targetVnode = nil;
	BOOL contentsChanged = NO;
	struct timeval now;
	
	sys_msg(debug, LOG_DEBUG, "NSLVnode.depopulate: depopulating neighborhood '%s'...", [[self apparentName] value]);

	if ([self children]) {
		while ((NSLVnode *)([[self children] objectAtIndex:target_offspring]) != targetVnode) {
			targetVnode = (NSLVnode *)[[self children] objectAtIndex:target_offspring];
			if (targetVnode == nil) {
				++target_offspring;		/* Skip this entry */
			} else {
				if ([targetVnode getobjectType] == kNetworkNeighborhood) {
					if (depopulateDescendants) {
						[targetVnode depopulateDescendants:YES destroyEmptyNeighborhoods:destroyEmptyNeighborhoods];
					};
					if (destroyEmptyNeighborhoods && ([[targetVnode children] count] == 0)) {
						sys_msg(debug, LOG_DEBUG, "NSLVnode.depopulate: discarding neighborhood '%s'...", [[targetVnode apparentName] value]);
						[controller destroyVnode:targetVnode];
						contentsChanged = YES;
					} else {
						++target_offspring;		/* Skip this entry */
					};
				} else if ([targetVnode getobjectType] == kNetworkServer) {
					if ([targetVnode mounted]) {
						++target_offspring;		/* Skip this entry */
					} else {
						sys_msg(debug, LOG_DEBUG, "NSLVnode.depopulate: discarding server '%s'...", [[targetVnode apparentName] value]);
						[controller destroyVnode:targetVnode];
						contentsChanged = YES;
					};
				} else {
					sys_msg(debug, LOG_DEBUG, "NSLVnode.depopulate: unknown node type ('%s')...", [[targetVnode apparentName] value]);
					++target_offspring;		/* Skip this entry */
				};
			};
		};
	};
    
	havePopulated = NO;	/* Reset to trigger a fresh look at things */
	
	if (contentsChanged) {
		do {
			gettimeofday(&now, NULL);
		} while ((now.tv_sec == attributes.atime.seconds) && (now.tv_usec == attributes.atime.useconds));
		
		/* Update this directory's timestamps as well: */
		attributes.atime.seconds = now.tv_sec;
		attributes.atime.useconds = now.tv_usec;
		attributes.mtime = attributes.atime;
		attributes.ctime = attributes.atime;
	};
}



- (void)destroyVnodeGenerationsPriorTo:(unsigned long)currentGeneration
{
	int offspringCount;
	int target_offspring;
	NSLVnode *targetVnode = nil;
	BOOL contentsChanged = NO;
	struct timeval now;
	
	sys_msg(debug, LOG_DEBUG, "NSLVnode.destroyVnodeGenerationsPriorTo: destroying generations before #%ld...", currentGeneration);

	if ([self children]) {
		offspringCount = [[self children] count];
		target_offspring = 0;
		while (target_offspring < offspringCount) {
			targetVnode = (NSLVnode *)[[self children] objectAtIndex:target_offspring];
			if ((targetVnode == nil) ||
                ([targetVnode fixedEntry]) ||
                ([targetVnode getGeneration] >= currentGeneration)) {
				++target_offspring;
				continue;
			};
			
			/* This Vnode belongs to an older generation: */
			if ([targetVnode getobjectType] == kNetworkNeighborhood) {
				sys_msg(debug, LOG_DEBUG, "NSLVnode.destroyVnodeGenerationsPriorTo: destroying offspring of '%s'...", [[targetVnode apparentName] value]);
				[targetVnode depopulateDescendants:YES destroyEmptyNeighborhoods:YES];
			};
			sys_msg(debug, LOG_DEBUG, "NSLVnode.destroyVnodeGenerationsPriorTo: destroying '%s'...", [[targetVnode apparentName] value]);
			[controller destroyVnode:targetVnode];
			--offspringCount;
			contentsChanged = YES;
		};
	};
	
	if (contentsChanged) {
		do {
			gettimeofday(&now, NULL);
		} while ((now.tv_sec == attributes.atime.seconds) && (now.tv_usec == attributes.atime.useconds));
		
		/* Update this directory's timestamps as well: */
		attributes.atime.seconds = lastUpdate.tv_sec;
		attributes.atime.useconds = lastUpdate.tv_usec;
		attributes.mtime = attributes.atime;
		attributes.ctime = attributes.atime;
	};
}



- (Vnode *)lookupWithoutPopulating:(String *)n
{
	return [super lookup:n];
}



- (Vnode *)lookupByApparentNameWithoutPopulating:(String *)n
{	
	if (!strchr([n value], '/')) {
		return [(NSLVnode *)self lookupWithoutPopulating:n];
	} else {
		String *e = [self makeExternalName:n];
		Vnode *v = [(NSLVnode *)self lookupWithoutPopulating:e];
		[e release];
		return v;
	};
}



- (void)stopSearchesInProgress
{
	NSLError error;

	if (neighborhoodSearchStarted) {
		if (gTerminating) {
			sys_msg(debug, LOG_DEBUG, "stopSearchesInProgress: taking no action on neighborhood search during shutdown.");
		} else {
			sys_msg(debug, LOG_DEBUG, "stopSearchesInProgress: stopping neighborhood search in %s...", [[self path] value]);
			error = NSLDeleteRequest(neighborhoodSearchContext.searchRef);
			if (error.theErr) {
				sys_msg(debug, LOG_ERR, "stopSearchesInProgress: error %d canceling ongoing neighborhood search in %s",
											error.theErr, [[self path] value]);
			};
		};
		[(NSLMap *)[self map] deleteSearchInProgress:&neighborhoodSearchContext];
		neighborhoodSearchStarted = NO;
	};

	if (servicesSearchStarted) {
		if (gTerminating) {
			sys_msg(debug, LOG_DEBUG, "stopSearchesInProgress: taking no action on services search during shutdown.");
		} else {
			sys_msg(debug, LOG_DEBUG, "stopSearchesInProgress: stopping services search in %s...", [[self path] value]);
			error = NSLDeleteRequest(servicesSearchContext.searchRef);
			if (error.theErr) {
				sys_msg(debug, LOG_ERR, "stopSearchesInProgress: error %d canceling ongoing services search in %s",
											error.theErr, [[self path] value]);
			};
		};
		[(NSLMap *)[self map] deleteSearchInProgress:&servicesSearchContext];
		servicesSearchStarted = NO;
	};
}



- (int)populateWithNeighborhoodsWithGeneration:(unsigned long)newGeneration completely:(BOOL)waitForSearch
{
	int result;
	
	if (!neighborhoodSearchStarted) {
		INIT_SEARCHCONTEXT(&neighborhoodSearchContext,
						self,
						[(NSLMap *)map getNSLClientRef],
						&neighborhoodSearchResults,
						newGeneration,
						NSLVnodeNewSearchResultAlert,
						self);
		result = StartSearchForNeighborhoodsInNeighborhood(NSLObject.neighborhood, &neighborhoodSearchContext);
		if (result) {
			sys_msg(debug, LOG_ERR, "NSLVnode.populateWithNeighborhoodsWithGeneration: error %d in SearchForNeighborhoodsInNeighborhood", result);
			return result;
		};
	
		[(NSLMap *)[self map] recordSearchInProgress:&neighborhoodSearchContext];
		neighborhoodSearchStarted = YES;
		gettimeofday(&lastUpdate, NULL);
	};
	
	if (waitForSearch) {
		sys_msg(debug, LOG_DEBUG, "Waiting for neighborhood search completion in %s (SearchContext = 0x%x)...",
									[[self path] value], &neighborhoodSearchContext);
		WaitForInitialSearchCompletion(&neighborhoodSearchContext);
		sys_msg(debug, LOG_DEBUG, "Neighborhood search complete in %s (SearchContext = 0x%x)",
									[[self path] value], &neighborhoodSearchContext);
		neighborhoodSearchComplete = YES;
	} else {
		WaitForCachedSearchCompletion(&neighborhoodSearchContext);
	};
	
	return 0;
}



- (unsigned long)populateWithServicesWithGeneration:(unsigned long)newGeneration completely:(BOOL)waitForSearch
{
	if (!servicesSearchStarted) {
		CFMutableArrayRef serviceListRef;
		int result;
	
		serviceListRef = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
		if (serviceListRef == NULL) {
			sys_msg(debug, LOG_ERR, "NSLVnode.populateWithServicesWithGeneration: couldn't allocate serviceListRef?!");
			goto Error_Exit;
		};
		
		CFArrayAppendValue( serviceListRef, CFSTR("afp") );
		CFArrayAppendValue( serviceListRef, CFSTR("smb") );
		CFArrayAppendValue( serviceListRef, CFSTR("cifs") );
		CFArrayAppendValue( serviceListRef, CFSTR("nfs") );
		CFArrayAppendValue( serviceListRef, CFSTR("webdav") );
		CFArrayAppendValue( serviceListRef, CFSTR("ftp") );
		
		INIT_SEARCHCONTEXT(&servicesSearchContext,
						self,
						[(NSLMap *)map getNSLClientRef],
						&servicesSearchResults,
						newGeneration,
						NSLVnodeNewSearchResultAlert,
						self);
	
		result = StartSearchForServicesInNeighborhood( NSLObject.neighborhood, serviceListRef, &servicesSearchContext );
		CFRelease(serviceListRef);
		if (result) {
			sys_msg(debug, LOG_ERR, "NSLVnode.populateWithServicesWithGeneration: error %d in StartSearchForServicesInNeighborhood", result);
			return result;
		};
		
		[(NSLMap *)[self map] recordSearchInProgress:&servicesSearchContext];
		servicesSearchStarted = YES;
		gettimeofday(&lastUpdate, NULL);
	};
	
	if (waitForSearch) {
		sys_msg(debug, LOG_DEBUG, "Waiting for services search completion in %s (SearchContext = 0x%x)...",
									[[self path] value], &servicesSearchContext);
		WaitForInitialSearchCompletion(&servicesSearchContext);
		sys_msg(debug, LOG_DEBUG, "Services search complete in %s (SearchContext = 0x%x)",
									[[self path] value], &servicesSearchContext);
		servicesSearchComplete = YES;
	} else {
		WaitForCachedSearchCompletion(&servicesSearchContext);
	};

Error_Exit:
	return 0;
}



/*
 * Get sub-neighborhoods and servers for this neighborhood.
 */
- (void)populateCompletely:(BOOL)waitForSearchCompletion;
{
	struct timeval now;
	BOOL newGeneration = NO;
	
	if (havePopulated) {
		if (NSL_REFRESH_RATE == 0) goto Std_Exit;		/* No refresh requested */
		
		gettimeofday(&now, NULL);
		if (now.tv_sec < (lastUpdate.tv_sec + NSL_REFRESH_RATE)) goto Std_Exit;
		
		[(NSLVnode *)self stopSearchesInProgress];
#if COUNTGENERATIONS
		++currentContentGeneration;	/* About to populate with a new generation of descendants */
		newGeneration = YES;
#endif
	};
	
#if 0
	sys_msg(debug, LOG_DEBUG, "NSLVnode.populateCompletely(%s): populating neighborhood '%s', generation %d...",
									waitForSearchCompletion ? "YES" : "NO",
									[[self apparentName] value],
									currentContentGeneration);
#else
	sys_msg(debug, LOG_DEBUG, "NSLVnode.populateCompletely(%s): populating neighborhood '%s' looking for '%s'...",
									waitForSearchCompletion ? "YES" : "NO",
									[[self apparentName] value],
									gLookupTarget);
#endif
	
	(void)[self populateWithNeighborhoodsWithGeneration:currentContentGeneration completely:waitForSearchCompletion];
	(void)[self populateWithServicesWithGeneration:currentContentGeneration completely:waitForSearchCompletion];

#if COUNTGENERATIONS
	if (newGeneration) {
		[self destroyVnodeGenerationsPriorTo:currentContentGeneration];
#if 0
		/* Update this directory's timestamps as well: */
		[self markDirectoryChanged];
#endif
	};
#endif
	
    if (neighborhoodSearchComplete && servicesSearchComplete) havePopulated = YES;

Std_Exit: ;
	return;
}



- (void)processAddResult:(struct SearchResult *)resultEntry
			ofType:(NSLResultType)searchType
			withGeneration:(unsigned long)searchGeneration
{
	String *targetNameString = nil;
	NSLVnode *v = nil;
	const char **p;

	if (searchType == kNetworkNeighborhood) {
		/*
			* Add a new neighborhood entry:
			*/
		CFStringRef neighborhoodNameRef=NULL;
		char neighborhoodname[MAXNSLOBJECTNAMELENGTH];
		
		if (resultEntry->result.neighborhood == NULL) {
			sys_msg(debug, LOG_ERR, "NSLVnode.processAddResult: resultEntry->result.neighborhood is NULL; aborting.");
			goto Ignore_Entry;
		};
		neighborhoodNameRef = NSLXCopyNeighborhoodDisplayName( resultEntry->result.neighborhood );
		if (neighborhoodNameRef == NULL) {
			sys_msg(debug, LOG_ERR, "NSLVnode.processAddResult: Failed to get neighborhoodNameRef; aborting.");
			goto Abort_Neighborhood_Entry;
		};
		
		neighborhoodname[0] = (char)0;
		(void)CFStringGetCString(neighborhoodNameRef, neighborhoodname, (CFIndex)sizeof(neighborhoodname), kCFStringEncodingUTF8);
		
		CFRelease(neighborhoodNameRef);
		
		if ([self censorContents]) {
			/*
			 * This is /Network; ignore neighborhoods that have
			 * names that are the same as /Network names reserved
			 * for the system, such as /Network/Library and
			 * /Network/Applications.
			 */
			for (p = &ignore_names[0]; *p != NULL; p++) {
				if (strcmp(neighborhoodname, *p) == 0) {
					/*
					 * This is a /Network name that's
					 * reserved for the system.
					 */
					sys_msg(debug, LOG_ERR,
					    "NSLVnode.processAddResult: Ignoring neighborhood %s - that name is reserved in /Network",
					    neighborhoodname);
					goto Abort_Neighborhood_Entry;
				}
			}
		}

		targetNameString = [String uniqueString:neighborhoodname];
		if (targetNameString == nil) {
			sys_msg(debug, LOG_ERR, "NSLVnode.processAddResult: Failed to allocate String 'targetNameString'; aborting.");
			goto Abort_Neighborhood_Entry;
		}
		
#if 0 /* DEBUG ONLY */
		if (1) {
			char attributeCString[64];
			CFStringRef attributeStringRef = NULL;
			char attributeCString[64];

			attributeStringRef = NSLXNeighborhoodGetAttributeValue( resultEntry->result.neighborhood, kNSLProtocolTypeKey );
			if (attributeStringRef == NULL) {
					sys_msg(debug, LOG_DEBUG, "NSLVnode.processAddResult: kNSLProtocolTypeKey attributeStringRef is NULL.");
					goto skip;
			};
			(void)CFStringGetCString(attributeStringRef, attributeCString, (CFIndex)sizeof(attributeCString), kCFStringEncodingUTF8);
			sys_msg(debug, LOG_DEBUG, "NSLVnode.processAddResult: kNSLProtocolTypeKey for '%s' = '%s'...",
																			neighborhoodname,
																			attributeCString);
skip: ;
		};
#endif

		v = (NSLVnode *)[self lookupByApparentNameWithoutPopulating:targetNameString];
		if (v && ([v getobjectType] != kNetworkNeighborhood)) {
			[controller destroyVnode:v];
			v = nil;
		};
		if (v == nil) {
			sys_msg(debug, LOG_DEBUG, "NSLVnode.processAddResult: Adding neighborhood %s...", neighborhoodname);
			/* Note: newNeighborhoodWithName does not copy the NSL object AGAIN; it just takes over 'ownership' from the caller */
			v = [self newNeighborhoodWithName:targetNameString neighborhood:resultEntry->result.neighborhood];
			if (v == nil) {
				sys_msg(debug, LOG_ERR, "NSLVnode.processAddResult: Failed to allocate new neighborhood node; aborting.");
				goto Abort_Neighborhood_Entry;
			};
			resultEntry->result.neighborhood = nil;		/* NSL object is now 'owned' by newly created Vnode... */
		};
		
		[v setGeneration:searchGeneration];
		/* [v setVfsType:??? ]; */
			
		goto Next_Neighborhood_Entry;

Abort_Neighborhood_Entry:
		/* Release any data structures otherwise permanently incorporated into this new node: */
		
Next_Neighborhood_Entry:
		/* Release any data structures used to construct this new node: */
		if (resultEntry->result.neighborhood) NSLXReleaseNeighborhoodResult(resultEntry->result.neighborhood);
		resultEntry->result.neighborhood = NULL;
	} else if (searchType == kNetworkServer) {
		/*
			* Add a new server entry:
			*/
		CFStringRef serviceNameRef;
		char servicename[MAXNSLOBJECTNAMELENGTH];
		
		if (resultEntry->result.service == NULL) {
			sys_msg(debug, LOG_ERR, "NSLVnode.processAddResult: resultEntry->result.service is NULL; aborting.");
			goto Ignore_Entry;
		};
		serviceNameRef = NSLXCopyServiceDisplayName( resultEntry->result.service );
		if (serviceNameRef == NULL) {
			sys_msg(debug, LOG_ERR, "NSLVnode.processAddResult: Failed to get serviceNameRef; aborting.");
			goto Abort_Service_Entry;
		};
		
		servicename[0] = (char)0;
		(void)CFStringGetCString(serviceNameRef, servicename, (CFIndex)sizeof(servicename), kCFStringEncodingUTF8);
#if 0
		sys_msg(debug, LOG_DEBUG, "NSLVnode.processAddResult: resultEntry = 0x%x; serviceNameRef = 0x%x, servicename = '%s'...",
							(unsigned long)resultEntry, (unsigned long)serviceNameRef, servicename);
#endif
		CFRelease(serviceNameRef);	// release from NSLXCopyServiceDisplayName
		
		if (servicename[0] == (char)0) {
			sys_msg(debug, LOG_ERR, "NSLVnode.processAddResult: empty servicename string; aborting.");
			goto Abort_Service_Entry;
		};
		
		if ([self censorContents]) {
			/*
			 * This is /Network; ignore services that have
			 * names that are the same as /Network names reserved
			 * for the system, such as /Network/Library and
			 * /Network/Applications.
			 */
			for (p = &ignore_names[0]; *p != NULL; p++) {
				if (strcmp(servicename, *p) == 0) {
					/*
					 * This is a /Network name that's
					 * reserved for the system.
					 */
					sys_msg(debug, LOG_ERR,
					    "NSLVnode.processAddResult: Ignoring service %s - that name is reserved in /Network",
					    servicename);
					goto Abort_Neighborhood_Entry;
				}
			}
		}

		targetNameString = [String uniqueString:servicename];
		if (targetNameString == nil) {
			sys_msg(debug, LOG_ERR, "NSLVnode.processAddResult: Failed to allocate targetNameString string; aborting.");
			goto Abort_Service_Entry;
		}
		
#if 0 /* DEBUG ONLY */
		if (1) {
			CFStringRef attributeStringRef = NULL;
			char attributeCString[64];

			attributeStringRef = CopyMainStringFromAttribute( resultEntry->result.service, kNSLProtocolTypeKey );
			if (attributeStringRef == NULL) {
					sys_msg(debug, LOG_DEBUG, "NSLVnode.processAddResult: kNSLProtocolTypeKey attributeStringRef is NULL.");
					goto skip;
			};
			(void)CFStringGetCString(attributeStringRef, attributeCString, (CFIndex)sizeof(attributeCString), kCFStringEncodingUTF8);
			sys_msg(debug, LOG_DEBUG, "NSLVnode.processAddResult: kNSLProtocolTypeKey for '%s' = '%s'...",
																			servicename,
																			attributeCString);
skip: ;
			CFRelease(attributeStringRef);	// from CopyMainStringFromAttribute
		};
#endif
			
		v = (NSLVnode *)[self lookupByApparentNameWithoutPopulating:targetNameString];
		if (v && ([v getobjectType] != kNetworkServer)) {
			[controller destroyVnode:v];
			v = nil;
		};
		if (v == nil) {
			sys_msg(debug, LOG_DEBUG, "NSLVnode.processAddResult: Adding server %s...", servicename);
			
			/* Note: newServiceWithName does not copy the NSL object AGAIN; it just takes over 'ownership' from the caller */
			v = [self newServiceWithName:targetNameString service:resultEntry->result.service serviceURL:nil];	// delay resolution of url
			if (v == nil) {
				sys_msg(debug, LOG_ERR, "NSLVnode.processAddResult: Failed to allocate new service node; aborting.");
				goto Abort_Service_Entry;
			};
			resultEntry->result.service = nil;		/* NSL object is now 'owned' by newly created Vnode... */
		};
		
		[v setGeneration:searchGeneration];
		
		goto Next_Service_Entry;

Abort_Service_Entry:
		/* Release any data structures otherwise permanently incorporated into this new node: */
		if (v) [v release];

Next_Service_Entry:
		/* Release any data structures used to construct this new node: */
		if(resultEntry->result.service) NSLXReleaseServiceResult(resultEntry->result.service);
		resultEntry->result.service = NULL;
	} else {
		sys_msg(debug, LOG_ERR, "NSLVnode.processAddResult: Unknown search target type (%d).", searchType);
	};
				
	if (targetNameString) [targetNameString release];
	targetNameString = nil;

Ignore_Entry: ;
}



- (void)processDeleteResult:(struct SearchResult *)resultEntry ofType:(NSLResultType)searchType
{
	String *targetNameString = nil;
	NSLVnode *v = nil;

	sys_msg(debug_nsl, LOG_DEBUG, "NSLVnode.processDeleteResult: Processing kNSLXDeleteResult request...");
	if (searchType == kNetworkNeighborhood) {
		/*
		 * Delete a neighborhood entry:
		 */
		char *neighborhoodname = NULL;
		long neighborhoodnamelength;
	
		if (resultEntry->result.neighborhood == NULL) {
			sys_msg(debug, LOG_ERR, "NSLVnode.processDeleteResult: resultEntry->result.neighborhood is NULL; aborting.");
			goto Ignore_Entry;
		};
		NSLGetNameFromNeighborhood( resultEntry->result.neighborhood, &neighborhoodname, &neighborhoodnamelength );
		if ((neighborhoodname == NULL) || (neighborhoodnamelength == 0)) {
			sys_msg(debug, LOG_ERR, "NSLVnode.processDeleteResult: Failed to get neighborhood name for entry; aborting.");
			goto Release_Neighborhood;
		};
		
		targetNameString = [String uniqueString:neighborhoodname];
		if (targetNameString == nil) {
			sys_msg(debug, LOG_ERR, "NSLVnode.processDeleteResult: Failed to allocate String 'targetNameString'; aborting.");
			goto Release_Neighborhood;
		}
		
		sys_msg(debug, LOG_DEBUG, "NSLVnode.processDeleteResult: Deleting neighborhood '%s'.", neighborhoodname);
		
Release_Neighborhood:
		/* Release the result data structure: */
		NSLXReleaseNeighborhoodResult(resultEntry->result.neighborhood);
		resultEntry->result.neighborhood = NULL;
	} else if (searchType == kNetworkServer) {
		/*
		 * Delete a server entry:
		 */
		CFStringRef serviceNameRef;
		char servicename[MAXNSLOBJECTNAMELENGTH];
		
		if (resultEntry->result.service == NULL) {
			sys_msg(debug, LOG_ERR, "NSLVnode.processDeleteResult: resultEntry->result.service is NULL; aborting.");
			goto Ignore_Entry;
		};
		serviceNameRef = NSLXCopyServiceDisplayName( resultEntry->result.service );
		if (serviceNameRef == NULL) {
			sys_msg(debug, LOG_ERR, "NSLVnode.processDeleteResult: Failed to get serviceNameRef; aborting.");
			goto Release_Server;
		};
		
		servicename[0] = (char)0;
		(void)CFStringGetCString(serviceNameRef, servicename, (CFIndex)sizeof(servicename), kCFStringEncodingUTF8);
#if 0
		sys_msg(debug, LOG_DEBUG, "NSLVnode.processDeleteResult: resultEntry = 0x%x; serviceNameRef = 0x%x, servicename = '%s'...",
							(unsigned long)resultEntry, (unsigned long)serviceNameRef, servicename);
#endif
		CFRelease(serviceNameRef);		// from NSLXCopyServiceDisplayName
		
		if (servicename[0] == (char)0) {
			sys_msg(debug, LOG_ERR, "NSLVnode.processDeleteResult: empty servicename string; aborting.");
			goto Release_Server;
		};
		
		targetNameString = [String uniqueString:servicename];
		if (targetNameString == nil) {
			sys_msg(debug, LOG_ERR, "NSLVnode.processDeleteResult: Failed to allocate targetNameString string; aborting.");
			goto Release_Server;
		}
		
		sys_msg(debug, LOG_DEBUG, "NSLVnode.processDeleteResult: Deleting server '%s'.", servicename);
		
Release_Server:
		/* Release the result data structure: */
		NSLXReleaseServiceResult(resultEntry->result.service);
		resultEntry->result.service = NULL;
	};
	
	if (targetNameString) {
		v = (NSLVnode *)[self lookupByApparentNameWithoutPopulating:targetNameString];
		if (v) {
			[controller destroyVnode:v];
		};
		[targetNameString release];
	};
Ignore_Entry: ;
}

- (void)processUpdatedResult:(struct SearchResult *)resultEntry ofType:(NSLResultType)searchType
{
	sys_msg(debug_nsl, LOG_DEBUG, "NSLVnode.processUpdatedResult Processing kNSLXResultUpdated request...");
	
	// for either type of result, we should double check to see if its category has changed...
	if (searchType == kNetworkNeighborhood) {
		// Nothing to do - this should be handled entirely inside NSL...
		
		// Release the extra retain we put on this object before it was passed up:
		NSLXReleaseNeighborhoodResult(resultEntry->result.neighborhood);
	} else if (searchType == kNetworkServer) {
		// if we've already cached the url for this service, we want to flush that as the url may have changed
		if (urlString) {
			[urlString release];
			urlString = nil;
		}
		
		// Release the extra retain we put on this object before it was passed up:
		NSLXReleaseServiceResult(resultEntry->result.service);
	};
}

- (BOOL)processSearchResults:(SearchContext *)searchContext
{
    struct SearchResultList *results = searchContext->results;
    struct SearchResult *resultEntry;
	BOOL directoryChanged = NO;

	while (1) {
		pthread_mutex_lock(&results->resultListMutex);
		if (TAILQ_EMPTY(&results->contentsFound)) {
			pthread_mutex_unlock(&results->resultListMutex);
			break;
		};
		resultEntry = TAILQ_FIRST(&results->contentsFound);
		TAILQ_REMOVE(&results->contentsFound, resultEntry, sr_link);
		pthread_mutex_unlock(&results->resultListMutex);
		
		switch (resultEntry->resultType) {
		  case kNSLXAddResult:
			[self processAddResult:resultEntry
					ofType:searchContext->searchTargetType
					withGeneration:searchContext->searchGenerationNumber];
			break;
		  
		  case kNSLXDeleteResult:
			[self processDeleteResult:resultEntry ofType:searchContext->searchTargetType];
			break;
		  
		  case kNSLXResultUpdated:
			[self processUpdatedResult:resultEntry ofType:searchContext->searchTargetType];
			break;
		  
		  case kNSLXPreviousResultsInvalid:
			sys_msg(debug, LOG_ERR, "NSLVnode.processSearchResults: Processing kNSLXPreviousResultsInvalid result...");
			[self invalidateRecursively:YES notifyFinder:YES];
			break;
			
		  default:
			sys_msg(debug, LOG_ERR, "NSLVnode.processSearchResults: Unknown result type (%d).", resultEntry->resultType);
			break;
		}; /* switch (resultEntry->resultType) ... */
		
		directoryChanged = YES;
		
		free(resultEntry);
	};
	
	return directoryChanged;
}



- (Vnode *)lookup:(String *)n
{
	char *target;
	
	[(NSLMap *)[self map] cleanupSearchContextList];

	Vnode *vp = [super lookup:n];
	if (vp) return vp;
	
	target = [n value];
	
	/* Don't bother populating the directory for invisible files... */
	if (target[0] == '.') {
		return nil;
	};
	
	/* Don't bother populating for "Library" or "Developer" until the user has logged in: */
	if (!gUserLoggedIn &&
		((strcmp(target, "Library") == 0) || (strcmp(target, "Developer") == 0))) {
		return nil;
	}
	
	if (strcmp([n value], gInvalidateCue) == 0) {
		/* Invalidate the contents of this directory: */
		[self invalidateRecursively:NO notifyFinder:NO];
		return nil;
	} else {
		if (gUserLoggedIn && AutomounterInitializationComplete()) [self populateCompletely:YES];	/* Look up the directory's contents in NSL if necessary */
		return [super lookup:n];
	};
}

- (void)generateDirectoryContents:(BOOL)waitForSearchCompletion
{
	/* Note the last time this directory's contents were seen: */
	lastSeen.tv_sec = attributes.mtime.seconds;
	lastSeen.tv_usec = attributes.mtime.useconds;

	[self populateCompletely:waitForSearchCompletion];	/* Look up the directory's contents in NSL if necessary */
	
	[(NSLMap *)[self map] cleanupSearchContextList];
}

/*
 * called from readdir
 */
- (Array *)dirlist
{
	[self generateDirectoryContents:NO];
	return [super dirlist];
}

- (int)symlinkWithName:(char *)from to:(char *)to attributes:(struct nfsv2_sattr *)attributes
{
	int result;
	String *source = nil;
	NSLVnode *v = nil;
	
	if (strchr(from, '/')) {
		result = NFSERR_NXIO;
		goto Error_Exit;
	};
	
	source = [String uniqueString:from];
	if (havePopulated) {
		v = (NSLVnode *)[self lookup:source];
	} else {
		v = (NSLVnode *)[super lookup:source];
	}
	if (v) {
		result = NFSERR_EXIST;
		goto Error_Exit;
	}

	v = [self newSymlinkWithName:source target:to];
	result = (v) ? NFS_OK : NFSERR_NXIO;
	
Error_Exit:
	if (source) [source release];
	return result;
}

- (int)remove:(String *)s
{
	NSLVnode *child;

	/* Find the object they're trying to remove */
	if (havePopulated)
		child = (NSLVnode *)[self lookup: s];
	else
		child = (NSLVnode *)[super lookup: s];
	if (child == nil)
		return NFSERR_NOENT;

	/* Only allow removing externally created symlinks */
	if (![child fixedEntry])
		return NFSERR_PERM;
	
	/* Remove it... */
	[controller destroyVnode: child];
	
	return NFS_OK;
}

-(NSLNeighborhood)getNSLNeighborhood
{
	if (NSLObjectType == kNetworkNeighborhood) {
		return NSLObject.neighborhood;
	} else {
		sys_msg(debug, LOG_ERR,
					"NSLVnode.getNSLNeighborhood: type for %s is %d instead of kNetworkNeighborhood (%d)?!",
					[[self apparentName] value], NSLObjectType, kNetworkNeighborhood);
		return NULL;
	};
}

-(NSLServiceRef)getNSLService
{
	if (NSLObjectType == kNetworkServer) {
		return NSLObject.service;
	} else {
		sys_msg(debug, LOG_ERR, 
					"NSLVnode.getNSLService: type for %s is %d instead of kNetworkServer (%d)?!",
					[[self apparentName] value], NSLObjectType, kNetworkServer);
		return NULL;
	};
}

- (String *)urlString
{
	if (urlString) {
		return urlString;
	} else {
		NSLServiceRef service = [self getNSLService];
		CFStringRef urlStringRef = NULL;
		char urlbuffer[MAXNSLOBJECTNAMELENGTH];

		urlbuffer[0] = (char)0;
		
		if (service == NULL) {
			sys_msg(debug, LOG_ERR, "NSLVnode.urlString: NULL service record for %d?!", [[self apparentName] value]);
			return nil;
		};
		
		urlStringRef = NSLXCopyServicePreferredURLResultAsString( service );

		if (urlStringRef) {
			(void)CFStringGetCString(urlStringRef, urlbuffer, (CFIndex)sizeof(urlbuffer), kCFStringEncodingUTF8);
			CFRelease(urlStringRef);
		} else {
			CFStringRef attributeStringRef = NULL;
			char servicetype[64];
			char hostaddress[MAXNSLOBJECTNAMELENGTH];
			char portnumber[64];
			
			sys_msg(debug, LOG_DEBUG, "NSLVnode.urlString: reconstituting URL for %s...", [[self apparentName] value]);
			
			attributeStringRef = CopyMainStringFromAttribute( service, kNSLServiceTypeAttrRef );
			if (attributeStringRef == NULL) {
				sys_msg(debug, LOG_DEBUG, "NSLVnode.urlString: kNSLServiceTypeAttrRef attributeStringRef is NULL; aborting.");
				goto Error_Exit;
			};
			(void)CFStringGetCString(attributeStringRef, servicetype, (CFIndex)sizeof(servicetype), kCFStringEncodingUTF8);
			// sys_msg(debug, LOG_DEBUG, "NSLVnode.urlString: kNSLServiceTypeAttrRef = '%s'...", servicetype);
			if (strcmp(servicetype, "afpovertcp") == 0) {
				strcpy(servicetype, "afp");
			};
			
			CFRelease(attributeStringRef);	// from CopyMainStringFromAttribute

			attributeStringRef = CopyMainStringFromAttribute( service, kNSLIPAddressAttrRef );
			if (attributeStringRef == NULL) {
				sys_msg(debug, LOG_DEBUG, "NSLVnode.urlString: kNSLIPAddressAttrRef attributeStringRef is NULL; aborting.");
				goto Error_Exit;
			};
			(void)CFStringGetCString(attributeStringRef, hostaddress, (CFIndex)sizeof(hostaddress), kCFStringEncodingUTF8);
			// sys_msg(debug, LOG_DEBUG, "NSLVnode.urlString: hostaddress = '%s'...", hostaddress);
			
			CFRelease(attributeStringRef);	// from CopyMainStringFromAttribute

			attributeStringRef = CopyMainStringFromAttribute( service, kNSLPortAttrRef );
			if (attributeStringRef == NULL) {
				sys_msg(debug, LOG_DEBUG, "NSLVnode.urlString: kNSLPortAttrRef attributeStringRef is NULL; aborting.");
				goto Error_Exit;
			};
			(void)CFStringGetCString(attributeStringRef, portnumber, (CFIndex)sizeof(portnumber), kCFStringEncodingUTF8);
			// sys_msg(debug, LOG_DEBUG, "NSLVnode.urlString: portnumber = '%s'...", portnumber);
			
			snprintf(urlbuffer, sizeof(urlbuffer), "%s://%s:%s/", servicetype, hostaddress, portnumber);
			sys_msg(debug, LOG_DEBUG, "NSLVnode.urlString: reconstituted URL = '%s'...", urlbuffer);

			CFRelease(attributeStringRef);	// from CopyMainStringFromAttribute
		};
			
		[self setUrlString:[String uniqueString:urlbuffer]];
	};
	
Error_Exit:
	return urlString;
}

- (void)setNfsStatus:(unsigned int)s
{
	gettimeofday(&ErrorTime, NULL);	/* Mark the time this error was generated */
	[super setNfsStatus:s];
}

- (unsigned int)nfsStatus
{
	if (nfsStatus) {
		struct timeval now;
		
		gettimeofday(&now, NULL);
		if (now.tv_sec >= (ErrorTime.tv_sec + NSL_ERROR_PERSISTENCE_DURATION)) {
			[self setNfsStatus:0];
		};
	};
	
	return nfsStatus;
}

/*
 * Return YES if the node's server may require authentication.
 *
 * This should be lightweight because it is called as the result of
 * an NFS getattr call.  Searching the keychain for an entry
 * corresponding to the server would probably be too costly.
 *
 * Beware that parsing an item's URL can be very expensive, because
 * it may take expensive NSL operations to determine the URL (apparently,
 * some services are initially populated without a URL).
 */
- (BOOL)needsAuthentication
{
	BOOL answer = YES;	/* Assume most NSL mount points require authentication. */
	NSLServiceRef service = NULL;
	CFStringRef serviceType = NULL;
	
	service = [self getNSLService];
	if (service)
		serviceType = NSLXCopyServicePreferredServiceTypeResultAsString( service );

	if (serviceType && CFStringCompare(serviceType, CFSTR("nfs"), kCFCompareCaseInsensitive ) == kCFCompareEqualTo)
		answer = NO;	/* Assume that NFS never requires authentication. */
	
	if (serviceType)
		CFRelease(serviceType);		// from NSLXCopyServicePreferredServiceTypeResultAsString
		
	return answer;
}

- (BOOL)marked
{
	return NO;	/* Cause -[Controller validate] to skip NSLVnodes */
}

- (BOOL)mounted
{
	/*
	 * Pretend that ordinary symlinks are always mounted, so the NFS server
	 * will just return the link without trying to trigger a (non-existent) mount.
	 */

	return ([self fixedEntry]) ? YES : [super mounted];
}

- (BOOL)checkForUnmount
{
	int i, len;
	Array *kids;
	BOOL result = NO;       /* No unmounts found in this hierarchy yet. */
	Vnode *ancestorNode;
	Vnode *serverNode;

	if ([[self map] mountStyle] != kMountStyleParallel) return NO;
	
	kids = [self children];
	len = 0;
	if (kids != nil)
		len = [kids count];

	if (len == 0)
	{
		if (mounted && ![self fakeMount] && [self server] != nil)
		{
			/*
			 * We've found a server marked as mounted.  If
			 * none of its children are mounted any more,
			 * then mark the server unmounted, and return
			 * that we found an unmount.
			 */
			if (![self anyChildMounted:[[self link] value]])
			{
				sys_msg(debug, LOG_DEBUG, "Server %s unmounted", [[self path] value]);
				[self setMounted: NO];
				
				/* Find the most distant ancestor node: */
				serverNode = nil;
				ancestorNode = [self parent];
				while (ancestorNode) {
					if ([ancestorNode server] == [self server]) {
						serverNode = ancestorNode;
					}
					ancestorNode = [ancestorNode parent];
				}
				
				if (serverNode) {
					/* Mark all intervening nodes as unmounted to ensure a re-mount attempt: */
					ancestorNode = [self parent];
					while (ancestorNode) {
						[ancestorNode setMounted:NO];
						ancestorNode = (ancestorNode == serverNode) ? nil : [ancestorNode parent];
					}
				}
				
				result = YES;
			}
		}
	}
	else
	{
		/* Propagate any unmounts from descendants. */
		for (i=0; i<len; ++i)
		{
			if ([[kids objectAtIndex: i] checkForUnmount])
				result = YES;
		}
	}
	
	return result;
}

@end



static String *generateLinkTarget(Vnode *v)
{
	size_t len;
	char *buf;
	String *s;

	len = [[[v map] mountPoint] length] + [[v path] length] + 1;
	buf = malloc(len);
	if (buf == NULL) return nil;
	
	sprintf(buf, "%s%s", [[[v map] mountPoint] value], [[v path] value]);
	s = [String uniqueString:buf];
	free(buf);
	return s;
}



static void setupLink(Vnode *v)
{
	String *x;

	if (v == nil) return;

	if ([[v server] isLocalHost])
	{
		[v setLink:[v source]];
		[v setMode:00755 | NFSMODE_LNK];
		[v setMounted:YES];
		[v setFakeMount:YES];
		return;
	}

	x = generateLinkTarget(v);
	if (x == nil) {
		sys_msg(debug, LOG_ERR, "setupLink: Failed to allocate link string for '%s'...", [[(NSLVnode *)v apparentName] value]);
		goto Error_Exit;
	};
	sys_msg(debug, LOG_DEBUG, "setupLink: Targeting '%s' to mount on '%s'...", [[(NSLVnode *)v apparentName] value], [x value]);
	[v setLink:x];
	[x release];

Error_Exit: ;
}
