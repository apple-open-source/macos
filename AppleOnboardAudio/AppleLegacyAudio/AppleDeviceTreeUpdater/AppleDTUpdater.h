#include <IOKit/IOService.h>

class AudioDeviceTreeUpdater : public IOService
{
    OSDeclareDefaultStructors(AudioDeviceTreeUpdater)

protected:
    virtual void mergeProperties(OSObject* inDest, OSObject* inSrc);

    // IOService method overrides
public:
    virtual bool start(IOService * provider );
};

