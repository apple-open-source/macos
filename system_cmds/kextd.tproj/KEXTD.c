#define CFRUNLOOP_NEW_API 1

#include "KEXTD.h"
#include "PTLock.h"
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitServer.h>
#include <IOKit/IOCFSerialize.h>
#include <IOKit/IOCFURLAccess.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/bootstrap.h>
#include <mach/kmod.h>
#include <syslog.h>


#define TIMER_PERIOD_S 10
#define LOOKAPPLENDRV 1

static Boolean gDebug;
static void KEXTdaemonSignal(void);

// from kernserv/queue.h   now kern/queue.h

/*
 *	A generic doubly-linked list (queue).
 */

struct queue_entry {
        struct queue_entry	*next;		/* next element */
        struct queue_entry	*prev;		/* previous element */
};

typedef struct queue_entry	*queue_t;
typedef	struct queue_entry	queue_head_t;
typedef	struct queue_entry	queue_chain_t;
typedef	struct queue_entry	*queue_entry_t;

/*
 *	Macro:		queue_init
 *	Function:
 *		Initialize the given queue.
 *	Header:
 *		void queue_init(q)
 *			queue_t		q;	\* MODIFIED *\
 */
#define	queue_init(q)	((q)->next = (q)->prev = q)

/*
 *	Macro:		queue_first
 *	Function:
 *		Returns the first entry in the queue,
 *	Header:
 *		queue_entry_t queue_first(q)
 *			queue_t	q;		\* IN *\
 */
#define	queue_first(q)	((q)->next)

/*
 *	Macro:		queue_next
 *	Header:
 *		queue_entry_t queue_next(qc)
 *			queue_t qc;
 */
#define	queue_next(qc)	((qc)->next)

/*
 *	Macro:		queue_end
 *	Header:
 *		boolean_t queue_end(q, qe)
 *			queue_t q;
 *			queue_entry_t qe;
 */
#define	queue_end(q, qe)	((q) == (qe))

#define	queue_empty(q)		queue_end((q), queue_first(q))

/*
 *	Macro:		queue_enter
 *	Header:
 *		void queue_enter(q, elt, type, field)
 *			queue_t q;
 *			<type> elt;
 *			<type> is what's in our queue
 *			<field> is the chain field in (*<type>)
 */
#define queue_enter(head, elt, type, field)			\
do {							\
        if (queue_empty((head))) {				\
                (head)->next = (queue_entry_t) elt;		\
                (head)->prev = (queue_entry_t) elt;		\
                (elt)->field.next = head;			\
                (elt)->field.prev = head;			\
        }							\
        else {							\
                register queue_entry_t prev;			\
                                                                \
                prev = (head)->prev;				\
                (elt)->field.prev = prev;			\
                (elt)->field.next = head;			\
                (head)->prev = (queue_entry_t)(elt);		\
                ((type)prev)->field.next = (queue_entry_t)(elt);\
        }							\
} while(0)

/*
 *	Macro:		queue_field [internal use only]
 *	Function:
 *		Find the queue_chain_t (or queue_t) for the
 *		given element (thing) in the given queue (head)
 */
#define	queue_field(head, thing, type, field)			\
                (((head) == (thing)) ? (head) : &((type)(thing))->field)

/*
 *	Macro:		queue_remove
 *	Header:
 *		void queue_remove(q, qe, type, field)
 *			arguments as in queue_enter
 */
#define	queue_remove(head, elt, type, field)			\
do {							\
        register queue_entry_t	next, prev;			\
                                                                \
        next = (elt)->field.next;				\
        prev = (elt)->field.prev;				\
                                                                \
        queue_field((head), next, type, field)->prev = prev;	\
        queue_field((head), prev, type, field)->next = next;	\
} while(0)


typedef struct _KEXTD {
    CFIndex _refcount;
    CFRunLoopRef _runloop;
    CFRunLoopSourceRef _signalsource;
    CFRunLoopSourceRef _kernelsource;
    CFMutableArrayRef _scanPaths;
    CFMutableArrayRef _unloaded;
    CFMutableArrayRef _helpers;
    queue_head_t _requestQ;
    PTLockRef _runloop_lock;
    PTLockRef _queue_lock;
    KEXTManagerRef _manager;
    mach_port_t _catPort;
    Boolean _initializing;
    Boolean _beVerbose;
#if TIMERSOURCE
    Boolean _pollFileSystem;
    CFIndex _pollingPeriod;
#endif
} KEXTD;

typedef struct _request {
    unsigned int	type;
    CFStringRef		kmodname;
    CFStringRef		kmodvers;
    queue_chain_t	link;
} request_t;

typedef struct _KEXTDHelper {
    void * context;
    KEXTDHelperCallbacks cbs;
} KEXTDHelper;

CFDictionaryRef   _KEXTPersonalityGetProperties(KEXTPersonalityRef personality);
static void       _KEXTDRemovePersonalitiesFromUnloadedList(KEXTDRef kextd, CFStringRef parentKey);
static void       _KEXTDAddPersonalitiesWithModuleToUnloadedList(KEXTDRef kextd, CFStringRef modName);
const void *      _KEXTPersonalityRetainCB(CFAllocatorRef allocator, const void *ptr);
void              _KEXTPersonalityReleaseCB(CFAllocatorRef allocator, const void *ptr);
Boolean           _KEXTPersonalityEqualCB(const void *ptr1, const void *ptr2);
mach_port_t       _KEXTManagerGetMachPort(KEXTManagerRef manager);

extern kern_return_t
kmod_control(host_t host,
             kmod_t id,
             kmod_control_flavor_t flavor,
             kmod_args_t *data,
             mach_msg_type_number_t *dataCount);

extern KEXTReturn KERN2KEXTReturn(kern_return_t kr);

static KEXTDRef _kextd = NULL;

static void logErrorFunction(const char * string)
{
    syslog(LOG_ERR, string);
    return;
}

static void logMessageFunction(const char * string)
{
    syslog(LOG_INFO, string);
    return;
}


static void ArrayMergeFunc(const void * val, void * context)
{
    CFMutableArrayRef array;

    array = context;

    CFArrayAppendValue(array, val);
}

static void CFArrayMergeArray(CFMutableArrayRef array1, CFArrayRef array2)
{
    CFRange range;

    if ( !array1 || !array2 ) {
        return;
    }

    range = CFRangeMake(0, CFArrayGetCount(array2));
    CFArrayApplyFunction(array2, range, ArrayMergeFunc, array1);
}

static void CallHelperEvent(void * val, void * context[])
{
    KEXTDRef kextd;
    KEXTEvent event;
    KEXTDHelper * helper;
    CFTypeRef item;

    helper = val;
    kextd = context[0];
    event = *(KEXTEvent *)context[1];
    item = context[2];

    if ( helper && context && helper->cbs.EventOccurred ) {
        helper->cbs.EventOccurred(event, item, kextd);
    }
}

static void ConfigsForBundles(const void * var, void * context[])
{
    KEXTManagerRef manager;
    KEXTBundleRef bundle;
    CFMutableArrayRef array;
    CFArrayRef configs;

    bundle = (KEXTBundleRef)var;

    manager = context[0];
    array = context[1];

    configs = KEXTManagerCopyConfigsForBundle(manager, bundle);
    if ( configs ) {
        CFArrayMergeArray(array, configs);
        CFRelease(configs);
    }
}


