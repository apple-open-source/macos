#import <assert.h>
#import "RExplorer.h"
#include <IOKit/IOKitLibPrivate.h>

mach_port_t gMasterPort;

@implementation NSDictionary (Compare)

- (NSComparisonResult)compareNames:(NSDictionary *)dict2
{
    return [(NSString *)[self objectForKey:@"name"] caseInsensitiveCompare:(NSString *)[dict2 objectForKey:@"name"]];
}

@end

@implementation RExplorer

static void addChildrenOfPlistToMapsRecursively(id plist, NSMapTable *_parentMap, NSMapTable *_keyMap) {
    // Adds all the children of the given plist to the map-tables.  Does not add the plist itself.
    if ([plist isKindOfClass:[NSDictionary class]]) {
        NSEnumerator *keyEnum = [plist keyEnumerator];
        NSString *curKey;
        id curChild;


        while ((curKey = [keyEnum nextObject]) != nil) {
            curChild = [plist objectForKey:curKey];

            NSMapInsert(_parentMap, curChild, plist);
            NSMapInsert(_keyMap, curChild, curKey);
            addChildrenOfPlistToMapsRecursively(curChild, _parentMap, _keyMap);
        }
    } else if ([plist isKindOfClass:[NSArray class]]) {
        unsigned i, c = [plist count];
        id curChild;

        for (i=0; i<c; i++) {
            curChild = [plist objectAtIndex:i];
            NSMapInsert(_parentMap, curChild, plist);
            NSMapInsert(_keyMap, curChild, [NSString stringWithFormat:@"%u", i] );
            addChildrenOfPlistToMapsRecursively(curChild, _parentMap, _keyMap);
        }
    }
}

- (id)init
{
    io_registry_entry_t		entry;
    kern_return_t		kr;
    IONotificationPortRef	notifyPort;
    io_object_t		notification;

    self = [super init];

    // Obtain the I/O Kit communication handle.
    assert( KERN_SUCCESS == (
    kr = IOMasterPort(bootstrap_port, &gMasterPort)
    ));

    notifyPort = IONotificationPortCreate( gMasterPort );
    port = IONotificationPortGetMachPort( notifyPort );
    assert( KERN_SUCCESS == (
    kr = IOServiceAddInterestNotification( notifyPort,
                                           IORegistryEntryFromPath( gMasterPort, kIOServicePlane ":/"),
                                           kIOBusyInterest, 0, 0, &notification )
    ));

    assert( KERN_SUCCESS == (
    kr = IOServiceAddMatchingNotification( notifyPort, kIOFirstMatchNotification,
                                           IOServiceMatching("IOService"),
                                           0, 0, &notification )
    ));

    while ( (entry = IOIteratorNext( notification )) ) {
        IOObjectRelease( entry );
    }

    assert( KERN_SUCCESS == (
    kr = IOServiceAddMatchingNotification( notifyPort, kIOTerminatedNotification,
                                           IOServiceMatching("IOService"),
                                           0, 0, &notification )
    ));

    while ( (entry = IOIteratorNext( notification )) ) {
        IOObjectRelease( entry );
    }

    // create a timer to check for hardware additions/removals, etc.
    updateTimer = [[NSTimer scheduledTimerWithTimeInterval:1 target:self selector:@selector(checkForUpdate:) userInfo:nil repeats:YES] retain];

    // register services
    [NSApp registerServicesMenuSendTypes: [NSArray arrayWithObjects: NSStringPboardType, nil] returnTypes: [NSArray arrayWithObjects: NSStringPboardType, nil]];

    return self;
}

