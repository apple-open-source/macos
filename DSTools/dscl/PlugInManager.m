#import "PlugInManager.h"

#import <DSObjCWrappers/DSObjCWrappers.h>
#import "NSStringEscPath.h"

#import "dsclPlugInHelper.h"

#import "PathRecord.h"
#import "PathDirService.h"
#import "PathNode.h"
#import "PathRecordType.h"

static NSString* kPlugInFolders[] = { @"~/Library/DirectoryServices/dscl/", @"/System/Library/DirectoryServices/dscl/", nil };
static NSString* kPlugInSuffix = @"dsclext";

@protocol DSCLPlugIn
-(void) processArguments:(char**) argv numArgs:(int) argc info:(DSCLPlugInHelper*) helper returningStatus:(int*) statusP wasHandled:(BOOL*) handledP;
-(void) dumpHelpToFile:(FILE*) fp forCommand:(NSString*) cmd;
@end
 
//--------------------------------------------------------------------------------

static NSArray* GetPlugins()
{
	static NSArray*	gPlugins = nil;		// will contain a list of top-level classes of the plugins, not a list of NSBundles
	
	// If the plugins have already been loaded, just return the list
	if (gPlugins)
		return gPlugins;
		
	// Build a list and assign it immediately to the global.  That way if we get any error, we won't try building the list again
	NSMutableArray* list = [[NSMutableArray arrayWithCapacity:1] retain];
	gPlugins = list;
	
	// Look for and load any plugins
	NSMutableArray*	listOfAlreadyLoaded = [NSMutableArray arrayWithCapacity:0];		// keep track of the names of plugins we've already loaded
	NSString**		baseFolderP = kPlugInFolders;
	while (*baseFolderP)
	{
		NSString*		baseFolder = [*baseFolderP stringByExpandingTildeInPath];
		baseFolderP++;
				
		NSArray* 		dirContents = [[NSFileManager defaultManager] directoryContentsAtPath: baseFolder];
		unsigned int	numInDir = [dirContents count];
		unsigned int	i;
		
		for (i=0; i<numInDir; i++)
		{
			NSString*	name = [dirContents objectAtIndex:i];
			NSString*	suffix = [name pathExtension];
			if ([suffix isEqualToString: kPlugInSuffix] == false)		// make sure this is a bundle type we know about
				continue;
			
			// Check to see if we've already loaded a plugin with this name.  This is to allow overrides to be placed in the home directory
			// that will take precedence over like-named plugins in the system /Library directory
			if ([listOfAlreadyLoaded containsObject: name])
			{
				// NSLog(@"Skipping '%@' in '%@' because a plugin with that name was already loaded.", name, baseFolder);
				continue;
			}
			
			NSString* 	fullPath = [baseFolder stringByAppendingPathComponent: name];
			NSBundle*	bundle = [NSBundle bundleWithPath:fullPath];
			if (bundle == nil)
			{
				NSLog(@"Unable to generate bundle for: %@", fullPath);
				continue;
			}
			
			if ([bundle load] == false)
			{
				NSLog(@"Unable to load bundle for: %@", fullPath);
				continue;
			}
			
			id topObj = [[[bundle principalClass] alloc] init];
			if (topObj == nil)
			{
				NSLog(@"Unable to instantiate principalClass for: %@", fullPath);
				continue;
			}
			
			[list addObject:topObj];
			[listOfAlreadyLoaded addObject: name];
		}
	}
	
	return gPlugins;
}

//--------------------------------------------------------------------------------

@implementation DSCLPlugInHelper

//--------------------------------------------------------------------------------

-(id) init
{
	if ((self = [super init]) == nil)
		return nil;
		
		
	return self;
}

//--------------------------------------------------------------------------------

-(void) dealloc
{
	[fEngine release];
	
	[super dealloc];
}

//--------------------------------------------------------------------------------

-(BOOL) isInteractive
{
	return fInteractive;
}

//--------------------------------------------------------------------------------

-(BOOL) rootIsDirService
{
	// Purpose of this method is so that plugin can determine whether the top level item
	// in the path is a directory service or a node.  In the former case, paths should be
	// of the form "/LDAPv3/127.0.0.1/Users" while in the latter, they should just be "/Users"
	
	id lastObj = [[fEngine stack] lastObject];
	if (lastObj == nil)
		return NO;
	
	return [lastObj isKindOfClass:[PathDirService class]];
}

//--------------------------------------------------------------------------------