static inline KEXTBootlevel _KEXTDGetBootlevel(KEXTPersonalityRef personality)
{
    KEXTBootlevel bootlevel;
    CFStringRef priority;
    CFStringRef category;

    bootlevel = kKEXTBootlevelExempt;
    priority = KEXTPersonalityGetProperty(personality, CFSTR("BootPriority"));
    if ( !priority ) {
        category = KEXTPersonalityGetProperty(personality, CFSTR("DeviceCategory"));
        if ( !category ) {
            return kKEXTBootlevelExempt;
        }

        if ( CFEqual(category, CFSTR("System Controller")) ) {
             bootlevel = kKEXTBootlevelRequired;
        }
        else if ( CFEqual(category, CFSTR("Bus Controller")) ) {
             bootlevel = kKEXTBootlevelFlexible;
        }
        else if ( CFEqual(category, CFSTR("Keyboard")) ) {
             bootlevel = kKEXTBootlevelSingleUser;
        }
        else if ( CFEqual(category, CFSTR("Input Device")) ) {
             bootlevel = kKEXTBootlevelExempt;
        }
        else if ( CFEqual(category, CFSTR("Pointing Device")) ) {
             bootlevel = kKEXTBootlevelExempt;
        }
        else if ( CFEqual(category, CFSTR("Mouse")) ) {
             bootlevel = kKEXTBootlevelRecovery;
        }
        else if ( CFEqual(category, CFSTR("Graphics Controller")) ) {
             bootlevel = kKEXTBootlevelRecovery;
        }
        else if ( CFEqual(category, CFSTR("Graphics Accelerator")) ) {
             bootlevel = kKEXTBootlevelExempt;
        }
        else if ( CFEqual(category, CFSTR("Video Device")) ) {
             bootlevel = kKEXTBootlevelExempt;
        }
        else if ( CFEqual(category, CFSTR("Disk Controller")) ) {
             bootlevel = kKEXTBootlevelFlexible;
        }
        else if ( CFEqual(category, CFSTR("Disk Media")) ) {
             bootlevel = kKEXTBootlevelFlexible;
        }
        else if ( CFEqual(category, CFSTR("Audio Controller")) ) {
             bootlevel = kKEXTBootlevelExempt;
        }
        else if ( CFEqual(category, CFSTR("Sound Device")) ) {
             bootlevel = kKEXTBootlevelExempt;
        }
        else if ( CFEqual(category, CFSTR("Network Controller")) ) {
             bootlevel = kKEXTBootlevelFlexible;
        }

        return bootlevel;
    }

    if ( CFEqual(priority, CFSTR("Exempt")) ) {
        bootlevel = kKEXTBootlevelExempt;
    }
    else if ( CFEqual(priority, CFSTR("Recovery")) ) {
        bootlevel = kKEXTBootlevelRecovery;
    }
    else if ( CFEqual(priority, CFSTR("Special")) ) {
        bootlevel = kKEXTBootlevelSingleUser;
    }
    else if ( CFEqual(priority, CFSTR("Flexible")) ) {
        bootlevel = kKEXTBootlevelFlexible;
    }
    else if ( CFEqual(priority, CFSTR("Required")) ) {
        bootlevel = kKEXTBootlevelRequired;
    }

    return bootlevel;
}

static void ArrayAddToLoadList(void * val, void * context[])
{
    KEXTPersonalityRef person;
    KEXTBootlevel bootlevel;
    CFMutableArrayRef unloaded;
    CFMutableArrayRef loadlist;
    CFRange range;
    Boolean doAdd;

    if ( !val || !context ) {
        return;
    }

    doAdd = true;
    person = val;
    unloaded = context[0];
    loadlist = context[1];
    bootlevel = *(KEXTBootlevel *)context[2];

    if ( bootlevel != kKEXTBootlevelNormal ) {
        doAdd  = _KEXTDGetBootlevel(person) & bootlevel;
    }

    if ( doAdd ) {
        range = CFRangeMake(0, CFArrayGetCount(loadlist));
        if ( !CFArrayContainsValue(loadlist, range, person) ) {
            CFArrayAppendValue(loadlist, person);
        }
    }
    else {
        range = CFRangeMake(0, CFArrayGetCount(unloaded));
        if ( !CFArrayContainsValue(unloaded, range, person) ) {
            CFArrayAppendValue(unloaded, person);
        }
        CFArrayAppendValue(unloaded, person);
    }
}

static void signalhandler(int signal)
{
    if ( _kextd && (signal == SIGHUP) ) {
        KEXTDHangup(_kextd);
    }
}

static KEXTReturn _KEXTDInitSyslog(KEXTD * k)
{
    openlog("kextd", LOG_PID | LOG_NDELAY, LOG_DAEMON);

    return kKEXTReturnSuccess;
}

// This is called when authenticating a new bundle.
static KEXTReturn _KEXTDAuthenticateBundleCB(CFURLRef url, void * context)
{
    Boolean ret;

    ret = KEXTManagerAuthenticateURL(url);
    if ( !ret ) {
        KEXTD * k = (KEXTD *)context;
        KEXTEvent event;
        CFStringRef urlString;
        CFRange range;
        char name[256];
        void * context2[3];

        urlString = CFURLGetString(url);
        if ( CFStringGetCString(urlString, name, 256, kCFStringEncodingNonLossyASCII) )
            syslog(LOG_ERR, "%s failed authentication.", name);

        event = kKEXTEventBundleAuthenticationFailed;
        context2[0] = k;
        context2[1] = &event;
        context2[2] = (void *)url;

        range = CFRangeMake(0, CFArrayGetCount(k->_helpers));
        CFArrayApplyFunction(k->_helpers, range, (CFArrayApplierFunction)CallHelperEvent, context2);
    }

    return ret;
}

// This is called when a new bundle has been found.
static Boolean _KEXTDWillAddBundleCB(KEXTManagerRef manager, KEXTBundleRef bundle, void * context)
{
    KEXTD * k = (KEXTD *)context;
    CFURLRef url;
    CFIndex count;
    CFIndex i;
    Boolean ret;

    ret = true;
    count = CFArrayGetCount(k->_helpers);
    for ( i = 0; i < count; i++ ) {
        KEXTDHelper * helper;

        helper = (KEXTDHelper *)CFArrayGetValueAtIndex(k->_helpers, i);
        if ( !helper )
            continue;

        if ( helper->cbs.BundleAdd ) {
            ret = helper->cbs.BundleAdd(bundle, helper->context);
            if ( !ret )
                break;
        }
    }

    url = KEXTBundleCopyURL(bundle);
    if ( url ) {
        if ( k->_beVerbose && ret ) {
            CFStringRef cfstr;
            char str[256];

            cfstr = CFURLGetString(url);
            if ( CFStringGetCString(cfstr, str, 256, kCFStringEncodingNonLossyASCII) ) {
                syslog(LOG_INFO, "%s added.", str);
            }
        }

        // Remove any unloaded personalities from look-aside queue
        // which are in this bundle.
        _KEXTDRemovePersonalitiesFromUnloadedList((KEXTDRef)k, KEXTBundleGetPrimaryKey(bundle));
        CFRelease(url);
    }

    return ret;
}

