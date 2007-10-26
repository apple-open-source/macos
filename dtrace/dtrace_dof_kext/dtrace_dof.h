#include <IOKit/IOService.h>
class com_apple_driver_dtraceDOF : public IOService
{
OSDeclareDefaultStructors(com_apple_driver_dtraceDOF)
public:
    virtual bool init(OSDictionary *dictionary = 0);
    virtual void free(void);
    virtual IOService *probe(IOService *provider, SInt32 *score);
    virtual bool start(IOService *provider);
    virtual void stop(IOService *provider);
};