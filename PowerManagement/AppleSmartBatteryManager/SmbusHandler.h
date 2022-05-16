//
//  SmbusHandler.h
//  PowerManagement
//
//  Created by prasadlv on 11/8/16.
//
//

#ifndef SmbusHandler_h
#define SmbusHandler_h

#include <IOKit/acpi/IOACPIPlatformDevice.h>
#include <libkern/c++/OSObject.h>
#include "AppleSmartBatteryCommands.h"
#include "AppleSmartBatteryManager.h"

#define MAX_SMBUS_DATA_SIZE 32

class AppleSmartBatteryManager;


class SmbusHandler : public OSObject
{
    
OSDeclareDefaultStructors(SmbusHandler)

private:
    int         fRetryAttempts;
    int         fExternalTransactionWait;
    
    AppleSmartBatteryManager    *fMgr;
    IOWorkLoop                  *fWorkLoop;

    ASBMgrTransactionCompletion     fCompletion;
    bool                            fFullyDischarged;
    OSObject                        *fTarget;
    void                            *fReference;
    IOSMBusTransaction              fTransaction;
    ASBMgrOpType                    fOpType;
    uint32_t                        fCmdCount;

    IOACPIPlatformDevice            *fACPIProvider;

    void smbusCompletion(void *ref, IOSMBusTransaction *transaction);
    void smbusExternalTransactionCompletion(void *ref, IOSMBusTransaction *transaction);
    IOReturn getErrorCode(IOSMBusStatus status);

public:

    IOReturn initialize ( AppleSmartBatteryManager *mgr );
    uint32_t requiresRetryGetMicroSec(IOSMBusTransaction *transaction);
    IOReturn isTransactionAllowed();
    IOReturn performTransaction(ASBMgrRequest *req, ASBMgrTransactionCompletion completion, OSObject * target, void * reference);

    /*
     * smbusExternalTransaction - Handles smbus transactions received from user clients.
     * This call is blocked until command is completed.
     */
    IOReturn smbusExternalTransaction(void *in, void *out, IOByteCount inSize, IOByteCount *outSize);
    void handleExclusiveAccess(bool exclusive);
    IOReturn inhibitCharging(int level);
    IOReturn disableInflow(int level);


};

#endif /* SmbusHandler_h */