- (void)awakeFromNib
{
    int prefsSetting = 0;
        
    [self initializeRegistryDictionaryWithPlane:kIOServicePlane];
    [splitView setVertical:NO];
    [propertiesOutlineView setDelegate:self];
    [propertiesOutlineView setDataSource:self];
    [browser setDelegate:self];
    [browser setMinColumnWidth:170];
    [browser setMaxVisibleColumns:7];
    [planeBrowser setDelegate:self];
    [window setDelegate:self];

    keyColumn = [[propertiesOutlineView tableColumns] objectAtIndex:0];
    valueColumn = [[propertiesOutlineView tableColumns] objectAtIndex:1];
    
    [window setFrameAutosaveName:@"MainWindow"];
    [window setFrameUsingName:@"MainWindow"];
    [planeWindow setFrameAutosaveName:@"PlaneWindow"];
    [planeWindow setFrameUsingName:@"PlaneWindow"];
    [inspectorWindow setFrameAutosaveName:@"InspectorWindow"];
    [inspectorWindow setFrameUsingName:@"InspectorWindow"];

    [browser setPathSeparator:@":"];

    prefsSetting = [[NSUserDefaults standardUserDefaults] integerForKey:@"UpdatePrefs"];

    [updatePrefsMatrix selectCellAtRow:prefsSetting column:0];

    [objectDescription setStringValue:@""];
    [objectState setStringValue:@""];
        
    return;
}

- (void)dealloc
{
    [currentSelectedItemDict release];
    [updateTimer release];
    [super dealloc];
    return;
}

- (NSDictionary *)dictForIterated:(io_registry_entry_t)passedEntry
{
    kern_return_t       status;
    io_name_t		name;
    io_name_t		className;
    io_name_t           location;
    int             	retain, busy;
    uint64_t	    	state;
    char *		s;
    char		stateStr[256];

    io_registry_entry_t iterated;
    io_iterator_t       regIterator;
    NSMutableDictionary *localDict = [NSMutableDictionary dictionary];
    NSMutableArray 	*localArray = [NSMutableArray array];

    status = IORegistryEntryGetChildIterator(passedEntry, currentPlane, &regIterator);
    assert(status == KERN_SUCCESS);

    status = IORegistryEntryGetNameInPlane(passedEntry, currentPlane, name);
    assert(status == KERN_SUCCESS);

    status = IOObjectGetClass(passedEntry, className);
    assert(status == KERN_SUCCESS);

    status = IORegistryEntryGetLocationInPlane(passedEntry, currentPlane, location);
    if (status == KERN_SUCCESS) {
        strcat(name, "@");
        strcat(name, location);
    }

    s = stateStr + sprintf(stateStr, "(");
    retain = IOObjectGetRetainCount(passedEntry);
    status = IOServiceGetState(passedEntry, &state);
    if (status == KERN_SUCCESS) {

	status = IOServiceGetBusyState(passedEntry, &busy);
	if (status == KERN_SUCCESS)
	    busy = 0;
	s += sprintf(s, "%sregistered, %smatched, %sactive, busy %d, ",
		state & kIOServiceMatchedState    ? "" : "!",
		state & kIOServiceRegisteredState ? "" : "!",
		state & kIOServiceInactiveState   ? "in" : "",
		busy );
    }
    s += sprintf(s, "retain count %d)", retain);

    while ( (iterated = IOIteratorNext(regIterator)) != NULL ) {
        id insideDict = [self dictForIterated:iterated];
        [localArray addObject:insideDict];
//	IOObjectRelease(iterated);
    }
    IOObjectRelease(regIterator);

    [localDict setObject:localArray forKey:@"children"];
    [localDict setObject:[NSString stringWithFormat:@"%s", name] forKey:@"name"];
    [localDict setObject:[NSString stringWithFormat:@"%s", className] forKey:@"className"];
    [localDict setObject:[NSString stringWithFormat:@"%s", stateStr] forKey:@"state"];

    [localDict setObject:[NSNumber numberWithInt:passedEntry] forKey:@"regEntry"];

    return [NSDictionary dictionaryWithDictionary:localDict];
}

