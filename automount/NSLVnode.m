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
#import <CoreServices/CoreServices.h>

/* Re-enumerate folders at least once an hour: */

#define NSL_REFRESH_RATE (10*60*60)
#define NSL_ERROR_PERSISTENCE_DURATION 10
#define DONTINVALIDATEINTERMEDIATECONTAINERS 1

extern BOOL doServerMounts;

static String *generateLinkTarget(Vnode *v);
static void setupLink(Vnode *v);

static char gLocalHostName[] = "localhost";

@implementation NSLVnode


- (NSLVnode *)init
{
	[super init];
	
	generation = 0;
	NSLObjectType = kNetworkObjectTypeNone;
	fixedEntry = NO;
	havePopulated = NO;
	beingPopulated = NO;
	mountInProgress = NO;
	currentContentGeneration = 0;
	
	return self;
}



- (NSLVnode *)newNeighborhoodWithName:(String *)neighborhoodNameString neighborhood:(NSLNeighborhood)neighborhood
{
	NSLVnode *v;

	v = [NSLVnode alloc];
	if (v == nil) {
		sys_msg(debug, LOG_ERR, "NSLVnode.newNeighborhoodWithName: Failed to allocate NSLVnode; aborting.");
		goto Error_Exit;
	};
	
	[v init];
	
	[v setName:neighborhoodNameString];
	[v setNSLObject:neighborhood type:kNetworkNeighborhood];
	[v setMap:map];
	[controller registerVnode:v];
	[self addChild:v];

	return v;

Error_Exit:
	if (v) [v release];
	return nil;
}



- (NSLVnode *)newServiceWithName:(String *)newServiceName service:(NSLService)service serviceURL:(char *)serverURL
{
	NSLVnode *v;
	Server *newserver = nil;
	String *serversrc = nil;
	String *serviceURL = nil;

	v = [NSLVnode alloc];
	if (v == nil) {
		sys_msg(debug, LOG_ERR, "NSLVnode.newServerWithName: Failed to allocate NSLVnode; aborting.");
		goto Error_Exit;
	};
	
	[v init];

	if (serverURL) {
		serviceURL = [String uniqueString:serverURL];
		if (serviceURL == nil) {
			sys_msg(debug, LOG_ERR, "NSLVnode.initAsServerWithName: Failed to allocate serviceURL; aborting.");
			goto Error_Exit;
		};
	};
	
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
	
	[v setName:newServiceName];
	[v setServer:newserver];
	[v setSource:serversrc];
	[v setUrlString:serviceURL];
	[v setType:NFLNK];
	[v setMode:01755 | NFSMODE_LNK];

	[v setVfsType:[String uniqueString:"url"]];
	[v setNSLObject:service type:kNetworkServer];
	/* [v setupOptions:opts]; */
	[v setMap:map];
	
	[controller registerVnode:v];
	// sys_msg(debug, LOG_DEBUG, "NSLVnode.populate: Adding new offspring %s (%s)...", [[v name] value], [[v link] value]);
	[self addChild:v];

	/* This relies on the superlink/child/map information to construct its path and must be done last... */
	setupLink(v);
	
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
	
	[v setMode:([v mode] & ~01000)];		/* Turn off the sticky bit: this is not a special mount-trigger link */
	[v setName:newSymlink];
	[v setLink:targetString];
    [v setFixedEntry:YES];
	
	return v;

Error_Exit:
	return nil;
}



- (void)dealloc
{
	[self freeNSLObject];
	
	[super dealloc];
}