-(void) currDirRef:(tDirReference*) dirRef nodeRef:(tDirNodeReference*) nodeRef recType:(NSString**) recType recName:(NSString**) recName
{
	*dirRef = 0;
	*nodeRef = 0;
	*recType = nil;
	*recName = nil;
	
	id lastObj = [[fEngine stack] lastObject];
	if (lastObj == nil)
		return;
	
	/*
		Hierarchy of stack objects appears to be:
			PathDirService		()
			PathNode			(NetInfo/root)		or, 1 each for (LDAPv3) and (127.0.0.1)
			PathRecordType		(Users)
			PathRecord			(mcxtest)
	*/
	
	DSoDirectory* 	dir = nil;
	DSoNode*		node = nil;
	
	if ([lastObj isKindOfClass:[PathDirService class]])
	{
		PathDirService*	obj = (PathDirService*) lastObj;
		
		dir = [obj directory];									// via DSHacks.mm
	}
	else if ([lastObj isKindOfClass:[PathNode class]])
	{
		PathNode*	obj = (PathNode*) lastObj;
		
		node = [obj node];										// via DSHacks.mm
		dir  = [node directory];
	}
	else if ([lastObj isKindOfClass:[PathRecordType class]])
	{
		PathRecordType*		obj = (PathRecordType*) lastObj;
		
		node = [obj node];										// via DSHacks.mm
		dir  = [node directory];

		*recType	= [obj recordType];							// via DSHacks.mm
		*recName	= nil;
	}
	else if ([lastObj isKindOfClass:[PathRecord class]])
	{
		PathRecord*	pathRec = (PathRecord*) lastObj;
		DSoRecord*	rec		= [pathRec record];					// via DSHacks.mm
		
		node = [rec node];
		dir  = [node directory];

		const char* cRecType = [rec getType];
		
		*recType 	= cRecType ? [NSString stringWithUTF8String: cRecType] : nil;		// don't really expect cRecType to be nil here
		*recName 	= [rec getName];
	}
	
	if (dir)
		*dirRef = [dir verifiedDirRef];		// ?? or just -[dsDirRef] ??
		
	if (node)
		*nodeRef = [node dsNodeReference];
}

//--------------------------------------------------------------------------------

-(void) backupStack
{
	[fEngine backupStack];
}

//--------------------------------------------------------------------------------

-(void) restoreStack
{
	[fEngine restoreStack];
}

//--------------------------------------------------------------------------------

-(void) cd:(NSString*) path
{
	[fEngine cd:path];
}

//--------------------------------------------------------------------------------

-(NSString*) cwdAsDisplayString
{
	return [[fEngine cwd] unescapedString];
}

//--------------------------------------------------------------------------------

-(NSString*) getStackDescription
{
	// For debugging only
	return [[fEngine stack] description];
}

//--------------------------------------------------------------------------------

@end	// DSCLPlugInHelper

@interface DSCLPlugInHelper(Private)
-(void) setEngine:(PathManager*) engine setInteractive:(BOOL) interactive;
@end

//--------------------------------------------------------------------------------

@implementation DSCLPlugInHelper(Private)
-(void) setEngine:(PathManager*) engine setInteractive:(BOOL) interactive
{
	fInteractive = interactive;
	
	if (fEngine != engine)
	{
		[fEngine release];
		fEngine = [engine retain];
	}
}
@end	// DSCLPlugInHelper(Private)

//--------------------------------------------------------------------------------

bool dscl_PlugInDispatch(int argc, char* argv[], BOOL interactive, u_int32_t dsid, PathManager* engine, tDirStatus* status)
{
	BOOL handled = false;
	*status = eDSNoErr;
	
	NS_DURING
		// Try dispatching the command to various plugins
        // [NSException raise:@"DSCL" format:@"Test Exception"];
    
    	NSArray* list = GetPlugins();
		unsigned int	numPlugins = [list count];
		
		if (numPlugins > 0)
		{
			DSCLPlugInHelper*	helper = [[[DSCLPlugInHelper alloc] init] autorelease];
			[helper setEngine:engine setInteractive:interactive];
			
			unsigned int	i;
			for (i=0; i<numPlugins; i++)
			{
				id plugin = [list objectAtIndex:i];
				if ([plugin respondsToSelector:@selector(processArguments:numArgs:info:returningStatus:wasHandled:)])
				{
					id<DSCLPlugIn>	dsclPlugin = plugin;
					[dsclPlugin processArguments:argv numArgs:argc info:helper returningStatus:status wasHandled:&handled];
					if (handled)
						break;
				}
			}
		}
        
	NS_HANDLER
		char *cmd = argv[0];
				
		// This code is stolen from dscl_cmd()
        [engine restoreStack]; // In case the error happened before the stack was restored.

        if ([[localException name] isEqualToString:@"DSCL"])
        {
            printf("%s: %s\n", cmd, [[localException reason] UTF8String]);
            *status = eDSUnknownNodeName;
        }
        else if ([localException isKindOfClass:[DSoException class]])
        {
            printf("%s: DS error: %s\n", cmd, [(DSoException*)localException statusCString]);
            *status = -[(DSoException*)localException status];
        }
        else
            [localException raise];
	NS_ENDHANDLER
	
	return handled;		
}

//--------------------------------------------------------------------------------

void dscl_PlugInShowUsage(FILE* fp)
{
	NS_DURING
		NSArray* 		list = GetPlugins();
		unsigned int	numPlugins = [list count];
		unsigned int	i;
		
		for (i=0; i<numPlugins; i++)
		{
			id plugin = [list objectAtIndex:i];
			if ([plugin respondsToSelector:@selector(dumpHelpToFile:forCommand:)])
			{
				id<DSCLPlugIn>	dsclPlugin = plugin;
				
				fprintf(fp, "\n");
				[dsclPlugin dumpHelpToFile:fp forCommand:NULL];	// dump entire help
			}
		}

	NS_HANDLER
		fprintf(fp, "\n** Error showing usage from plugins **\n");
	NS_ENDHANDLER
}

//--------------------------------------------------------------------------------
