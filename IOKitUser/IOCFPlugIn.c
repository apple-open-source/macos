#ifdef HAVE_CFPLUGIN

#include <IOKit/IOCFPlugIn.h>

// Inited by class IOCFPlugInIniter
CFUUIDRef gIOCFPlugInInterfaceID = NULL;

typedef struct LookUUIDContextStruct {
    const void *	result;
    CFUUIDRef		key;
} LookUUIDContext;

static void
_IOGetWithUUIDKey(const void *key, const void * value, void *ctx)
{
    LookUUIDContext * 	context = (LookUUIDContext *) ctx;
    CFUUIDRef 		uuid;

    uuid = CFUUIDCreateFromString(NULL, (CFStringRef)key);
    if( uuid) {
        if( CFEqual( uuid, context->key))
            context->result = value;
        CFRelease(uuid);
    }
}

static kern_return_t
IOFindPlugIns( io_service_t service,
               CFUUIDRef pluginType,
               CFArrayRef * factories, CFArrayRef * plists )
{
    CFURLRef		url;
    CFPlugInRef		onePlugin;
    CFBundleRef		bundle;
    CFMutableDictionaryRef	serviceProps;
    CFDictionaryRef	plist;
    CFDictionaryRef	matching;
    CFDictionaryRef	pluginTypes;
    CFMutableStringRef	path;
    LookUUIDContext	context;
    CFStringRef		pluginName;
    boolean_t		matches;
    kern_return_t	kr = kIOReturnSuccess;
    
    // -- loadables
    onePlugin 		= NULL;
    serviceProps	= NULL;
    pluginName		= NULL;
    path 		= NULL;
    url 		= NULL;

    do {

        kr = IORegistryEntryCreateCFProperties( service, &serviceProps,
                                            kCFAllocatorDefault, kNilOptions );
        if( kr != kIOReturnSuccess)
            continue;
        pluginTypes = (CFDictionaryRef)
            CFDictionaryGetValue(serviceProps, CFSTR(kIOCFPlugInTypesKey));
        if( !pluginTypes )
            continue;

        // look up UUID key this way - otherwise string case matters
//        CFShow( pluginTypes );
        context.key = pluginType;
        context.result = 0;
        CFDictionaryApplyFunction( pluginTypes, &_IOGetWithUUIDKey, &context);
        pluginName = (CFStringRef) context.result;
        if( !pluginName)
            continue;

        path = CFStringCreateMutable( kCFAllocatorDefault, 0 );
        if( !path)
            continue;
        CFStringAppendCString(path,
                            "/System/Library/Extensions/",
                            kCFStringEncodingMacRoman);
        CFStringAppend(path, pluginName);
        url = CFURLCreateWithFileSystemPath(NULL, path,
                        kCFURLPOSIXPathStyle, TRUE);
        if( !url)
            continue;

        onePlugin = CFPlugInCreate(NULL, url);

    } while( false );

//    if (pluginName && (!onePlugin))
//        printf("Could not create CFPluginRef.\n");

    if( url)
        CFRelease( url );
    if( path)
        CFRelease( path );
    if( serviceProps)
        CFRelease( serviceProps );
    // --

    if( onePlugin
        && (bundle = CFPlugInGetBundle(onePlugin))
        && (plist = CFBundleGetInfoDictionary(bundle))
        && (matching = (CFDictionaryRef)
            CFDictionaryGetValue(plist, CFSTR("Personality")))) {

        kr = IOServiceMatchPropertyTable( service, matching, &matches);
        if( kr != kIOReturnSuccess)
            matches = FALSE;
    } else
        matches = TRUE;

    if( matches) {
        if( onePlugin)
            *factories = CFPlugInFindFactoriesForPlugInTypeInPlugIn(pluginType, onePlugin);
        else
            *factories = CFPlugInFindFactoriesForPlugInType(pluginType);
    } else
        *factories = 0;

    *plists = 0;

    return( kr );
}