// This is called after a bundle has been added to the KEXTManager database.
static void _KEXTDWasAddedBundleCB(KEXTManagerRef manager, KEXTBundleRef bundle, void * context)
{
    KEXTD * k;
    KEXTBootlevel bootlevel;
    CFMutableArrayRef toload;
    CFArrayRef persons;
    CFArrayRef configs;
    CFArrayRef unloaded;
    CFRange range;
    void * context2[3];

    k = (KEXTD *)context;
    if ( k->_initializing ) {
        return;
    }

    bootlevel = kKEXTBootlevelNormal;

    toload = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if ( !toload ) {
        return;
    }

    context2[0] = k->_unloaded;
    context2[1] = toload;
    context2[2] = &bootlevel;


    // Create a list of all personalities and configurations
    // and send it to the catalogue.  First, get the unloaded
    // personalities.
    unloaded = CFArrayCreateCopy(kCFAllocatorDefault, k->_unloaded);
    CFArrayRemoveAllValues(k->_unloaded);
    if ( unloaded ) {
        range = CFRangeMake(0, CFArrayGetCount(unloaded));
        CFArrayApplyFunction(unloaded, range, (CFArrayApplierFunction)ArrayAddToLoadList, context2);
        CFRelease(unloaded);
    }

    persons = KEXTManagerCopyPersonalitiesForBundle(manager, bundle);
    if ( persons ) {
        range = CFRangeMake(0, CFArrayGetCount(persons));
        CFArrayApplyFunction(persons, range, (CFArrayApplierFunction)ArrayAddToLoadList, context2);
        CFRelease(persons);
    }

    configs = KEXTManagerCopyConfigsForBundle(manager, bundle);
    if ( configs ) {
        range = CFRangeMake(0, CFArrayGetCount(configs));
        CFArrayApplyFunction(configs, range, (CFArrayApplierFunction)ArrayAddToLoadList, context2);
        CFRelease(configs);
        
    }

    // Send the list to IOCatalogue.
    if ( CFArrayGetCount(toload) > 0 ) {
        KEXTManagerLoadPersonalities(k->_manager, toload);
    }
    
    CFRelease(toload);
}

static void _KEXTDConfigWasAdded(KEXTManagerRef manager, KEXTConfigRef config, void * context)
{
    KEXTD * kextd;

    if ( !manager || !config || !context ) {
        return;
    }

    kextd = context;

    if ( !kextd->_initializing ) {
        CFStringRef primaryKey;
        CFArrayRef array;
        void * vals[1];

        primaryKey = CFDictionaryGetValue(config, CFSTR("ParentKey"));
        if ( !primaryKey ) {
            return;
        }

        if ( !KEXTManagerGetBundle(manager, primaryKey) ) {
            return;
        }
        
        vals[0] = config;
        array = CFArrayCreate(kCFAllocatorDefault, vals, 1, &kCFTypeArrayCallBacks);
        if ( array ) {
            KEXTManagerLoadPersonalities(manager, array);
            CFRelease(array);
        }
    }
}

static void _KEXTDConfigWasRemoved(KEXTManagerRef manager, KEXTConfigRef config, void * context)
{
    KEXTD * kextd;
    
    if ( !manager || !config || !context ) {
        return;
    }

    kextd = context;
    
    if ( !kextd->_initializing ) {
        KEXTManagerUnloadPersonality(manager, config);
    }
}

static void ArrayUnloadPersonality(const void * val, void * context)
{
    KEXTManagerRef manager;
    KEXTPersonalityRef person;

    manager = context;
    person = (KEXTPersonalityRef)val;
    
    KEXTManagerUnloadPersonality(manager, person);
}

// This is called when a bundle has been removed from the filesystem.
static Boolean _KEXTDWillRemoveBundleCB(KEXTManagerRef manager, KEXTBundleRef bundle, void * context)
{
    KEXTD * k = (KEXTD *)context;
    CFIndex count;
    CFIndex i;
    Boolean ret;

    ret = true;
    count = CFArrayGetCount(k->_helpers);
    for ( i = 0; i < count; i++ ) {
        KEXTDHelper * helper;

        helper = (KEXTDHelper *)CFArrayGetValueAtIndex(k->_helpers, i);
        if ( !helper )
            continue;

        if ( helper->cbs.BundleRemove ) {
            ret = helper->cbs.BundleRemove(bundle, helper->context);
            if ( !ret )
                break;
        }
    }
    
    if ( ret ) {
        CFArrayRef personalities;
        CFArrayRef configs;
        CFURLRef url;
        CFRange range;

        // XXX -- svail: might want to unload bundle personalities
        // from IOCatalogue and maybe unload the modules if possible.
        // If a module is present with active personalities, don't remove
        // bundle from database.
        personalities = KEXTManagerCopyPersonalitiesForBundle(manager, bundle);
        if ( personalities ) {
            range = CFRangeMake(0, CFArrayGetCount(personalities));
            CFArrayApplyFunction(personalities, range, ArrayUnloadPersonality, manager);
            CFRelease(personalities);
        }
        
        configs = KEXTManagerCopyConfigsForBundle(manager, bundle);
        if ( configs ) {
            range = CFRangeMake(0, CFArrayGetCount(configs));
            CFArrayApplyFunction(configs, range, ArrayUnloadPersonality, manager);
            CFRelease(configs);
        }
        
        url = KEXTBundleCopyURL(bundle);
        if ( url ) {
            if ( k->_beVerbose && ret ) {
                CFStringRef cfstr;
                char str[256];

                cfstr = CFURLGetString(url);
                if ( CFStringGetCString(cfstr, str, 256, kCFStringEncodingNonLossyASCII) ) {
                    syslog(LOG_INFO, "%s removed.", str);
                }
            }

            // Remove any unloaded personalities from the unloaded
            // list if they are associated with the bundle.

            _KEXTDRemovePersonalitiesFromUnloadedList((KEXTDRef)k, KEXTBundleGetPrimaryKey(bundle));
            CFRelease(url);
        }
    }
    
    return ret;
}

// This is called before KEXT loads a KMOD.
static Boolean _KEXTDModuleWillLoadCB(KEXTManagerRef manager, KEXTModuleRef module, void * context)
{
    KEXTD * k;
    CFIndex count;
    CFIndex i;
    Boolean ret;

    k = (KEXTD *)context;

    ret = true;
    count = CFArrayGetCount(k->_helpers);
    for ( i = 0; i < count; i++ ) {
        KEXTDHelper * helper;

        helper = (KEXTDHelper *)CFArrayGetValueAtIndex(k->_helpers, i);
        if ( !helper )
            continue;

        if ( helper->cbs.ModuleWillLoad ) {
            if ( !helper->cbs.ModuleWillLoad(module, helper->context) ) {
                ret = false;
                break;
            }
        }
    }

    if ( ret ) {
        CFStringRef moduleName;
        char name[256];

        moduleName = KEXTModuleGetProperty(module, CFSTR("CFBundleIdentifier"));
        if ( !moduleName )
            return false;

        if ( !CFStringGetCString(moduleName, name, 256, kCFStringEncodingNonLossyASCII) )
            return false;

        syslog(LOG_INFO, "loading module: %s.\n", name);
    }

    return ret;
}

