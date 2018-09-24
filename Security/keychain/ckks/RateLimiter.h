/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface RateLimiter : NSObject <NSSecureCoding>

@property (readonly, nonatomic) NSDictionary* config;
@property (readonly, nonatomic) NSUInteger stateSize;
@property (readonly, nonatomic, nullable) NSString* assetType;

typedef NS_ENUM(NSInteger, RateLimiterBadness) {
    RateLimiterBadnessClear = 0,  // everything is fine, process right now
    RateLimiterBadnessCongested,
    RateLimiterBadnessSeverelyCongested,
    RateLimiterBadnessGridlocked,
    RateLimiterBadnessOverloaded,  // everything is on fire, go away
};

- (instancetype _Nullable)initWithConfig:(NSDictionary*)config;
- (instancetype _Nullable)initWithPlistFromURL:(NSURL*)url;
- (instancetype _Nullable)initWithAssetType:(NSString*)type;  // Not implemented yet
- (instancetype _Nullable)initWithCoder:(NSCoder*)coder;
- (instancetype _Nullable)init NS_UNAVAILABLE;

/*!
 * @brief Find out whether objects may be processed or must wait.
 * @param obj The object being judged.
 * @param time Current time.
 * @param limitTime Assigned okay-to-process time. Nil when object may be processed immediately.
 * @return RateLimiterBadness enum value indicating current congestion situation, or to signal
 *
 * judge:at: will set the limitTime object to nil in case of 0 badness. For badnesses 1-4 the time object will indicate when it is okay to send the entry.
 * At badness 5 judge:at: has determined there is too much activity so the caller should hold off altogether. The limitTime object will indicate when
 * this overloaded state will end.
 */
- (NSInteger)judge:(id)obj at:(NSDate*)time limitTime:(NSDate* _Nonnull __autoreleasing* _Nonnull)limitTime;

- (void)reset;
- (NSString*)diagnostics;
+ (BOOL)supportsSecureCoding;

// TODO:
// implement config loading from MobileAsset

@end

NS_ASSUME_NONNULL_END

/* Annotated example plist

<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>general</key>
    <dict>
        <!-- Total item limit -->
        <key>maxStateSize</key>
        <integer>250</integer>
        <!-- Throw away items after this many seconds -->
        <key>maxItemAge</key>
        <integer>3600</integer>
        <!-- Ignore everybody for this many seconds -->
        <key>overloadDuration</key>
        <integer>1800</integer>
        <!-- Printable string for logs -->
        <key>name</key>
        <string>CKKS</string>
        <!-- Load config stored in this MobileAsset (ignored if inited with config or plist directly) -->
        <key>MAType</key>
        <string></string>
    </dict>
    <!-- Each property you want to ratelimit on must have its own group dictionary -->
    <key>groups</key>
    <array>
        <dict>
            <!-- The first group must be for the global bucket. It behaves identically otherwise -->
            <key>property</key>
            <string>global</string>
            <key>capacity</key>
            <integer>20</integer>
            <key>rate</key>
            <integer>30</integer>
            <key>badness</key>
            <integer>1</integer>
        </dict>
        <dict>
            <!-- Your object must respond to this selector that takes no arguments by returning an NSString * -->
            <key>property</key>
            <string>UUID</string>
            <!-- Buckets of this type hold at most this many tokens -->
            <key>capacity</key>
            <integer>3</integer>
            <!-- Tokens replenish at 1 every this many seconds -->
            <key>rate</key>
            <integer>600</integer>
            <!-- Max of all empty bucket badnesses is returned to caller. See RateLimiterBadness enum -->
            <key>badness</key>
            <integer>3</integer>
        </dict>
    </array>
</dict>
</plist>

*/
