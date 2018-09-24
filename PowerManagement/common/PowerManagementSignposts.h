//
//  PowerManagementSignposts.h
//  PowerManagement
//
//  Created by John Scheible on 5/11/18.
//

#ifndef PowerManagementSignposts_h
#define PowerManagementSignposts_h

#include <sys/kdebug.h>
#include <IOKit/IOTimeStamp.h>

#define SYSTEMCHARGING_ARIADNE_SUBCLASS  103
#define PROJECT_SHIFT 8
#define PROJECT_CODE 0x02
#define SYSTEMCHARGINGDBG_CODE(event) ARIADNEDBG_CODE(SYSTEMCHARGING_ARIADNE_SUBCLASS, (PROJECT_CODE << PROJECT_SHIFT) | event)

// AppleSmartBatteryManager
#define SYSTEMCHARGING_ASBM_READ_EXTERNAL_CONNECTED       SYSTEMCHARGINGDBG_CODE(0x00) // Impulse

#endif /* PowerManagementSignposts_h */
