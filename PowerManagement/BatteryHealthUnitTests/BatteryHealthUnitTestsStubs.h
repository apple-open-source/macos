//
//  BatteryHealthUnitTestsStubs.h
//  PowerManagement
//
//  Created by Faramola Isiaka on 11/18/20.
//

#ifndef BatteryHealthUnitTestsStubs_h
#define BatteryHealthUnitTestsStubs_h

#define kMsgReturnCode 1
#define kSetBHUpdateTimeDelta 1
#define kPSAdapterDetails 1
#define kSetPermFaultStatus 1
#define kReadPersistentBHData ""

extern uint64_t currentTime;

void InternalEvalConnections(void)
{
    return;
}

void InternalEvaluateAssertions(void)
{
    return;
}

void evalTcpkaForPSChange(__unused int pwrSrc)
{
    return;
}

void PMSettingsPSChange(void)
{
    return;
}

void UPSLowPowerPSChange(void)
{
    return;
}

void SystemLoadBatteriesHaveChanged(__unused int count)
{
    return;
}

void initializeBatteryDataCollection(void)
{
    return;
}

void evalProxForPSChange(void)
{
    return;
}

uint64_t getMonotonicContinuousTime()
{
    return currentTime;
}

bool auditTokenHasEntitlement(__unused audit_token_t token, __unused CFStringRef entitlement)
{
    return true;
}

bool PMStoreSetValue(__unused CFStringRef key, __unused CFTypeRef value)
{
    return true;
}

bool isSenderEntitled(__unused xpc_object_t remoteConnection, __unused CFStringRef __unused entitlementString, __unused bool requireRoot)
{
    return true;
}

void audit_token_to_au32(__unused audit_token_t     atoken,
                           __unused uid_t        *auidp,
                           __unused uid_t        *euidp,
                           __unused gid_t        *egidp,
                           __unused uid_t        *ruidp,
                           __unused gid_t        *rgidp,
                           __unused pid_t        *pidp,
                           __unused au_asid_t    *asidp,
                           __unused au_tid_t    *tidp)
{
    return;
}

dispatch_queue_t _getPMMainQueue(void)
{
    return NULL;
}

CFTypeRef _copyRootDomainProperty(__unused CFStringRef key)
{
    return NULL;
}

void recordFDREvent(__unused int eventType, __unused bool checkStandbyStatus)
{
    return;
}

void LogObjectRetainCount(__unused const char* str, __unused io_object_t msg_port)
{
    return;
}

void logASLBatteryHealthChanged(__unused const char *health,
                                __unused const char *oldhealth,
                                __unused const char *reason)
{
    return;
}

void logASLLowBatteryWarning(__unused IOPSLowBatteryWarningLevel level, __unused int time, __unused int ccap)
{
    return;
}

void evaluateClamshellSleepState(void)
{
    return;
}

#endif /* BatteryHealthUnitTestsStubs_h */
