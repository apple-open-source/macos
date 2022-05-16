/*
 *  PMDynamicStore.h
 *  PowerManagement
 *
 *  Created by Ethan Bold on 1/13/11.
 *  Copyright 2011 __MyCompanyName__. All rights reserved.
 *
 */

__private_extern__ void PMStoreLoad(void);

__private_extern__ bool PMStoreSetValue(CFStringRef key, CFTypeRef value);

__private_extern__ bool PMStoreRemoveValue(CFStringRef key);