// This is called when a module has been successfully loaded.
static void _KEXTDModuleWasLoadedCB(KEXTManagerRef manager, KEXTModuleRef module, void * context)
{
    KEXTD * k;
    CFArrayRef array;
    CFMutableArrayRef send;
    CFIndex count;
    CFIndex i;

    k = (KEXTD *)context;

    count = CFArrayGetCount(k->_helpers);
    for ( i = 0; i < count; i++ ) {
        KEXTDHelper * helper;

        helper = (KEXTDHelper *)CFArrayGetValueAtIndex(k->_helpers, i);
        if ( !helper )
            continue;

        if ( helper->cbs.ModuleWasLoaded ) {
            helper->cbs.ModuleWasLoaded(module, helper->context);
        }
    }

    // Remove personalities from unloaded list if they
    // are associated with the module and pass them to the
    // kernel just in case they aren't there yet.

    array = CFArrayCreateCopy(kCFAllocatorDefault, k->_unloaded);
    send = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    count = CFArrayGetCount(array);
    
    CFArrayRemoveAllValues(k->_unloaded);

    for ( i = 0; i < count; i++ ) {
        KEXTPersonalityRef person;
        CFStringRef moduleID;
        CFStringRef personalityBundleID;

        person = (KEXTPersonalityRef)CFArrayGetValueAtIndex(array, i);
        if ( !person ) {
            continue;
        }
        
        moduleID = KEXTPersonalityGetProperty(person, CFSTR("CFBundleIdentifier"));
        if ( !moduleID ) {
            continue;
        }

        personalityBundleID = KEXTModuleGetProperty(module, CFSTR("CFBundleIdentifier"));
        if ( !personalityBundleID ) {
            continue;
        }

        if ( !CFEqual(moduleID, personalityBundleID) ) {
            CFArrayAppendValue(k->_unloaded, person);
        }
        else {
            CFArrayAppendValue(send, person);
        }
    }

    if ( CFArrayGetCount(send) > 0 ) {
        KEXTManagerLoadPersonalities(k->_manager, send);
    }
    
    CFRelease(send);
    CFRelease(array);
    
    if ( k->_beVerbose ) {
        CFStringRef moduleName;
        char name[256];

        moduleName = KEXTModuleGetProperty(module, CFSTR("CFBundleIdentifier"));
        if ( !moduleName )
            return;

        if ( !CFStringGetCString(moduleName, name, 256, kCFStringEncodingNonLossyASCII) )
            return;
        
        syslog(LOG_INFO, "loaded module: %s\n", name);
    }
}

static KEXTReturn _KEXTDModuleErrorCB(KEXTManagerRef manager, KEXTModuleRef module, KEXTReturn error, void * context)
{
    char name[256];
    KEXTD * k;
    KEXTReturn ret;
    CFIndex i;
    CFIndex count;
    CFStringRef moduleName;

    k = (KEXTD *)context;

    moduleName = KEXTModuleGetProperty(module, CFSTR("CFBundleIdentifier"));
    if ( !moduleName )
        return kKEXTReturnPropertyNotFound;

    if ( !CFStringGetCString(moduleName, name, 256, kCFStringEncodingNonLossyASCII) )
        return kKEXTReturnNoMemory;

    if ( error == kKEXTReturnModuleAlreadyLoaded ) {
        if ( k->_beVerbose )
            syslog(LOG_INFO, "module already loaded: %s.\n", name);

        return error;
    }

    ret = error;
    count = CFArrayGetCount(k->_helpers);
    for ( i = 0; i < count; i++ ) {
        KEXTDHelper * helper;

        helper = (KEXTDHelper *)CFArrayGetValueAtIndex(k->_helpers, i);
        if ( !helper )
            continue;

        if ( helper->cbs.ModuleLoadError ) {
            ret = helper->cbs.ModuleLoadError(module, error, helper->context);
            if ( ret == kKEXTReturnSuccess ) {
                break;
            }
        }
    }
    if ( ret == kKEXTReturnSuccess )
        return kKEXTReturnSuccess;

    syslog(LOG_ERR, "error (%d) loading module: %s.\n", ret, name);

    return ret;
}

#if TIMERSOURCE
static void _KEXTDTimerCallout(CFRunLoopTimerRef timer, void * info)
{
    KEXTDScanPaths((KEXTDRef)info);
}
#endif

static void _KEXTDSIGHUPCallout(void * info)
{
    KEXTD * k;

    k = (KEXTD *)info;
    if ( k->_beVerbose ) {
        syslog(LOG_INFO, "user requests directory re-scan.");
    }

    // Check for new or removed bundles and do the appropriate
    // things.
    KEXTDScanPaths((KEXTDRef)info);

    // Make sure we try to load the unloaded personalities
    // It's probably overkill to do this here.
    if ( CFArrayGetCount(k->_unloaded) > 0 ) {
        KEXTManagerLoadPersonalities(k->_manager, k->_unloaded);
    }
}

// This function is called when IOCatalogue requests a driver module.
static void _KEXTDPerform(void * info)
{
    KEXTD * k;
    unsigned int type;

    k = (KEXTD *)info;

//    KEXTDScanPaths((KEXTDRef)k);

    PTLockTakeLock(k->_queue_lock);
    while ( !queue_empty(&k->_requestQ) ) {
        request_t * reqstruct;
        CFStringRef name;

        // Dequeue the kernel request structure.
        reqstruct = (request_t *)queue_first(&k->_requestQ);
        queue_remove(&k->_requestQ, reqstruct, request_t *, link);
        PTLockUnlock(k->_queue_lock);

        type = reqstruct->type;
        name = reqstruct->kmodname;
        free(reqstruct);

	if( type == kIOCatalogMatchIdle) {

	    mach_timespec_t timeout = { 10, 0 };
	    IOKitWaitQuiet( k->_catPort, &timeout );
	    KEXTdaemonSignal();

        } else if ( name ) {

            if ( k->_beVerbose ) {
                char modname[256];

                if ( CFStringGetCString(name, modname, 256, kCFStringEncodingNonLossyASCII) ) {
                    syslog(LOG_INFO, "kernel requests module: %s", modname);
                }
            }

            KEXTDKernelRequest((KEXTDRef)k, name);
            CFRelease(name);
        }

        PTLockTakeLock(k->_queue_lock);
    }
    PTLockUnlock(k->_queue_lock);
}

KEXTDRef KEXTDCreate(CFArrayRef scanPaths, KEXTReturn * error)
{
    KEXTD * kextd;

    kextd = (KEXTD *)malloc(sizeof(KEXTD));
    if ( !kextd ) {
        *error = kKEXTReturnNoMemory;
        return NULL;
    }
    memset(kextd, 0, sizeof(KEXTD));

    kextd->_queue_lock = PTLockCreate();
    kextd->_runloop_lock = PTLockCreate();

    kextd->_helpers = CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);
    kextd->_unloaded = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    kextd->_scanPaths = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if ( !kextd->_scanPaths || !kextd->_unloaded || !kextd->_helpers ) {
        *error = kKEXTReturnNoMemory;
        KEXTDFree((KEXTDRef)kextd);
        return NULL;
    }

    kextd->_initializing = true;
    kextd->_beVerbose = false;