- (void)initializeRegistryDictionaryWithPlane:(const char *)plane
{
    io_registry_entry_t rootEntry;
    NSMutableDictionary *localDict = [NSMutableDictionary dictionary];

    if (registryDict) {
        [registryDict release];
    }
    registryDict = [[NSMutableDictionary alloc] init];

    currentPlane = plane;

    rootEntry = IORegistryGetRootEntry(gMasterPort);

    localDict = (NSMutableDictionary *)[self dictForIterated:rootEntry];

    registryDict = [localDict retain];
}

- (NSDictionary *) propertiesForRegEntry:(NSDictionary *)object
{
    NSDictionary * props;

    if ([object objectForKey:@"properties"]) {
        props = [[object objectForKey:@"properties"] retain];
        //[informationView setString:[[object objectForKey:@"properties"] description]];

    } else if ([object objectForKey:@"regEntry"]) {

	CFDictionaryRef dict;
	kern_return_t status;

        status = IORegistryEntryCreateCFProperties([[object objectForKey:@"regEntry"] intValue],
                                        &dict,
                                        kCFAllocatorDefault, kNilOptions);
        assert( KERN_SUCCESS == status );
        assert( CFDictionaryGetTypeID() == CFGetTypeID(dict));
        props = (NSDictionary *) dict;

    } else
	props = [object retain];

    return props;
}

- (void)changeLevel:(id)sender
{
    id object = nil;
    int column = [sender selectedColumn];
    int row = [sender selectedRowInColumn:column];
    NSDictionary * newItemDict;
    int count;
    int i;

    autoUpdate = NO;
    if (column < 0 || row < 0)
        return;

    if (column)
        object = [[self childArrayAtColumn:column] objectAtIndex:row];
    else
        object = registryDict;

    newItemDict = [self propertiesForRegEntry:object];

    if (currentSelectedItemDict != newItemDict) {
        [currentSelectedItemDict release];
        currentSelectedItemDict = newItemDict;
        [inspectorText setString:[newItemDict description]];
        [inspectorText display];
    }

    [objectDescription setStringValue:[NSString stringWithFormat:@"%@ : %@",
	    [object objectForKey:@"name"],
	    [object objectForKey:@"className"]]];

    [objectState setStringValue:[NSString stringWithFormat:@"%@",
	    [object objectForKey:@"state"]]];

    // go through and create a uniqued dictionary where all the values are uniqued and all the keys are uniqued
    if ([currentSelectedItemDict count]) {
        NSMutableDictionary *newDict = [NSMutableDictionary dictionary];
        NSArray *keyArray = [currentSelectedItemDict allKeys];
        NSArray *valueArray = [currentSelectedItemDict allValues];
        count = [currentSelectedItemDict count];

        for (i = 0; i < count ; i++) {

            if (CFGetTypeID([currentSelectedItemDict objectForKey:[keyArray objectAtIndex:i]]) == CFBooleanGetTypeID()) {
                if ([[[currentSelectedItemDict objectForKey:[keyArray objectAtIndex:i]] description] isEqualToString:@"0"]) {
                    [newDict setObject:[[[RBool alloc] initWithBool:NO] autorelease] forKey:[keyArray objectAtIndex:i]];
                } else {
                    [newDict setObject:[[[RBool alloc] initWithBool:YES] autorelease] forKey:[keyArray objectAtIndex:i]];
                }

            } else {
                [newDict setObject:[[[valueArray objectAtIndex:i] copy] autorelease] forKey:[keyArray objectAtIndex:i]];
            }
        }
        [currentSelectedItemDict release];
        currentSelectedItemDict = [newDict retain];
    }

    [self initializeMapsForDictionary:currentSelectedItemDict];

    [propertiesOutlineView reloadData];
#if 0
    count = [propertiesOutlineView numberOfRows];
    for (i = 0; i < count ; i++) {
        [propertiesOutlineView expandItem:[propertiesOutlineView itemAtRow:i] expandChildren:YES];
    }
#endif

    return;
}

- (void)dumpDictionaryToOutput:(id)sender
{

    NSLog(@"%@", registryDict);
    return;
}


