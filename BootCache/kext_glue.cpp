//
// Glue to make IO Kit happy with BootCache as a non-NKE KEXT.
//

#include <IOKit/IOService.h>

/*
 * Hooks from c++ glue to the cache core.
 */
extern "C" void	BC_start(void);
extern "C" int	BC_stop(void);

class com_apple_BootCache : public IOService
{
	OSDeclareDefaultStructors(com_apple_BootCache);

public:
	virtual bool	start(IOService *provider);
	virtual void	stop(IOService *provider);
};

OSDefineMetaClassAndStructors(com_apple_BootCache, IOService);

bool
com_apple_BootCache::start(IOService *provider)
{
	bool	result;

	result = IOService::start(provider);
	if (result == true) {
		BC_start();
		provider->retain();
	}
	return(result);
}

void
com_apple_BootCache::stop(IOService *provider)
{
	if (BC_stop())
		return;	// refuse unload?
	provider->release();
	IOService::stop(provider);
}
