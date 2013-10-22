/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef __AppleSmartBatteryManager__
#define __AppleSmartBatteryManager__

#include <IOKit/IOService.h>
#include <IOKit/smbus/IOSMBusController.h>
#include "AppleSmartBattery.h"
#include "AppleSmartBatteryManagerUserClient.h"


void BattLog(const char *fmt, ...);

class AppleSmartBattery;
class AppleSmartBatteryManagerUserClient;

/*
 * Support for external transactions (from user space)
 */
enum {
    kEXDefaultBatterySelector = 0,
    kEXManagerSelector = 1
};

enum {
    kEXReadWord     = 0,
    kEXWriteWord    = 1,
    kEXReadBlock    = 2,
    kEXWriteBlock   = 3,
    kEXReadByte     = 4,
    kEXWriteByte    = 5,
    kEXSendByte     = 6
};    

enum {
    kEXFlagRetry = 1
};

#define MAX_SMBUS_DATA_SIZE     32

// * WriteBlock note
// rdar://5433060
// For block writes, clients always increment inByteCount +1 
// greater than the actual byte count. (e.g. 32 bytes to write becomes a 33 byte count.)
// Other types of transactions are not affected by this workaround.
typedef struct {
    uint8_t         flags;
    uint8_t         type;
    uint8_t         batterySelector;
    uint8_t         address;
    uint8_t         inByteCount;
    uint8_t         inBuf[MAX_SMBUS_DATA_SIZE];
} EXSMBUSInputStruct;

typedef struct {
    uint32_t        status;
    uint32_t        outByteCount;
    uint32_t        outBuf[MAX_SMBUS_DATA_SIZE];
} EXSMBUSOutputStruct;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class AppleSmartBatteryManager : public IOService {    
    friend class AppleSmartBatteryManagerUserClient;
    
    OSDeclareDefaultStructors(AppleSmartBatteryManager)
    
public:
    bool start(IOService *provider);
    
    IOReturn performTransaction(IOSMBusTransaction * transaction,
				    IOSMBusTransactionCompletion completion = 0,
				    OSObject * target = 0,
				    void * reference = 0);

    IOReturn setPowerState(unsigned long which, IOService *whom);

    IOReturn message(UInt32 type, IOService *provider, void * argument);

    // Called by AppleSmartBattery
    // Re-enables AC inflow if appropriate
    void handleFullDischarge(void);
    
    // bool argument true means "set", false means "clear" exclusive acces
    // return: false means "exclusive access already granted", "true" means success
    bool requestExclusiveSMBusAccess(bool request);
    
    // Returns true if an exclusive AppleSmartBatteryUserClient is attached. False otherwise.
    bool hasExclusiveClient(void);
    
    bool requestPoll(int type);
    
private:
    // Called by AppleSmartBatteryManagerUserClient
    IOReturn inhibitCharging(int level);        

    // Called by AppleSmartBatteryManagerUserClient
    IOReturn disableInflow(int level);

    // Called by AppleSmartBatteryManagerUserClient
    // Called by Battery Updater application
    IOReturn performExternalTransaction( 
                        void            *in,    // struct EXSMBUSInputStruct
                        void            *out,   // struct EXSMBUSOutputStruct
                        IOByteCount     inSize,
                        IOByteCount     *outSize);

    IOReturn performExternalTransactionGated(void *arg0, void *arg1,
                                             void *arg2, void *arg3);

    void    gatedSendCommand(int cmd, int level, IOReturn *ret_code);

    // transactionCompletion is the guts of the state machine
    bool    transactionCompletion(void *ref, IOSMBusTransaction *transaction);

private:
    IOSMBusTransaction          fTransaction;
    IOCommandGate               * fBatteryGate;
    IOCommandGate               * fManagerGate;
    IOSMBusController           * fProvider;
    AppleSmartBattery           * fBattery;
    bool                        fExclusiveUserClient;
};

#endif
