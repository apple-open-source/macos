#ifndef _APPLEEHCIHUBINFO_H
#define _APPLEEHCIHUBINFO_H

// this structure is used to monitor the hubs which are attached. there will
// be an instance of this structure for every high speed hub with a FS/LS
// device attached to it. If the hub is in single TT mode, then there will
// just be one instance on port 0. If the hub is in multi-TT mode, then there
// will be that instance AND an instance for each active port
// note that this is NOT an OSObject because the overhead (data + vtable) 
// would be too high.

typedef struct AppleUSBEHCIHubInfo AppleUSBEHCIHubInfo, *AppleUSBEHCIHubInfoPtr;

struct AppleUSBEHCIHubInfo
{
    AppleUSBEHCIHubInfoPtr	next;
    UInt32			flags;
    UInt16			bandwidthAvailable;
    UInt8			hubAddr;
    UInt8			hubPort;
};

enum
{
    kUSBEHCIFlagsMuliTT		= 0x0001
};

#endif