- (NSArray *)childArrayAtColumn:(int)column
{
    int i = 0;
    id lastDict = registryDict;
    NSArray * localArray = nil;

    for (i = 0; (i < column); i++ ) {
        if (localArray) {
            lastDict = [localArray objectAtIndex:[browser selectedRowInColumn:i]];            
        }

        localArray = [[lastDict objectForKey:@"children"] sortedArrayUsingSelector:@selector(compareNames:)];
        //NSLog(@"array = %@", localArray);
    }

    return localArray;
}


- (void)displayAboutWindow:(id)sender
{
        if (aboutBoxOptions == nil) {
                aboutBoxOptions = [[NSDictionary alloc] initWithObjectsAndKeys:
                    @"2000", @"CopyrightStartYear",
                    nil
                ];
            }

            NSShowSystemInfoPanel(aboutBoxOptions);

    return;
}

- (void)displayPlaneWindow:(id)sender
{
    [planeBrowser loadColumnZero];
    [planeWindow makeKeyAndOrderFront:self];
    return;
}


- (void)switchRootPlane:(id)sender
{
    [self initializeRegistryDictionaryWithPlane:[[[sender selectedCell] stringValue] cString]];
    [browser loadColumnZero];
    [self changeLevel:browser];
    [currentSelectedItemDict release];
    currentSelectedItemDict = nil;
    [propertiesOutlineView reloadData];
    return;

}

- (void)doUpdate
{

    int wasAuto = autoUpdate;
    [self changeLevel:browser];
    autoUpdate = wasAuto;

    return;
}

- (void)reload
{
    // reload
    NSString *currentPath = [browser path];
    [self initializeRegistryDictionaryWithPlane:currentPlane];
    [browser loadColumnZero];
    [browser setPath:currentPath];
    [self doUpdate];
}

- (void)sheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
        if (NSAlertDefaultReturn == returnCode) {
            [self reload];
        }
        [NSApp endSheet:window];
}

- (void)registryHasChanged
{
    int prefsSetting = [[NSUserDefaults standardUserDefaults] integerForKey:@"UpdatePrefs"];

    if (prefsSetting == 1) {
        [self reload];
    } else if (prefsSetting == 0) {
        NSBeginInformationalAlertSheet(NSLocalizedString(@"The IOKit Registry has been changed.\nDo you wish to update your display or skip this update?", @""), NSLocalizedString(@"Update", @""), NSLocalizedString(@"Skip", @""), NULL, window, self, @selector(sheetDidEnd:returnCode:contextInfo:), NULL, NULL, @"");
    }
    
        
}

- (void)forceUpdate:(id)sender
{
    if(autoUpdate)
	autoUpdate = NO;
    else {
//	autoUpdate = YES;
	[self doUpdate];
    }
    return;
}

// Window delegation
- (void)windowWillClose:(id)sender
{
    [NSApp stop:nil];
    return;
}


- (void)initializeMapsForDictionary:(NSDictionary *)dict
{

    if (_parentMap) {
        NSFreeMapTable(_parentMap);
    }
    if (_keyMap) {
        NSFreeMapTable(_keyMap);
    }

    
    _parentMap = NSCreateMapTableWithZone(NSIntMapKeyCallBacks, NSNonRetainedObjectMapValueCallBacks, 100, [self zone]);
    _keyMap = NSCreateMapTableWithZone(NSIntMapKeyCallBacks, NSObjectMapValueCallBacks, 100, [self zone]);


    addChildrenOfPlistToMapsRecursively(dict, _parentMap, _keyMap);

    //NSLog(@"%@", NSStringFromMapTable(_keyMap));

    return;
}

