#ifndef _APPLEMEDIABAY_H_
#define _APPLEMEDIABAY_H_

#include <IOKit/IOService.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/platform/AppleMacIODevice.h>

// Uncomment the following define to see the driver logs:
//#define APPLEMB_VERBOSE



// Forward delcaration for the client class
class AppleMediaBayATANub;

class AppleMediaBay : public IOService
{
    OSDeclareDefaultStructors(AppleMediaBay)

private:
    // Definitions of the possible chpiset
    // for this media bay:
    typedef enum {
        kMBControllerUndefined = -1,
        kMBControllerOHare     = 0,
        kMBControllerHeathrow  = 1,
        kMBControllerKeyLargo  = 2
    } ControllerType;
    
    // Bit Masks
    typedef enum {
        mediaBayIDMask          = 0x00700000,
        mediaBayIDMask_KeyLargo = 0x70000000,
    } MediaBayMask;

    // Device types:
    typedef enum {
        deviceAutoFloppy        = 0,
        deviceManualFloppy      = 1,
        deviceSound             = 2,
        deviceATA               = 3,
        device4                 = 4,   
        devicePCI               = 5,
        devicePower             = 6,   
        deviceNone              = 7
    } MediaBayDeviceType;
    
    // Power control bits:
    typedef enum {
    	mbSoundOn               = 0x08,
	floppyOn                = 0x04,
	ATAOn                   = 0x02,
	PCIOn                   = 0x01,
	mbOff                   = 0
    } PowerBit;
    
    // Commands accpeted by the command gate:
    typedef enum {
        registerNub             = 1,
        deRegisterNub           = 2,
        powerOn                 = 3,
        powerOff                = 4
    };

    ControllerType		mbControllerType;
    
    MediaBayDeviceType          mbCurrentDevice;
    MediaBayDeviceType          mbPreviousDevice;
        
    IOWorkLoop			*workloop;
    IOInterruptEventSource 	*intSource;
    IOCommandGate               *commandGate;
    
    IOMemoryMap                 *configAddrMap; 
    volatile UInt32             *configAddr;
    
    // symbol to power the media bay:
    const OSSymbol 	        *powerMediaBay;
    
    // This is the clients the media bay can talk to the ATA Nub.
    AppleMediaBayATANub         *ioClient;
    
    // The io provider for the media bay:
    IOService 		        *myMacIO;
        
    // This is the local interrupt handler. It does not do anything
    // special, just defers the call to the interrupt handler in the
    // AppleMediaBay object:
    static void handleInterrupt(OSObject *owner, IOInterruptEventSource *src, int count);
    
    // Thie is to call the real dispatcher for the commands to the
    // media bay driver.
    static IOReturn commandGateCaller(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3r);
    
protected:
    virtual MediaBayDeviceType readMBID(void);

    // Interrupt handler/dispatcher. When a device is inserted (or removed)
    // from the media bay this dispatches the calls to the right method:
    virtual void dispatchInterrupt();
    
    // Dispatches commands to the ATA the arguments are the command ID
    // and three possible pointers:
    IOReturn dispatchMBCommand(int commandID, void *arg1, void *arg2, void *arg3r);
    
    // This are the teo handlers for the (only) two events that the
    // MB can generate:
    virtual void handleDeviceInsertion();
    virtual void handleDeviceEjection();

    // Sets the power on the given device off or on:
    virtual void setMediaBayDevicePower(bool powerUp, MediaBayDeviceType thisDevice);
    
    // This allows the ATA Nub to register with the media bay driver:
    virtual bool registerMBClient(AppleMediaBayATANub *registerMe);
    virtual bool deRegisterMBClient(AppleMediaBayATANub *deRegisterMe);
    
public:
    virtual bool init(OSDictionary *dictionary = 0);
    virtual void free(void);
    virtual bool start(IOService *provider);
    
    // The following set of functions return the
    // current mediabay device. Since we do not
    // wish to publish the interal IDs (that may
    // change) we have a method for each possible
    // device:
    virtual bool isFloppyOn(void);
    virtual bool isATAOn(void);
    virtual bool isPCIOn(void);
    virtual bool isSoundOn(void);
    virtual bool isPowerOn(void);
    
    // Power managment functions:
    bool initForPM(IOService *provider);
    IOReturn setPowerState(unsigned long powerStateOrdinal, IOService* whatDevice);
    
    // Public methods to change the state of the driver
    IOReturn registerMediaNub(AppleMacIODevice *registerMe);
    IOReturn deRegisterMediaNub(AppleMacIODevice *deRegisterMe);
};

#endif _APPLEMEDIABAY_H_