#if TIMERSOURCE
    kextd->_pollFileSystem = false;
    kextd->_pollingPeriod = TIMER_PERIOD_S;
#endif

    queue_init(&kextd->_requestQ);
    
    if ( scanPaths ) {
        CFURLRef url;
        CFIndex count;
        CFIndex i;

        count = CFArrayGetCount(scanPaths);
        for ( i = 0; i < count; i++ ) {
            url = (CFURLRef)CFArrayGetValueAtIndex(scanPaths, i);
            KEXTDAddScanPath((KEXTDRef)kextd, url);
        }
    }

    return (KEXTDRef)kextd;
}

static void _KEXTDFlushHelpers(KEXTDRef kextd)
{
    KEXTD * k;
    CFIndex count;
    CFIndex i;

    k = (KEXTD *)kextd;
    count = CFArrayGetCount(k->_helpers);
    for ( i = 0; i < count; i++ ) {
        KEXTDHelper * helper;

        helper = (KEXTDHelper *)CFArrayGetValueAtIndex(k->_helpers, i);
        if ( !helper )
            continue;

        if ( helper->cbs.DaemonWillTerminate )
            helper->cbs.DaemonWillTerminate(helper->context);
        if ( helper->cbs.HelperFinalize )
            helper->cbs.HelperFinalize(helper->context);

        free(helper);
    }
}

void KEXTDFree(KEXTDRef kextd)
{
    KEXTD * k;

    k = (KEXTD *)kextd;

    syslog(LOG_DEBUG, "terminating.");

    if ( k->_helpers ) {
        _KEXTDFlushHelpers(kextd);
        CFRelease(k->_helpers);
    }
    if ( k->_kernelsource )
        CFRelease(k->_kernelsource);
    if ( k->_signalsource )
        CFRelease(k->_signalsource);
    if ( k->_runloop )  
        CFRelease(k->_runloop);
    if ( k->_scanPaths )
        CFRelease(k->_scanPaths);
    if ( k->_manager )
        KEXTManagerRelease(k->_manager);
    if ( k->_queue_lock )
        PTLockFree(k->_queue_lock);
    if ( k->_runloop_lock )
        PTLockFree(k->_runloop_lock);
    
    closelog();

    free(kextd);
}

void KEXTDReset(KEXTDRef kextd)
{
    KEXTD * k;
    CFIndex count;
    CFIndex i;

    syslog(LOG_DEBUG, "resetting.");
    
    k = (KEXTD *)kextd;
    count = CFArrayGetCount(k->_helpers);
    for ( i = 0; i < count; i++ ) {
        KEXTDHelper * helper;

        helper = (KEXTDHelper *)CFArrayGetValueAtIndex(k->_helpers, i);
        if ( !helper )
            continue;

        if ( helper->cbs.EventOccurred )
            helper->cbs.EventOccurred(kKEXTEventReset, NULL, helper->context);
    }

    if ( k->_manager )
        KEXTManagerReset(k->_manager);
    
    KEXTDScanPaths(kextd);
}

static KEXTReturn _KEXTDSendDataToCatalog(KEXTDRef kextd, int flag, CFTypeRef obj)
{
    KEXTD * k;
    KEXTReturn error;
    CFDataRef data;
    CFIndex len;
    void * ptr;

    k = (KEXTD *)kextd;
    data = NULL;
    error = kKEXTReturnSuccess;

    data = IOCFSerialize(obj, 0);
    if ( !data ) {
        return kKEXTReturnSerializationError;
    }

    len = CFDataGetLength(data);
    ptr = (void *)CFDataGetBytePtr(data);
    error = KERN2KEXTReturn(IOCatalogueSendData(k->_catPort, flag, ptr, len));
    CFRelease(data);

    return error;
}

static KEXTReturn _KEXTDSendPersonalities(KEXTDRef kextd, KEXTBootlevel bootlevel)
{
    KEXTReturn error;
    CFArrayRef persons;
    CFArrayRef bundles;
    CFMutableArrayRef configs;
    CFMutableArrayRef toload;
    CFRange range;
    void * context[3];

    error = kKEXTReturnSuccess;

    toload = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if ( !toload ) {
        return kKEXTReturnNoMemory;
    }

    configs = NULL;
    bundles = KEXTManagerCopyAllBundles(((KEXTD*)kextd)->_manager);
    if ( bundles ) {
        configs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        if ( !configs ) {
            CFRelease(bundles);
            CFRelease(toload);
            return kKEXTReturnNoMemory;
        }

        context[0] = ((KEXTD*)kextd)->_manager;
        context[1] = configs;

        range = CFRangeMake(0, CFArrayGetCount(bundles));
        CFArrayApplyFunction(bundles, range, (CFArrayApplierFunction)ConfigsForBundles, context);
        CFRelease(bundles);
    }
    
    // Filter out any inappropriate personalities given the bootlevel of the 
    // system. Store these personalities in the _unloaded list, they will be
    // loaded when a SIGHUP is sent to the daemon.
    context[0] = ((KEXTD *)kextd)->_unloaded;
    context[1] = toload;
    context[2] = &bootlevel;

    persons = KEXTManagerCopyAllPersonalities(((KEXTD*)kextd)->_manager);
    if ( persons ) {
        range = CFRangeMake(0, CFArrayGetCount(persons));
        CFArrayApplyFunction(persons, range, (CFArrayApplierFunction)ArrayAddToLoadList, context);
        CFRelease(persons);
    }

    if ( configs ) {
        range = CFRangeMake(0, CFArrayGetCount(configs));
        CFArrayApplyFunction(configs, range, (CFArrayApplierFunction)ArrayAddToLoadList, context);
        CFRelease(configs);
    }

    if ( CFArrayGetCount(toload) > 0 ) {
        error = KEXTManagerLoadPersonalities(((KEXTD *)kextd)->_manager, toload);
    } else {
	KEXTdaemonSignal();
    }

    CFRelease(toload);

    return error;

}

static inline Boolean _KEXTPersonalityNeedsModule(KEXTPersonalityRef person, CFStringRef modName)
{
    CFStringRef name;

    name = KEXTPersonalityGetProperty(person, CFSTR("CFBundleIdentifier"));
    if ( !name ) {
        return false;
    }

    return CFEqual(name, modName);
}