kern_return_t
IOCreatePlugInInterfaceForService(io_service_t service,
                    CFUUIDRef pluginType, CFUUIDRef interfaceType,
                    IOCFPlugInInterface *** theInterface, SInt32 * theScore)
{
    CFDictionaryRef	plist = 0;
    CFArrayRef		plists;
    CFArrayRef		factories;
    CFMutableArrayRef	candidates;
    CFMutableArrayRef	scores;
    CFIndex		index;
    CFIndex		insert;
    CFUUIDRef		factoryID;
    kern_return_t	kr;
    SInt32		score;
    IOCFPlugInInterface **	interface;
    Boolean		haveOne;


    kr = IOFindPlugIns( service, pluginType,
                        &factories, &plists );
    if( KERN_SUCCESS != kr)
        return( kr );

    if ((KERN_SUCCESS != kr)
        || (factories == NULL)
        || (0 == CFArrayGetCount(factories))) {
//        printf("No factories for type\n");
        return( kIOReturnUnsupported );
    }
    candidates = CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);
    scores = CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);

    // allocate and Probe all
    if (candidates && scores) for( index = 0;
         index < CFArrayGetCount(factories);
         index++ ) {

        IUnknownVTbl **				iunknown;

        factoryID = (CFUUIDRef) CFArrayGetValueAtIndex(factories, index);
        iunknown = (IUnknownVTbl **)
            CFPlugInInstanceCreate(NULL, factoryID, pluginType);
        if (!iunknown) {
//            printf("Failed to create instance (link error?)\n");
            continue;
        }
        (*iunknown)->QueryInterface(iunknown, CFUUIDGetUUIDBytes(interfaceType),
                            (LPVOID *)&interface);

        // Now we are done with IUnknown interface
        (*iunknown)->Release(iunknown);

        if (!interface) {
//            printf("Failed to get interface.\n");
            continue;
        }
        if (plists)
            plist = (CFDictionaryRef) CFArrayGetValueAtIndex( plists, index );
        score = 0;   // from property table
        kr = (*interface)->Probe(interface, plist, service, &score);

        if (kIOReturnSuccess == kr) {
            for (insert = 0; insert < CFArrayGetCount(scores); insert++) {
                if (score > ((SInt32) CFArrayGetValueAtIndex(scores, insert)))
                    break;
            }
            CFArrayInsertValueAtIndex(candidates, insert, (void *) interface);
            CFArrayInsertValueAtIndex(scores, insert, (void *) score);
        } else
            (*interface)->Release(interface);
    }


    // Start in score order
    for (haveOne = false, index = 0;
         index < CFArrayGetCount(candidates);
         index++) {

        Boolean freeIt;

        if (plists)
            plist = (CFDictionaryRef) CFArrayGetValueAtIndex(plists, index );
        interface = (IOCFPlugInInterface **)
            CFArrayGetValueAtIndex(candidates, index );
        if (!haveOne) {
            haveOne = (kIOReturnSuccess == (*interface)->Start(interface, plist, service));
            freeIt = !haveOne;
            if (haveOne) {
                *theInterface = interface;
                *theScore = (SInt32) CFArrayGetValueAtIndex(scores, index );
            }
        } else
            freeIt = true;
        if (freeIt)
            (*interface)->Release(interface);
    }

    if (plists)
        CFRelease(plists);
    if (candidates)
        CFRelease(candidates);
    if (scores)
        CFRelease(scores);
    //    CFRelease(plugin);

    return (haveOne ? kIOReturnSuccess : kIOReturnNoResources);
}

kern_return_t
IODestroyPlugInInterface(IOCFPlugInInterface ** interface)
{
    kern_return_t	err;

    err = (*interface)->Stop(interface);
    (*interface)->Release(interface);

    return( err );
}

kern_return_t
IOCreatePlugInInterfaces(CFUUIDRef pluginType, CFUUIDRef interfaceType);

#endif /* !HAVE_CFPLUGIN */
