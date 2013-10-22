/*
 * Copyright (c) 2012 Apple Inc.
 * All rights reserved.
 */

#ifndef __VPN_BEHAVIORS_H__
#define __VPN_BEHAVIORS_H__

typedef void (^BehaviorsUpdatedBlock)(void);

void behaviors_modify_ondemand(CFMutableDictionaryRef trigger_dict, BehaviorsUpdatedBlock cb_block);
void behaviors_cancel_asset_check(void);

#endif /* __VPN_BEHAVIORS_H__ */
