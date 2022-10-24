//
//  IOKitPMStubs.m
//  IOKitPMUnitTests
//
//  Created by Faramola Isiaka on 7/15/21.
//

#import <Foundation/Foundation.h>
#include "IOKitPMStubs.h"

CFTypeRef IORegistryEntryCreateCFProperty(__unused io_registry_entry_t entry, __unused CFStringRef key, __unused CFAllocatorRef allocator, __unused IOOptionBits options)
{
    return NULL;
}

dispatch_queue_t  getPMQueue()
{
    return NULL;
}

io_registry_entry_t  getPMRootDomainRef(void)
{
    return 0;
}

IOReturn _pm_connect(__unused mach_port_t *newConnection)
{
    return 0;
}

IOReturn _pm_disconnect(__unused mach_port_t connection)
{
    return 0;
}

kern_return_t io_pm_assertion_activity_aggregate(__unused mach_port_t server, __unused vm_offset_t *statsData, __unused mach_msg_type_number_t *statsDataCnt, __unused int *return_code)
{
    return 0;
}

kern_return_t io_pm_assertion_activity_log(__unused mach_port_t server, __unused vm_offset_t *log, __unused mach_msg_type_number_t *logCnt, __unused uint32_t *entryCnt, __unused uint32_t *overflow, __unused int *return_code)
{
    return 0;
}

kern_return_t io_pm_assertion_copy_details(__unused mach_port_t server, __unused int assertion_id, __unused int whichData, __unused vm_offset_t props, __unused mach_msg_type_number_t propsCnt, __unused vm_offset_t *assertions, __unused mach_msg_type_number_t *assertionsCnt, __unused int *return_val)
{
    return 0;
}

kern_return_t io_pm_assertion_create(__unused mach_port_t server, __unused vm_offset_t props, __unused mach_msg_type_number_t propsCnt, __unused int *assertion_id, __unused int *disableAppSleep, __unused int *enTrIntensity, __unused int *return_code)
{
    return 0;
}

kern_return_t io_pm_assertion_retain_release(__unused mach_port_t server, __unused int assertion_id, __unused int action, __unused int *retainCnt, __unused int *disableAppSleep, __unused int *enableAppSleep, __unused int *return_code)
{
    return 0;
}

kern_return_t io_pm_assertion_set_properties(__unused mach_port_t server, __unused int assertion_id, __unused vm_offset_t props, __unused mach_msg_type_number_t propsCnt, __unused int *disableAppSleep, __unused int *enableAppSleep, __unused int *enTrIntensity, __unused int *return_code)
{
    return 0;
}

kern_return_t io_pm_declare_network_client_active(__unused mach_port_t server, __unused vm_offset_t props, __unused mach_msg_type_number_t propsCnt, __unused int *assertion_id, __unused int *disableAppSleep, __unused int *return_code)
{
    return 0;
}

kern_return_t io_pm_declare_system_active(__unused mach_port_t server, __unused int *state, __unused vm_offset_t props, __unused mach_msg_type_number_t propsCnt, __unused int *assertion_id, __unused int *return_code)
{
    return 0;
}

kern_return_t io_pm_declare_user_active(__unused mach_port_t server, __unused int user_type, __unused vm_offset_t props, __unused mach_msg_type_number_t propsCnt, __unused int *assertion_id, __unused int *disableAppSleep, __unused int *return_code)
{
    return 0;
}

kern_return_t io_pm_set_exception_limits(__unused mach_port_t server, __unused vm_offset_t props, __unused mach_msg_type_number_t propsCnt, __unused int *return_code)
{
    return 0;
}

kern_return_t io_pm_set_value_int(__unused mach_port_t server, __unused int selector, __unused int value, __unused int *return_val)
{
    return 0;
}

void IOKitPMStubsTeardown(void)
{
    NotifyStubsTeardown();
    DispatchStubsTeardown();
    XPCStubsTeardown();
}
