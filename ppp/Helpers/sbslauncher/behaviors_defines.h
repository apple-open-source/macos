/*
 * Copyright (c) 2012 Apple Inc.
 * All rights reserved.
 */
#ifndef __BEHAVIORS_DEFINES_H__
#define __BEHAVIORS_DEFINES_H__

#define kVPNBehaviorsPlistFile				CFSTR("com.apple.pppcontroller-vpn-behaviors.plist")

#define kVPNBehaviorsNextAssetCheckTime		CFSTR("NextAssetCheckTime")
#define kVPNBehaviorsAssetCheckInterval		CFSTR("AssetCheckInterval")
#define kVPNBehaviorsAssetVersion			CFSTR("AssetVersion")
#define kVPNBehaviorsVODAlwaysMeansOnRetry	CFSTR("VODAlwaysMeansOnRetry")
#define kVPNBehaviorsAssetCheckStartDate	CFSTR("AssetCheckStartDate")
#define kVPNBehaviorsAssetRegions			CFSTR("AssetRegions")
#define kVPNBehaviorsReset					CFSTR("Reset")

#define kVPNBehaviorsTimeMultiplier				86400.0			/* Multiply by this to convert interval in the asset into seconds */
#define kVPNBehaviorsAssetCheckDefaultInterval	604800.0		/* 7 days */
#define kVPNBehaviorsAssetCheckErrorInterval	3600			/* 1 hour */
#define kVPNBehaviorsAssetCheckSmallDelay		120.0			/* 2 minutes */
#define kVPNBehaviorsAssetCheckMinInterval		1				/* 1 day */
#define kVPNBehaviorsAssetCheckMaxInterval		90				/* 90 days */

#endif /* __BEHAVIORS_DEFINES_H__ */
