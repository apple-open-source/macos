/*
     File:       DeviceControlPriv.h
 
     Contains:   xxx put contents here xxx
 
     Version:    xxx put version here xxx
 
     DRI:        xxx put dri here xxx
 
     Copyright:  (c) 1999-2000 by Apple Computer, Inc., all rights reserved.
 
     BuildInfo:  Built by:            wgulland
                 On:                  Thu Sep 21 14:47:23 2000
                 With Interfacer:     3.0d20e4 (Mac OS X for PowerPC)
                 From:                DeviceControlPriv.i
                     Revision:        3
                     Dated:           6/15/99
                     Last change by:  KW
                     Last comment:    fix screwup
 
     Bugs:       Report bugs to Radar component "System Interfaces", "Latest"
                 List the version information (from above) in the Problem Description.
 
*/
#ifndef __DEVICECONTROLPRIV__
#define __DEVICECONTROLPRIV__

#ifndef __DEVICECONTROL__
#include "DeviceControl.h"
#endif






#if PRAGMA_ONCE
#pragma once
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if PRAGMA_IMPORT
#pragma import on
#endif

#if PRAGMA_STRUCT_ALIGN
    #pragma options align=mac68k
#elif PRAGMA_STRUCT_PACKPUSH
    #pragma pack(push, 2)
#elif PRAGMA_STRUCT_PACK
    #pragma pack(2)
#endif

typedef UInt32                          DeviceConnectionID;
enum {
    kDeviceControlComponentType = FOUR_CHAR_CODE('devc'),       /* Component type */
    kDeviceControlSubtypeFWDV   = FOUR_CHAR_CODE('fwdv')        /* Component subtype */
};




/* Private calls made by the Isoc component */

EXTERN_API( ComponentResult )
DeviceControlEnableAVCTransactions (ComponentInstance   instance)                           FIVEWORDINLINE(0x2F3C, 0x0000, 0x0100, 0x7000, 0xA82A);

EXTERN_API( ComponentResult )
DeviceControlDisableAVCTransactions (ComponentInstance  instance)                           FIVEWORDINLINE(0x2F3C, 0x0000, 0x0101, 0x7000, 0xA82A);

EXTERN_API( ComponentResult )
DeviceControlSetDeviceConnectionID (ComponentInstance   instance,
                                 DeviceConnectionID     connectionID)                       FIVEWORDINLINE(0x2F3C, 0x0004, 0x0102, 0x7000, 0xA82A);

EXTERN_API( ComponentResult )
DeviceControlGetDeviceConnectionID (ComponentInstance   instance,
                                 DeviceConnectionID *   connectionID)                       FIVEWORDINLINE(0x2F3C, 0x0004, 0x0103, 0x7000, 0xA82A);


/* selectors for component calls */
enum {
    kDeviceControlEnableAVCTransactionsSelect  = 0x0100,
    kDeviceControlDisableAVCTransactionsSelect = 0x0101,
    kDeviceControlSetDeviceConnectionIDSelect  = 0x0102,
    kDeviceControlGetDeviceConnectionIDSelect  = 0x0103
};

#if PRAGMA_STRUCT_ALIGN
    #pragma options align=reset
#elif PRAGMA_STRUCT_PACKPUSH
    #pragma pack(pop)
#elif PRAGMA_STRUCT_PACK
    #pragma pack()
#endif

#ifdef PRAGMA_IMPORT_OFF
#pragma import off
#elif PRAGMA_IMPORT
#pragma import reset
#endif

#ifdef __cplusplus
}
#endif

#endif /* __DEVICECONTROLPRIV__ */

