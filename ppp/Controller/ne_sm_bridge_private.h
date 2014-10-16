/*
 * Copyright (c) 2014 Apple Inc.
 * All rights reserved.
 */

#ifndef __NE_SM_BRIDGE_PRIVATE_H__
#define __NE_SM_BRIDGE_PRIVATE_H__

#include <ne_sm_bridge.h>
#include <ne_session.h>

#define NESessionManagerPrivilegedEntitlement      "com.apple.private.nesessionmanager.privileged"

bool ne_sm_bridge_logv(int level, CFStringRef format, va_list args);
bool ne_sm_bridge_is_logging_at_level(int level);
CFDictionaryRef ne_sm_bridge_copy_configuration(ne_sm_bridge_t bridge);
void ne_sm_bridge_status_changed(ne_sm_bridge_t bridge);
void ne_sm_bridge_acknowledge_sleep(ne_sm_bridge_t bridge);
void ne_sm_bridge_filter_state_dictionaries(ne_sm_bridge_t bridge, CFMutableArrayRef names, CFMutableArrayRef dictionaries);
CFStringRef ne_sm_bridge_copy_password_from_keychain(ne_sm_bridge_t bridge, CFStringRef type);
void ne_sm_bridge_allow_dispose(ne_sm_bridge_t bridge);
uint64_t ne_sm_bridge_get_connect_time(ne_sm_bridge_t bridge);
bool ne_sm_bridge_request_install(ne_sm_bridge_t bridge, bool exclusive);
bool ne_sm_bridge_request_uninstall(ne_sm_bridge_t bridge);
bool ne_sm_bridge_start_profile_janitor(ne_sm_bridge_t bridge, CFStringRef profileIdentifier);
void ne_sm_bridge_clear_saved_password(ne_sm_bridge_t bridge, CFStringRef type);

#endif /* __NE_SM_BRIDGE_PRIVATE_H__ */
