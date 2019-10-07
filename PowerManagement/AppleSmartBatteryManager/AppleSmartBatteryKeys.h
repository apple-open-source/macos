#include <TargetConditionals.h>

#ifndef __AppleSmartBatteryKeys__
#define __AppleSmartBatteryKeys__

#if !TARGET_OS_BRIDGE && !TARGET_OS_OSX
// Battery Flags
enum {
    kAsbBatteryFlagDischarge = (1 << 0),
    kAsbBatteryFlagSocF = (1 << 1),
    kAsbBatteryFlagSoc1 = (1 << 2),
    kAsbBatteryFlagImaxIrq = (1 << 3),
    kAsbBatteryFlagLowV = (1 << 4),
    kAsbBatteryFlagCcaReq = (1 << 5),
    kAsbBatteryFlagOverTemp = (1 << 6),
    kAsbBatteryFlagOcvTaken = (1 << 7),
    kAsbBatteryFlagItLock = (1 << 8),
    kAsbBatteryFlagFc = (1 << 9),
    kAsbBatteryFlagTc1 = (1 << 10),
    kAsbBatteryFlagTc2 = (1 << 11),
    kAsbBatteryFlagCalmode = (1 << 12),
    kAsbBatteryFlagImaxOk = (1 << 13),
    kAsbBatteryFlagFastQUp = (1 << 14),
    kAsbBatteryFlagFsAct = (1 << 15),
    kAsbBatteryFlagSoc2 = (1 << 16),
    kAsbBatteryFlagSoc3 = (1 << 17),
    kAsbBatteryFlagNrs = (1 << 18),
};
#endif /* #if !TARGET_OS_BRIDGE && !TARGET_OS_OSX */

#endif /* ! __AppleSmartBatteryKeys */