// If a module fails to load for some reason, then put the
// personalities associated with this module in a look-aside
// buffer, we'll try loading them later, maybe when a broken
// dependency is fixed.
static void _KEXTDAddPersonalitiesWithModuleToUnloadedList(KEXTDRef kextd, CFStringRef modName)
{
    CFArrayRef array;
    CFIndex i;
    CFIndex count;
    KEXTD * k;

    if ( !modName )
        return;

    k = (KEXTD *)kextd;

    array = KEXTManagerCopyAllEntities(k->_manager);
    if ( !array )
        return;
    
    // Find personalities which depend on this module.
    count = CFArrayGetCount(array);
    for ( i = 0; i < count; i++ ) {
        CFStringRef type;
        CFStringRef name;
        CFRange range;
        KEXTEntityRef entity;

        entity = (KEXTEntityRef)CFArrayGetValueAtIndex(array, i);
        if ( !entity )
            continue;

        type = KEXTManagerGetEntityType(entity);
        if ( !type || !CFEqual(type, KEXTPersonalityGetEntityType()) ) {
            continue;
        }

        name = KEXTPersonalityGetProperty(entity, CFSTR("CFBundleIdentifier"));
        if ( !name || !CFEqual(modName, name) ) {
            continue;
        }

        range = CFRangeMake(0, CFArrayGetCount(k->_unloaded));

        if ( CFArrayContainsValue(k->_unloaded, range, entity) ) {
            continue;
        }

        CFArrayAppendValue(k->_unloaded, entity);

        if ( !k->_initializing ) {
            KEXTManagerUnloadPersonality(k->_manager, entity);
        }

    }
    CFRelease(array);
}

static void RemovePersonsWithParentFromUnloadedList(void * val, void * context[])
{
    KEXTPersonalityRef person;
    CFMutableArrayRef unloaded;
    CFStringRef parentKey;
    CFStringRef key;

    if ( !val || !context ) {
        return;
    }
    
    person = val;
    unloaded = context[0];
    parentKey = context[1];

    key = CFDictionaryGetValue(person, CFSTR("ParentKey"));
    if ( !parentKey || !key || CFEqual(parentKey, key) ) {
        return;
    }

    CFArrayAppendValue(unloaded, person);
}

// Remove personalities from the unloaded list if their
// associated bundle is removed.
static void _KEXTDRemovePersonalitiesFromUnloadedList(KEXTDRef kextd, CFStringRef parentKey)
{
    CFMutableArrayRef unloaded;
    CFArrayRef array;
    CFRange range;
    void * context[2];
    
    unloaded = ((KEXTD *)kextd)->_unloaded;
    
    array = CFArrayCreateCopy(kCFAllocatorDefault, unloaded);
    CFArrayRemoveAllValues(unloaded);

    context[0] = unloaded;
    context[1] = (void *)parentKey;
    
    range = CFRangeMake(0, CFArrayGetCount(array));
    CFArrayApplyFunction(
                    array,
                    range,
                    (CFArrayApplierFunction)RemovePersonsWithParentFromUnloadedList,
                    context);
    
    CFRelease(array);
}