- (void)invalidateRecursively:(BOOL)invalidateDescendants;
{
	struct timeval now;
	int offspring_count, neighborhood_offspring;

	[super invalidateRecursively:invalidateDescendants];
	
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
				[offspring invalidateRecursively:YES];
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

	sys_msg(debug, LOG_DEBUG, "NSLVnode.invalidateRecursively: notifying Finder about changes to '%s'...", [[self path] value]);
	FNNotifyByPath([[self path] value], kFNDirectoryModifiedMessage, kNilOptions);
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



- (NetworkObjectType)getobjectType {
	return NSLObjectType;
}



- (void)freeNSLObject {
	switch (NSLObjectType) {
		case kNetworkNeighborhood:
			if (NSLObject.neighborhood) NSLFreeNeighborhood(NSLObject.neighborhood);
			NSLObject.neighborhood = NULL;
			break;
			
		case kNetworkServer:
			if (NSLObject.service) CFRelease(NSLObject.service);
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
	switch (objecttype) {
		case kNetworkNeighborhood:
			NSLObject.neighborhood = NSLCopyNeighborhood((NSLNeighborhood)object);
			break;
		
		case kNetworkServer:
			NSLObject.service =
					(NSLService)CFPropertyListCreateDeepCopy( kCFAllocatorDefault,
																			(CFMutableDictionaryRef)object,
																			kCFPropertyListMutableContainers);
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



- (void)depopulateDescendants:(BOOL)depopulateDescendants destroyEmptyNeighborhoods:(BOOL)destroyEmptyNeighborhoods
{
	int target_offspring = 0;
	NSLVnode *targetVnode = nil;
	BOOL contentsChanged = NO;
	struct timeval now;
	
	sys_msg(debug, LOG_DEBUG, "NSLVnode.depopulate: depopulating neighborhood '%s'...", [[self name] value]);

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
						sys_msg(debug, LOG_DEBUG, "NSLVnode.depopulate: discarding neighborhood '%s'...", [[targetVnode name] value]);
						[controller destroyVnode:targetVnode];
						contentsChanged = YES;
					} else {
						++target_offspring;		/* Skip this entry */
					};
				} else if ([targetVnode getobjectType] == kNetworkServer) {
					if ([targetVnode mounted]) {
						++target_offspring;		/* Skip this entry */
					} else {
						sys_msg(debug, LOG_DEBUG, "NSLVnode.depopulate: discarding server '%s'...", [[targetVnode name] value]);
						[controller destroyVnode:targetVnode];
						contentsChanged = YES;
					};
				} else {
					sys_msg(debug, LOG_DEBUG, "NSLVnode.depopulate: unknown node type ('%s')...", [[targetVnode name] value]);
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
				sys_msg(debug, LOG_DEBUG, "NSLVnode.destroyVnodeGenerationsPriorTo: destroying offspring of '%s'...", [[targetVnode name] value]);
				[targetVnode depopulateDescendants:YES destroyEmptyNeighborhoods:YES];
			};
			sys_msg(debug, LOG_DEBUG, "NSLVnode.destroyVnodeGenerationsPriorTo: destroying '%s'...", [[targetVnode name] value]);
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



- (unsigned long)populateWithNeighborhoodsWithGeneration:(unsigned long)newGeneration
{
	unsigned long entrycount = 0;
	struct SearchResultList neighborhoodContents;
    SearchContext context;
    int result;
    struct SearchResult *resultEntry;
    char *neighborhoodname = NULL;
    long neighborhoodnamelength;
	NSLClientRef automountNSLClientRef = [(NSLMap *)map getNSLClientRef];
	
    INIT_SEARCHRESULTLIST(&neighborhoodContents, NSLObject.neighborhood);
    INIT_SEARCHCONTEXT(&context, automountNSLClientRef, &neighborhoodContents);
    
    result = StartSearchForNeighborhoodsInNeighborhood(NSLObject.neighborhood, &context);
    if (result) {
		sys_msg(debug, LOG_ERR, "NSLVnode.populateWithNeighborhoodsWithGeneration: error %d in SearchForNeighborhoodsInNeighborhood", result);
		goto Error_Exit;
    };
	
	WaitForSearchCompletion(&neighborhoodContents);
    
	(void)NSLDeleteRequest(context.searchRef);

#if 0
	sys_msg(debug, LOG_DEBUG, "NSLVnode.populateWithNeighborhoodsWithGeneration: Search result list at 0x%08lx = { 0x%08lx, 0x%08lx }.",
						(unsigned long)&neighborhoodContents.contentsFound,
						(unsigned long)neighborhoodContents.contentsFound.tqh_first,
						(unsigned long)neighborhoodContents.contentsFound.tqh_last);
#endif
						
    while (!TAILQ_EMPTY(&neighborhoodContents.contentsFound)) {
		String *neighborhoodNameString = nil;
		NSLVnode *v = nil;

		resultEntry = TAILQ_FIRST(&neighborhoodContents.contentsFound);
		
        NSLGetNameFromNeighborhood( resultEntry->result.neighborhood, &neighborhoodname, &neighborhoodnamelength );
		if ((neighborhoodname == NULL) || (neighborhoodnamelength == 0)) {
			sys_msg(debug, LOG_ERR, "NSLVnode.populateWithNeighborhoodsWithGeneration: Failed to get neighborhood name for entry %d; aborting.", entrycount);
			goto Abort_Neighborhood_Entry;
		};
		
		neighborhoodNameString = [String uniqueString:neighborhoodname];
		if (neighborhoodNameString == nil) {
			sys_msg(debug, LOG_ERR, "NSLVnode.populateWithNeighborhoodsWithGeneration: Failed to allocate String 'neighborhoodNameString'; aborting.");
			goto Abort_Neighborhood_Entry;
		}
		
		v = (NSLVnode *)[self lookup:neighborhoodNameString];
		if (v && ([v getobjectType] != kNetworkNeighborhood)) {
			[controller destroyVnode:v];
			v = nil;
		};
		if (v == nil) {
			sys_msg(debug, LOG_DEBUG, "NSLVnode.populateWithNeighborhoodsWithGeneration: Adding neighborhood %s...", neighborhoodname);
			v = [self newNeighborhoodWithName:neighborhoodNameString neighborhood:resultEntry->result.neighborhood];
			if (v == nil) {
				sys_msg(debug, LOG_ERR, "NSLVnode.populateWithNeighborhoodsWithGeneration: Failed to allocate new neighborhood node; aborting.");
				goto Abort_Neighborhood_Entry;
			};
		};
		
		[v setGeneration:newGeneration];
		/* [v setVfsType:??? ]; */
			
		++entrycount;
		
		goto Next_Neighborhood_Entry;

Abort_Neighborhood_Entry:
		/* Release any data structures otherwise permanently incorporated into this new node: */
		
Next_Neighborhood_Entry:
		/* Release any data structures used to construct this new node: */
		if (neighborhoodNameString) [neighborhoodNameString release];
		
		TAILQ_REMOVE(&neighborhoodContents.contentsFound, resultEntry, sibling_link);
		NSLFreeNeighborhood(resultEntry->result.neighborhood);
		free(resultEntry);
    };

Error_Exit:

	return entrycount;
}



- (unsigned long)populateWithServicesWithGeneration:(unsigned long)newGeneration
{
	unsigned long entrycount = 0;
	struct SearchResultList neighborhoodContents;
    SearchContext context;
    int result;
    struct SearchResult *resultEntry;
	CFMutableArrayRef serviceListRef = NULL;
    char servicename[MAXNSLOBJECTNAMELENGTH];
	NSLClientRef automountNSLClientRef = [(NSLMap *)map getNSLClientRef];
    char url[MAXNSLOBJECTNAMELENGTH];
	
    INIT_SEARCHRESULTLIST(&neighborhoodContents, NSLObject.neighborhood);
    INIT_SEARCHCONTEXT(&context, automountNSLClientRef, &neighborhoodContents);
    
	serviceListRef = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	if (serviceListRef == NULL) goto Error_Exit;
	
	CFArrayAppendValue( serviceListRef, CFSTR("afp") );
	CFArrayAppendValue( serviceListRef, CFSTR("smb") );
	CFArrayAppendValue( serviceListRef, CFSTR("cifs") );
	CFArrayAppendValue( serviceListRef, CFSTR("nfs") );
	CFArrayAppendValue( serviceListRef, CFSTR("webdav") );
#if 0
	sys_msg(debug, LOG_DEBUG, "Pre-StartSearchForServicesInNeighborhood: Search result list at 0x%08lx = { 0x%08lx, 0x%08lx }.",
						(unsigned long)&neighborhoodContents.contentsFound,
						(unsigned long)neighborhoodContents.contentsFound.tqh_first,
						(unsigned long)neighborhoodContents.contentsFound.tqh_last);
#endif

    result = StartSearchForServicesInNeighborhood( NSLObject.neighborhood, serviceListRef, &context );
    if (result) {
		sys_msg(debug, LOG_ERR, "NSLVnode.populate: error %d in StartSearchForServicesInNeighborhood", result);
		goto Error_Exit;
    };
	
	WaitForSearchCompletion(&neighborhoodContents);
	
	(void)NSLDeleteRequest(context.searchRef);

#if 0
	sys_msg(debug, LOG_DEBUG, "After search: Search result list at 0x%08lx = { 0x%08lx, 0x%08lx }.",
						(unsigned long)&neighborhoodContents.contentsFound,
						(unsigned long)neighborhoodContents.contentsFound.tqh_first,
						(unsigned long)neighborhoodContents.contentsFound.tqh_last);
#endif

    while (!TAILQ_EMPTY(&neighborhoodContents.contentsFound)) {
		String *servername = nil;
		CFStringRef urlStringRef = NULL;
		CFStringRef serviceNameRef;
		NSLVnode *v = nil;
		
		resultEntry = TAILQ_FIRST(&neighborhoodContents.contentsFound);
        serviceNameRef = GetMainStringFromAttribute( resultEntry->result.service, kNSLNameAttrRef );
		if (serviceNameRef == NULL) {
			sys_msg(debug, LOG_ERR, "NSLVnode.populate: Failed to get serviceNameRef; aborting.");
			goto Abort_Service_Entry;
		};
		
		servicename[0] = (char)0;
		(void)CFStringGetCString(serviceNameRef, servicename, (CFIndex)sizeof(servicename), kCFStringEncodingUTF8);
#if 0
		sys_msg(debug, LOG_DEBUG, "NSLVnode.populate: resultEntry = 0x%08lx; serviceNameRef = 0x%08lx, servicename = '%s'...",
							(unsigned long)resultEntry, (unsigned long)serviceNameRef, servicename);
#endif
		if (servicename[0] == (char)0) {
			sys_msg(debug, LOG_ERR, "NSLVnode.populate: empty servicename string; aborting.");
			goto Abort_Service_Entry;
		};
		
		servername = [String uniqueString:servicename];
		if (servername == nil) {
			sys_msg(debug, LOG_ERR, "NSLVnode.populate: Failed to allocate servername string; aborting.");
			goto Abort_Service_Entry;
		}
		
		v = (NSLVnode *)[self lookup:servername];
		if (v && ([v getobjectType] != kNetworkServer)) {
			[controller destroyVnode:v];
			v = nil;
		};
		if (v == nil) {
			sys_msg(debug, LOG_DEBUG, "NSLVnode.populate: Adding server %s (%s)...", servicename, url);
			
			urlStringRef = GetMainStringFromAttribute( resultEntry->result.service, kNSLURLAttrRef );
			if (urlStringRef) {
				(void)CFStringGetCString(urlStringRef, url, (CFIndex)sizeof(url), kCFStringEncodingUTF8);
			};

			v = [self newServiceWithName:servername service:resultEntry->result.service serviceURL:(urlStringRef ? url : nil)];
			if (v == nil) {
				sys_msg(debug, LOG_ERR, "NSLVnode.populate: Failed to allocate new service node; aborting.");
				goto Abort_Service_Entry;
			};
		};
		
		[v setGeneration:newGeneration];
		
		++entrycount;
		
		goto Next_Service_Entry;

Abort_Service_Entry:
		/* Release any data structures otherwise permanently incorporated into this new node: */
		if (v) [v release];

Next_Service_Entry:
		/* Release any data structures used to construct this new node: */
		if (servername) [servername release];
		
		TAILQ_REMOVE(&neighborhoodContents.contentsFound, resultEntry, sibling_link);
		CFRelease(resultEntry->result.service);
		free(resultEntry);
    };

Error_Exit:

	return entrycount;
}



/*
 * Get sub-neighborhoods and servers for this neighborhood.
 */
- (void)populate
{
	struct timeval now;
	unsigned long entrycount;
	
	if (beingPopulated) return;
	beingPopulated = YES;
	
	if (havePopulated) {
		if (NSL_REFRESH_RATE == 0) goto Std_Exit;		/* No refresh requested */
		
		gettimeofday(&now, NULL);
		if (now.tv_sec < (lastUpdate.tv_sec + NSL_REFRESH_RATE)) goto Std_Exit;
	};
	
	++currentContentGeneration;	/* About to populate with a new generation of descendants */
	sys_msg(debug, LOG_DEBUG, "NSLVnode.populate: populating neighborhood '%s'...", [[self name] value]);
	
	entrycount = [self populateWithNeighborhoodsWithGeneration:currentContentGeneration] +
				 [self populateWithServicesWithGeneration:currentContentGeneration];
	[self destroyVnodeGenerationsPriorTo:currentContentGeneration];
	
	if (entrycount == 1) {
		sys_msg(debug, LOG_DEBUG, "NSLVnode.populate: 1 entry.");
	} else {
		sys_msg(debug, LOG_DEBUG, "NSLVnode.populate: %ld entries.", entrycount);
	};

    gettimeofday(&lastUpdate, NULL);
	
	/* Update this directory's timestamps as well: */
	attributes.atime.seconds = lastUpdate.tv_sec;
	attributes.atime.useconds = lastUpdate.tv_usec;
	attributes.mtime = attributes.atime;
	attributes.ctime = attributes.atime;

    havePopulated = YES;

Std_Exit:
	beingPopulated = NO;
}

- (Vnode *)lookup:(String *)n
{
	[self populate];	/* Look up the directory's contents in NSL if necessary */

	return [super lookup:n];
}

/*
 * called from readdir
 */
- (Array *)dirlist
{
	[self populate];	/* Look up the directory's contents in NSL if necessary */
	
	return [super dirlist];
}

- (int)symlinkWithName:(char *)from to:(char *)to attributes:(struct nfsv2_sattr *)attributes;
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
		int i, count;
		NSLVnode *sub;
		
		count = [subnodes count];
		for (i = 0; i < count; i++)
		{
			sub = [subnodes objectAtIndex:i];
			if ([source equal:[sub name]]) {
				v = sub;
				break;
			};
		};
	};
	if (v) {
		result = NFSERR_EXIST;
		goto Error_Exit;
	};

	v = [self newSymlinkWithName:source target:to];
	result = (v) ? NFS_OK : NFSERR_NXIO;
	
Error_Exit:
	if (source) [source release];
	return result;
}

-(NSLNeighborhood)getNSLNeighborhood
{
	if (NSLObjectType == kNetworkNeighborhood) {
		return NSLObject.neighborhood;
	} else {
		sys_msg(debug, LOG_ERR,
					"NSLVnode.getNSLNeighborhood: type for %s is %d instead of kNetworkNeighborhood (%d)?!",
					[[self name] value], NSLObjectType, kNetworkNeighborhood);
		return NULL;
	};
}

-(NSLService)getNSLService
{
	if (NSLObjectType == kNetworkServer) {
		return NSLObject.service;
	} else {
		sys_msg(debug, LOG_ERR, 
					"NSLVnode.getNSLService: type for %s is %d instead of kNetworkServer (%d)?!",
					[[self name] value], NSLObjectType, kNetworkServer);
		return NULL;
	};
}

- (String *)urlString
{
	if (urlString) {
		return urlString;
	} else {
		NSLService service = [self getNSLService];
		CFStringRef urlStringRef = NULL;
		char urlbuffer[MAXNSLOBJECTNAMELENGTH];

		urlbuffer[0] = (char)0;
		
		if (service == NULL) {
			sys_msg(debug, LOG_ERR, "NSLVnode.urlString: NULL service record for %d?!", [[self name] value]);
			return nil;
		};
		
		urlStringRef = GetMainStringFromAttribute( service, kNSLURLAttrRef );
		if (urlStringRef == NULL) {
			sys_msg(debug, LOG_DEBUG, "NSLVnode.urlString: No URL attribute in service record; calling NSLXResolveService...");
			(void)NSLXResolveService(service);
			urlStringRef = GetMainStringFromAttribute( service, kNSLURLAttrRef );
		};

		if (urlStringRef) {
			(void)CFStringGetCString(urlStringRef, urlbuffer, (CFIndex)sizeof(urlbuffer), kCFStringEncodingUTF8);
		} else {
			CFStringRef attributeStringRef = NULL;
			char attributeCString[64];
			char servicetype[64];
			char hostaddress[MAXNSLOBJECTNAMELENGTH];
			char portnumber[64];
			
			sys_msg(debug, LOG_DEBUG, "NSLVnode.urlString: reconstituting URL for %s...", [[self name] value]);
			
			attributeStringRef = GetMainStringFromAttribute( service, kNSLServiceTypeAttrRef );
			if (attributeStringRef == NULL) {
				sys_msg(debug, LOG_DEBUG, "NSLVnode.urlString: kNSLServiceTypeAttrRef attributeStringRef is NULL; aborting.");
				goto Error_Exit;
			};
			(void)CFStringGetCString(attributeStringRef, servicetype, (CFIndex)sizeof(servicetype), kCFStringEncodingUTF8);
			// sys_msg(debug, LOG_DEBUG, "NSLVnode.urlString: kNSLServiceTypeAttrRef = '%s'...", servicetype);
			if (strcmp(servicetype, "afpovertcp") == 0) {
				strcpy(servicetype, "afp");
			};
			
			attributeStringRef = GetMainStringFromAttribute( service, kNSLIPAddressAttrRef );
			if (attributeStringRef == NULL) {
				sys_msg(debug, LOG_DEBUG, "NSLVnode.urlString: kNSLIPAddressAttrRef attributeStringRef is NULL; aborting.");
				goto Error_Exit;
			};
			(void)CFStringGetCString(attributeStringRef, hostaddress, (CFIndex)sizeof(attributeCString), kCFStringEncodingUTF8);
			// sys_msg(debug, LOG_DEBUG, "NSLVnode.urlString: hostaddress = '%s'...", hostaddress);
			
			attributeStringRef = GetMainStringFromAttribute( service, kNSLPortAttrRef );
			if (attributeStringRef == NULL) {
				sys_msg(debug, LOG_DEBUG, "NSLVnode.urlString: kNSLPortAttrRef attributeStringRef is NULL; aborting.");
				goto Error_Exit;
			};
			(void)CFStringGetCString(attributeStringRef, portnumber, (CFIndex)sizeof(attributeCString), kCFStringEncodingUTF8);
			// sys_msg(debug, LOG_DEBUG, "NSLVnode.urlString: portnumber = '%s'...", portnumber);
			
			snprintf(urlbuffer, sizeof(urlbuffer), "%s://%s:%s/", servicetype, hostaddress, portnumber);
			sys_msg(debug, LOG_DEBUG, "NSLVnode.urlString: reconstituted URL = '%s'...", urlbuffer);
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
		sys_msg(debug, LOG_ERR, "setupLink: Failed to allocate link string for '%s'...", [[v name] value]);
		goto Error_Exit;
	};
	sys_msg(debug, LOG_DEBUG, "setupLink: Targeting '%s' to mount on '%s'...", [[v name] value], [x value]);
	[v setLink:x];
	[x release];

Error_Exit: ;
}
