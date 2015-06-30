//
//  VoucherContentSysctl.cpp
//  system_cmds
//
//  Created by James McIlree on 4/29/14.
//
//

#include "global.h"

VoucherContentSysctl::VoucherContentSysctl(bool is_enabled) :
	_original_value(0),
	_new_value(is_enabled ? 1 : 0)
{
	size_t original_value_size = sizeof(_original_value);
	if (sysctlbyname("kern.ipc_voucher_trace_contents", &_original_value, &original_value_size, &_new_value, sizeof(_new_value))) {
		log_msg(ASL_LEVEL_ERR, "Unable to %s kern.ipc_voucher_trace_contents sysctl", is_enabled ? "set" : "clear");
	}
}

VoucherContentSysctl::~VoucherContentSysctl() {
	if (_original_value != _new_value) {
		if (sysctlbyname("kern.ipc_voucher_trace_contents", NULL, 0, &_original_value, sizeof(_original_value))) {
			log_msg(ASL_LEVEL_ERR, "Unable to restore original value of kern.ipc_voucher_trace_contents sysctl");
		}
	}
}