- (void)checkForUpdate:(NSTimer *)timer
{
    kern_return_t	kr;
    unsigned long int	type;
    Boolean		registryHasQuieted = FALSE;
    struct {
        mach_msg_header_t		msgHdr;
        OSNotificationHeader		notifyHeader;
        IOServiceInterestContent	content;
        mach_msg_trailer_t		trailer;
    } msg;

    do {

        kr = mach_msg(&msg.msgHdr, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(msg), port, 0, MACH_PORT_NULL);
        if (kr != KERN_SUCCESS) {
            break;
        }

        kr = OSGetNotificationFromMessage(&msg.msgHdr, 0, &type, 0, 0, 0);
        if (kr != KERN_SUCCESS) {
            continue;
        }

        if (type == kIOServiceMatchedNotificationType || type == kIOServiceTerminatedNotificationType) {
            io_registry_entry_t	entry;
            io_iterator_t	notification = (io_iterator_t) msg.msgHdr.msgh_remote_port;

            while ( (entry = IOIteratorNext( notification )) ) {
                IOObjectRelease( entry );
            }

            registryHasChanged = TRUE;
        } else if (type == kIOServiceMessageNotificationType) {
            registryHasQuieted = (msg.content.messageArgument[0]) ? FALSE : TRUE;
        }

    } while ( TRUE );

    if (registryHasChanged && registryHasQuieted) {
        registryHasChanged = FALSE;
        [self registryHasChanged];
    } else if (autoUpdate)
        [self doUpdate];

    return;
}

// search the dictionaries
- (NSArray *)searchResultsForText:(NSString *)text searchKeys:(BOOL)keys searchValues:(BOOL)values
{
    if (!keys && !values) {
        return [NSArray array];
    }

    if (keys) {
        NSMutableArray *array = [NSMutableArray arrayWithArray:[self searchKeysResultsInDictionary:registryDict forText:text passedPath:@""]];

        // do the root directory stuff here ...
        NSEnumerator *rootEnum = [[[registryDict objectForKey:@"properties"] allKeys] objectEnumerator];
        id aRootEntry = nil;

        while (aRootEntry = [rootEnum nextObject]) {
            if ([aRootEntry isEqualToString:@"IOCatalogue"]) {
                // special case
            }
        }

        return [NSArray arrayWithArray:array];
    }
    return [NSArray array];
}

- (NSArray *)searchKeysResultsInDictionary:(NSDictionary *)dict forText:(NSString *)text passedPath:(NSString *)path
{
    NSArray *children = [dict objectForKey:@"children"];
    NSEnumerator *kidEnum = [children objectEnumerator];
    id aKid = nil;
    NSMutableArray *array = [NSMutableArray array];

    if ([(NSString *)[dict objectForKey:@"name"] length]) {
        if ([[[dict objectForKey:@"name"] uppercaseString] rangeOfString:[text uppercaseString]].length > 0) {
            [array addObject:[NSString stringWithFormat:@"%@:%@", path, [dict objectForKey:@"name"]]];
        }
    }

    while (aKid = [kidEnum nextObject]) {
        [array addObjectsFromArray:[self searchKeysResultsInDictionary:aKid forText:text passedPath:[NSString stringWithFormat:@"%@:%@", path, [dict objectForKey:@"name"]]]];
    }

    return [[array copy] autorelease];
}

- (void)goToPath:(NSString *)path
{
    NSString *newPath = [@":Root" stringByAppendingString:path];
    int count = [[path componentsSeparatedByString:@":"] count];
   
    if (count > 2) {
        count -= 2;
    }

    [browser setPath:newPath];
    [self changeLevel:browser];
    [browser scrollColumnToVisible:count];

    return;
}

- (void)copy:(id)sender
{
        NSString *currentPath = [[browser path] substringFromIndex:5];

        [[NSPasteboard generalPasteboard] declareTypes: [NSArray arrayWithObject:NSStringPboardType] owner: [self class]];
        [[NSPasteboard generalPasteboard] setString:currentPath forType:NSStringPboardType];
}

- (void)updatePrefs:(id)sender
{
    [[NSUserDefaults standardUserDefaults] setInteger:[updatePrefsMatrix selectedRow] forKey:@"UpdatePrefs"];
    [[NSUserDefaults standardUserDefaults] synchronize];
    
}

@end
