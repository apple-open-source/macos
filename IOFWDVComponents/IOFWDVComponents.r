
#include "MacTypes.r"
#include "IOFWDVComponents.h"

#define forCarbon			1
#define UseExtendedThingResource	1
#include "Components.r"

resource 'thng' (kIsocCodecThing, "DV_IHandler")
{
    'ihlr',
    'dv  ',
    'appl',
    0, 					// 0x80000000 cmpWantsRegisterMessage,
    kAnyComponentFlagsMask,
    'dlle', kIsocCodecBaseID,
    'STR ', kIsocCodecNameID,
    'STR ', kIsocCodecNameID,
    '    ', 0,
    0x30000,
    // component registration flags
    componentHasMultiplePlatforms, // | componentDoAutoVersion | componentWantsUnregister,
    0, 					// resource id of icon family
    {
//        cmpWantsRegisterMessage  |
            0,		// component flags
        'dlle', kIsocCodecBaseID, platformPowerPCNativeEntryPoint,
            0,		// component flags
        'dlle', kIsocCodecBaseID, platformIA32NativeEntryPoint,
    }
};

resource 'dlle' (kIsocCodecBaseID)
{
	"FWDVICodecComponentDispatch"
};

resource 'thng' (kControlCodecThing, "DV_DCHandler")
{
    'devc',
    'fwdv',
    'appl',
    0, 					// 0x80000000 cmpWantsRegisterMessage,
    kAnyComponentFlagsMask,
    'dlle', kControlCodecBaseID,
    'STR ', kControlCodecNameID,
    'STR ', kControlCodecNameID,
    '    ', 0,
    0x30000,
    // component registration flags
    componentHasMultiplePlatforms, // | componentDoAutoVersion | componentWantsUnregister,
    0, 					// resource id of icon family
    {
//        cmpWantsRegisterMessage  |
            0,		// component flags
        'dlle', kControlCodecBaseID, platformPowerPCNativeEntryPoint,
            0,		// component flags
        'dlle', kControlCodecBaseID, platformIA32NativeEntryPoint,
    }
};

resource 'dlle' (kControlCodecBaseID)
{
	"FWDVCCodecComponentDispatch"
};


