
#ifndef __KEXTD_H_
#define __KEXTD_H_

#include <IOKit/kext/KEXTManager.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct __KEXTD * KEXTDRef;

typedef enum {
    kKEXTBootlevelNormal     = 0x00,
    kKEXTBootlevelExempt     = 0x01,
    kKEXTBootlevelRecovery   = 0x02,
    kKEXTBootlevelSingleUser = 0x04,
    kKEXTBootlevelFlexible   = 0x08,
    kKEXTBootlevelRequired   = 0x10,
} KEXTBootlevel;

typedef enum {
    kKEXTEventReset,
    kKEXTEventModuleRequest,
    kKEXTEventPersonalityRequest,
    kKEXTEventBundleAuthenticationFailed,
} KEXTEvent;

typedef struct {
    CFIndex version;
    void *	(*HelperInitialize)(KEXTDRef kextd);
    void	(*HelperFinalize)(void * context);
    void	(*DaemonDidFinishLaunching)(void * context);
    void	(*DaemonWillTerminate)(void * context);
    Boolean	(*BundleAdd)(KEXTBundleRef bundle, void * context);
    Boolean	(*BundleRemove)(KEXTBundleRef bundle, void * context);
    void	(*EventOccurred)(KEXTEvent event, CFTypeRef data, void * context);
    Boolean	(*ModuleWillLoad)(KEXTModuleRef module, void * context);
    void	(*ModuleWasLoaded)(KEXTModuleRef module, void * context);
    KEXTReturn	(*ModuleLoadError)(KEXTModuleRef module, KEXTReturn error, void * context);
} KEXTDHelperCallbacks;


KEXTDRef	KEXTDCreate(CFArrayRef scanPaths, KEXTReturn * error);
void		KEXTDFree(KEXTDRef kextd);

void            KEXTDHangup(KEXTDRef kextd);
void		KEXTDReset(KEXTDRef kextd);
#if TIMERSOURCE
KEXTReturn    	KEXTDStartMain(KEXTDRef kextd, Boolean beVerbose, Boolean safeBoot, Boolean debug, Boolean poll, CFIndex period, KEXTBootlevel bootlevel, Boolean cdMKextBoot);
#else
KEXTReturn    	KEXTDStartMain(KEXTDRef kextd, Boolean beVerbose, Boolean safeBoot, Boolean debug, KEXTBootlevel bootlevel, Boolean cdMKextBoot);
#endif
void		KEXTDScanPaths(KEXTDRef kextd, Boolean cdMKextBoot);
void		KEXTDAddScanPath(KEXTDRef kextd, CFURLRef path);
void		KEXTDRegisterHelperCallbacks(KEXTDRef kextd, KEXTDHelperCallbacks * callbacks);
KEXTReturn      KEXTDKernelRequest(KEXTDRef kextd, CFStringRef moduleName);
KEXTReturn	KEXTDLoadModule(KEXTDRef kextd, CFStringRef moduleName);

#if defined(__cplusplus)
} /* "C" */
#endif

#endif __KEXTD_H_