static KEXTReturn _KEXTDProcessLoadCommand(KEXTDRef kextd, CFStringRef name)
{
    char cname[256];
    KEXTReturn error;
    KEXTD * k;

    k = (KEXTD *)kextd;
    if ( !CFStringGetCString(name, cname, 256, kCFStringEncodingNonLossyASCII) ) {
        error = kKEXTReturnNoMemory;
        return error;
    }
    
    error = KEXTDLoadModule(kextd, name);
    if ( error != kKEXTReturnSuccess &&
         error != kKEXTReturnModuleAlreadyLoaded ) {
        CFDictionaryRef matchingDict;
        const void * keys[1];
        const void * vals[1];

        do {
            keys[0] = CFSTR("CFBundleIdentifier");
            vals[0] = name;

            if ( !vals[0] ) {
                error = kKEXTReturnPropertyNotFound;
                break;
            }

            matchingDict = CFDictionaryCreate(kCFAllocatorDefault, keys, vals, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            if ( !matchingDict ) {
                error = kKEXTReturnNoMemory;
                break;
            }

            error = _KEXTDSendDataToCatalog(kextd, kIOCatalogRemoveDrivers, matchingDict);
            CFRelease(matchingDict);
            if ( error != kKEXTReturnSuccess ) {
                syslog(LOG_DEBUG, "error ( %d) removing drivers.", error);
                break;
            }
        } while ( false );

        // Place personalities which failed to load this module onto
        // a look-aside queue.  We'll try to load the module later
        // when a broken dependency is fixed.
        _KEXTDAddPersonalitiesWithModuleToUnloadedList(kextd, name);
    }
    else {

        error = KERN2KEXTReturn(IOCatalogueModuleLoaded(k->_catPort, cname));
        if ( error != kKEXTReturnSuccess ) {
            syslog(LOG_DEBUG, "error (%d) signalling IOCatalogue.", error);
        }
    }
    
    return error;
}

void KEXTDHangup(KEXTDRef kextd)
{
    KEXTD * k;

    k = (KEXTD *)kextd;
    if ( k->_signalsource ) {
        PTLockTakeLock(k->_runloop_lock);
        CFRunLoopSourceSignal(k->_signalsource);
        CFRunLoopWakeUp(k->_runloop);
        PTLockUnlock(k->_runloop_lock);
    }
}

KEXTReturn KEXTDKernelRequest(KEXTDRef kextd, CFStringRef name)
{
    KEXTD * k;
    KEXTReturn ret;
    
    k = (KEXTD *)kextd;
    ret = kKEXTReturnBadArgument;
    if ( name ) {
        KEXTEvent event;
        CFRange range;
        void * context[3];

        event = kKEXTEventModuleRequest;

        context[0] = kextd;
        context[1] = &event;
        context[2] = (void *)name;

        range = CFRangeMake(0, CFArrayGetCount(k->_helpers));
        CFArrayApplyFunction(k->_helpers, range, (CFArrayApplierFunction)CallHelperEvent, context);
        ret = _KEXTDProcessLoadCommand((KEXTDRef)k, name);
    }

    return ret;
}

// The kernel blocks the thread which entered this
// function until the kernel requests a driver to load.
static void * _KEXTDKmodWait(void * info)
{
    mach_port_t kmodPort;
    kern_return_t kr;
    KEXTD * kextd;
    KEXTReturn error;
    request_t * reqstruct;
    CFStringRef str;
    unsigned int type;

    if ( !info )
        return (void *)kKEXTReturnBadArgument;

    kmodPort = mach_host_self(); /* must be privileged to work */

    kextd = (KEXTD *)info;
    if ( !kextd->_kernelsource )
        return (void *)kKEXTReturnBadArgument;

    while ( 1 ) {
        kmod_args_t data;
        kmod_load_extension_cmd_t * cmd;
        mach_msg_type_number_t dataCount;
        kern_return_t kr;

        data = 0;
        dataCount = 0;
        error = kKEXTReturnSuccess;

        // Wait for kernel to unblock the thread.
        kr = kmod_control(kmodPort, 0, KMOD_CNTL_GET_CMD, &data, &dataCount);
        if ( kr != KERN_SUCCESS ) {
            syslog(LOG_ERR, "error (%d): kmod_control.\n", kr);
            continue;
        }

        cmd = (kmod_load_extension_cmd_t *)data;
	type = cmd->type;
	str = 0;

        switch ( type ) {

            case kIOCatalogMatchIdle: 
		break;

            case KMOD_LOAD_EXTENSION_PACKET: {
                    
                    str = CFStringCreateWithCString(NULL, cmd->name, kCFStringEncodingNonLossyASCII);
		    if( str)
			break;
		    // else fall thru
                }
            default:
                error = kKEXTReturnError;
                break;
        }

	if( error == kKEXTReturnSuccess) {

            reqstruct = (request_t *)malloc(sizeof(request_t));
            if( reqstruct) {
                memset(reqstruct, 0, sizeof(request_t));
                reqstruct->type = cmd->type;
                reqstruct->kmodname = str;
                // queue up a reqest.
                PTLockTakeLock(kextd->_queue_lock);
                queue_enter(&kextd->_requestQ, reqstruct, request_t *, link);
                PTLockUnlock(kextd->_queue_lock);

                // wake up the runloop.
                PTLockTakeLock(kextd->_runloop_lock);
                CFRunLoopSourceSignal(kextd->_kernelsource);
                CFRunLoopWakeUp(kextd->_runloop);
                PTLockUnlock(kextd->_runloop_lock);
            }
	}

        // Deallocate kernel allocated memory.
        vm_deallocate(mach_task_self(), (vm_address_t)data, dataCount);
        if ( kr != KERN_SUCCESS ) {
            syslog(LOG_DEBUG, "vm_deallocate failed. aborting.\n");
            exit(1);
        }
    }
    
    return (void *)kKEXTReturnSuccess;
}


#include <fcntl.h>
#include <paths.h>
#include <unistd.h>
#include <assert.h>
#include <mach/semaphore.h>
#include <mach/sync_policy.h>
#include <mach/bootstrap.h>	/* bootstrap_ports */
#undef _bootstrap_user_		/* XXX FIXME */
#include <servers/bootstrap.h>	/* bootstrap_look_up */
#include <servers/bootstrap_defs.h>

static void
KEXTdaemonSignal(void)
{
    kern_return_t	kr;
    mach_port_t		bs_port;
    semaphore_t		sema;
    static boolean_t	signalled = false;

    if (signalled)
	return;
    signalled = TRUE;
    if (gDebug) {
        printf("kextd: idle\n");
	return;
    }

    kr = task_get_bootstrap_port(mach_task_self(), &bs_port);
    if( kr != KERN_SUCCESS )
        syslog(LOG_ERR, "task_get_bootstrap_port (%lx)\n", kr);
    kr = bootstrap_look_up(bs_port, "kextdsignal", &sema);
    if( kr != BOOTSTRAP_SUCCESS )
        syslog(LOG_ERR, "bootstrap_look_up(%lx)\n", kr);
    kr = semaphore_signal_all( sema );
    if( kr != KERN_SUCCESS )
        syslog(LOG_ERR, "semaphore_signal_all(%lx)\n", kr);

}

static semaphore_t gDaemonSema;

static void
KEXTdaemonWait(void)
{
    kern_return_t	kr;
    mach_timespec_t 	waitTime = { 40, 0 };

    kr = semaphore_timedwait( gDaemonSema, waitTime );
    if( kr != KERN_SUCCESS )
        syslog(LOG_ERR, "semaphore_timedwait(%lx)\n", kr);
}

static int
KEXTdaemon(nochdir, noclose)
	int nochdir, noclose;
{
    kern_return_t	kr;
    mach_port_t		bs_port;
    int fd;

    kr = semaphore_create( mach_task_self(), &gDaemonSema, SYNC_POLICY_FIFO, 0);
    if( kr != KERN_SUCCESS )
        syslog(LOG_ERR, "semaphore_create(%lx)\n", kr);
    kr = task_get_bootstrap_port(mach_task_self(), &bs_port);
    if( kr != KERN_SUCCESS )
        syslog(LOG_ERR, "task_get_bootstrap_port(%lx)\n", kr);
    kr = bootstrap_register(bs_port, "kextdsignal", gDaemonSema);
    if( kr != BOOTSTRAP_SUCCESS )
        syslog(LOG_ERR, "bootstrap_look_up(%lx)\n", kr);

    switch (fork()) {
    case -1:
            return (-1);
    case 0:
            break;
    default:
            KEXTdaemonWait();
            _exit(0);
    }

    if (setsid() == -1)
            return (-1);

    if (!nochdir)
            (void)chdir("/");

    if (!noclose && (fd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1) {
            (void)dup2(fd, STDIN_FILENO);
            (void)dup2(fd, STDOUT_FILENO);
            (void)dup2(fd, STDERR_FILENO);
            if (fd > 2)
                    (void)close (fd);
    }
    return (0);
}


#if TIMERSOURCE
KEXTReturn KEXTDStartMain(KEXTDRef kextd, Boolean beVerbose, Boolean safeBoot, Boolean debug, Boolean poll, CFIndex period, KEXTBootlevel bootlevel)
#else
KEXTReturn KEXTDStartMain(KEXTDRef kextd, Boolean beVerbose, Boolean safeBoot, Boolean debug, KEXTBootlevel bootlevel)
#endif
{
    pthread_attr_t kmod_thread_attr;
    pthread_t kmod_thread;
    KEXTReturn error;
    KEXTD * k;
    CFIndex count;
    CFIndex i;
    CFRunLoopSourceContext sourceContext;
    KEXTManagerBundleLoadingCallbacks bcb = {
        0,
        _KEXTDAuthenticateBundleCB,
        _KEXTDWillAddBundleCB,
        _KEXTDWasAddedBundleCB,
        NULL,
        _KEXTDWillRemoveBundleCB,
        NULL,
    };
    KEXTManagerModuleLoadingCallbacks modcbs = {
        0,
        _KEXTDModuleWillLoadCB,
        _KEXTDModuleWasLoadedCB,
        _KEXTDModuleErrorCB,
        NULL,
        NULL,
    };
    KEXTManagerConfigsCallbacks cfgcbs = {
        0,
        NULL,
        _KEXTDConfigWasAdded,
        NULL,
        _KEXTDConfigWasRemoved,
    };

    gDebug = debug;
    if (!debug) {
        errno = 0;
        KEXTdaemon(0, 0);
        if ( errno ) {
            syslog(LOG_ERR, "failed to daemonize process. Aborting!\n");
            return kKEXTReturnError;
        }
    }

    k = (KEXTD *)kextd;
    k->_manager = KEXTManagerCreate(&bcb, &modcbs, NULL, &cfgcbs, kextd,
        &logErrorFunction, &logMessageFunction, safeBoot, &error);
    if ( !k->_manager )
        return error;

    k->_initializing = true;
    k->_catPort = _KEXTManagerGetMachPort(k->_manager);
    k->_beVerbose = beVerbose;
#if TIMERSOURCE
    k->_pollFileSystem = poll;
    k->_pollingPeriod = period;
#endif
    memset(&sourceContext, NULL, sizeof(CFRunLoopSourceContext));

    error = _KEXTDInitSyslog(k);
    if ( error != kKEXTReturnSuccess ) {
        return error;
    }

    _kextd = kextd;

    // FIXME: Need a way to make this synchronous!
    error = KERN2KEXTReturn(IOCatalogueSendData(k->_catPort, kIOCatalogRemoveKernelLinker, 0, 0));
    if (error != kKEXTReturnSuccess) {
        syslog(LOG_ERR, "couldn't remove linker from kernel (may have been removed already).",
            error);
        // this is only serious the first time kextd launches....
        // FIXME: how exactly should we handle this? Create a separate program
        // to trigger KLD unload?
    }
    
    signal(SIGHUP, signalhandler);

    k->_runloop = CFRunLoopGetCurrent();
    if ( !k->_runloop ) {
        syslog(LOG_ERR, "error allocating runloop.\n");
        return NULL;
    }

    sourceContext.version = 0;
    sourceContext.info = k;
    sourceContext.perform = _KEXTDSIGHUPCallout;
    k->_signalsource = CFRunLoopSourceCreate(kCFAllocatorDefault, 1, &sourceContext);
    if ( !k->_signalsource ) {
        syslog(LOG_ERR, "error allocating signal runloop source.\n");
        return NULL;
    }
    CFRunLoopAddSource(k->_runloop, k->_signalsource, kCFRunLoopDefaultMode);

    sourceContext.perform = _KEXTDPerform;
    k->_kernelsource = CFRunLoopSourceCreate(kCFAllocatorDefault, 2, &sourceContext);
    if ( !k->_kernelsource ) {
        syslog(LOG_ERR, "error allocating kernel runloop source.\n");
        return NULL;
    }
    CFRunLoopAddSource(k->_runloop, k->_kernelsource, kCFRunLoopDefaultMode);

    count = CFArrayGetCount(k->_helpers);
    for ( i = 0; i < count; i++ ) {
        KEXTDHelper * helper;

        helper = (KEXTDHelper *)CFArrayGetValueAtIndex(k->_helpers, i);
        if ( !helper )
            continue;

        if ( helper->cbs.DaemonDidFinishLaunching )
            helper->cbs.DaemonDidFinishLaunching(helper->context);
    }

    // Fork off the kmod_control message thread.
    pthread_attr_init(&kmod_thread_attr);
    pthread_create(&kmod_thread, &kmod_thread_attr, _KEXTDKmodWait, kextd);
    pthread_detach(kmod_thread);

    syslog(LOG_INFO, "started.");

    IOCatalogueReset(k->_catPort, kIOCatalogResetDefault);
    KEXTDScanPaths(kextd);

#if TIMERSOURCE
    if ( poll ) {
        CFRunLoopTimerRef timer;
        CFRunLoopTimerContext timerContext = {
            0, kextd, NULL, NULL, NULL,
        };

        timer = CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent(), period, 0, 10, _KEXTDTimerCallout, &timerContext);
        if ( !timer ) {
            syslog(LOG_ERR, "error allocating kmod runloop timer.\n");
            return kKEXTReturnError;
        }

        CFRunLoopAddTimer(k->_runloop, timer, kCFRunLoopDefaultMode);
        CFRelease(timer);
    }
#endif

    if ( (error = _KEXTDSendPersonalities(kextd, bootlevel)) != kKEXTReturnSuccess ) {
        // KEXTError(error, CFSTR("Error sending personalities to IOCatalogue"));
        syslog(LOG_ERR, "error (%d) sending personalities to IOCatalogue.", error);
        return error;
    }

    k->_initializing = false;

    CFRunLoopRun();
    
    return kKEXTReturnSuccess;
}

void KEXTDScanPaths(KEXTDRef kextd)
{
    KEXTReturn error;
    KEXTD * k;
    CFIndex count;
    CFIndex i;

    k = (KEXTD *)kextd;
    if ( !k->_manager )
        return;
    
    count = CFArrayGetCount(k->_scanPaths);
    for ( i = 0; i < count; i++ ) {
        CFURLRef url;

        url = (CFURLRef)CFArrayGetValueAtIndex(k->_scanPaths, i);

        if ( url ) {
            if ( k->_beVerbose ) {
                CFStringRef cfstr;
                char str[256];

                cfstr = CFURLGetString(url);
                if ( CFStringGetCString(cfstr, str, 256, kCFStringEncodingNonLossyASCII) ) {
                    syslog(LOG_INFO, "scanning: %s.", str);
                }
            }
            error = KEXTManagerScanPath(k->_manager, url);
            if ( error != kKEXTReturnSuccess ) {
                syslog(LOG_ERR, "error (%d) scanning path.\n", error);
            }
#if LOOKAPPLENDRV
	    do {
                CFURLRef	path;
		CFArrayRef	array;
		CFIndex		count, index;
                SInt32		err;

                path = CFURLCreateCopyAppendingPathComponent(
                                    kCFAllocatorDefault,
                                    url,
                                    CFSTR("AppleNDRV"),
                                    TRUE);
                if ( !path )
		    continue;
                array = (CFArrayRef)IOURLCreatePropertyFromResource(
                                            kCFAllocatorDefault, path,
                                            kIOURLFileDirectoryContents,
                                            &err);
		CFRelease( path );
                if ( !array )
		    continue;

                count = CFArrayGetCount(array);
                for ( index = 0; index < count; index++ ) {
                    CFURLRef file;
                    file = (CFURLRef) CFArrayGetValueAtIndex(array, index);
                    if ( !file )
                        continue;
                    PEFExamineFile( k->_catPort, file );
		}
		CFRelease(array);

            } while( false );
#endif /* LOOKAPPLENDRV */
	}
    }
}

void KEXTDAddScanPath(KEXTDRef kextd, CFURLRef path)
{
    if ( !kextd || !path )
        return;

    if ( CFURLGetTypeID() != CFGetTypeID(path) )
        return;

    CFArrayAppendValue(((KEXTD *)kextd)->_scanPaths, path);
}

void KEXTDRegisterHelperCallbacks(KEXTDRef kextd, KEXTDHelperCallbacks * callbacks)
{
    KEXTD * k;
    KEXTDHelper * helper;

    if ( !kextd || !callbacks )
        return;
    
    k = (KEXTD *)kextd;
    helper = (KEXTDHelper *)malloc(sizeof(KEXTDHelper));
    if ( !helper )
        return;

    helper->cbs.HelperInitialize = callbacks->HelperInitialize;
    helper->cbs.HelperFinalize = callbacks->HelperFinalize;
    helper->cbs.DaemonDidFinishLaunching = callbacks->DaemonDidFinishLaunching;
    helper->cbs.DaemonWillTerminate = callbacks->DaemonWillTerminate;
    helper->cbs.BundleAdd = callbacks->BundleAdd;
    helper->cbs.BundleRemove = callbacks->BundleRemove;
    helper->cbs.EventOccurred = callbacks->EventOccurred;
    helper->cbs.ModuleWillLoad = callbacks->ModuleWillLoad;
    helper->cbs.ModuleWasLoaded = callbacks->ModuleWasLoaded;
    helper->cbs.ModuleLoadError = callbacks->ModuleLoadError;

    helper->context = helper->cbs.HelperInitialize(kextd);

    CFArrayAppendValue(k->_helpers, helper);
}

KEXTReturn KEXTDLoadModule(KEXTDRef kextd, CFStringRef moduleName)
{
    KEXTD * k = (KEXTD *)kextd;
    KEXTModuleRef module;

    if ( !kextd || !moduleName )
        return kKEXTReturnBadArgument;
    
    module = KEXTManagerGetModule(k->_manager, moduleName);
    if ( !module )
        return kKEXTReturnModuleNotFound;

    return KEXTManagerLoadModule(k->_manager, module);
}


