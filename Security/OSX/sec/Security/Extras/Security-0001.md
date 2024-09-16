# Making updates in Security Access Control API

* Proposal: Security-0001
* Author(s): Jan Schwarz [j_schwarz@apple.com](mailto:j_schwarz@apple.com)
* Intended Release: Crystal/Glow
* Framework: Security

##### Related radars

* rdar://121039831 ([Oneness] Modification of existing access controls to support Oneness)

##### Revision history

* **v1** Initial version

## Introduction

With respect to planned authentication features, we want to deprecate the Security access control flag that uses the word "Watch" in its name and replace it with more general version using the word "Companion" instead
    
## Motivation

* Oneness provides an option to authenticate a user with another device (authenticate a user on iPhone by interaction with Mac). It is very similar to the current use case when a user can authenticate themself on Mac using Apple Watch
* The authentication continuity is very handy and we can expect more effort in that direction in the future 
* We want to update Security API to be more universal in order to cover the future use cases of authentication with other paired devices
* To simplify future adoption, the updated API should make it possible to authenticate with any paired device nearby without the necessity of allow-listing all options

## Proposed solution

* We propose to deprecate `kSecAccessControlWatch` access control flag and replace it with `kSecAccessControlCompanion`
* The new symbol will use the same constant under the hood so there will be no difference from the functionality point of view between using the deprecated and the new symbol

## Examples

### Swift

```swift
import Security

var companionAccessControl: SecAccessControl? {
    let flags: SecAccessControlCreateFlags = [.biometryAny, .companion]
    
    return SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleWhenUnlockedThisDeviceOnly, flags, nil)
}
```

### Objective-C

```objc
#import <Security/Security.h>

- (SecAccessControlRef)companionAccessControl {
    SecAccessControlCreateFlags flags = kSecAccessControlBiometryAny | kSecAccessControlCompanion;
    
    return SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleWhenUnlockedThisDeviceOnly, flags, nil);
}

```

## Detailed design

### Objective-C definition

```objc
/*!
 @typedef SecAccessControlCreateFlags
 
 //...//

 @constant kSecAccessControlWatch
 Deprecated, please use kSecAccessControlCompanion instead.

 @constant kSecAccessControlCompanion
 Constraint: Paired companion device
 
 //...//

*/
typedef CF_OPTIONS(CFOptionFlags, SecAccessControlCreateFlags) {

    //...//

    kSecAccessControlWatch                  API_DEPRECATED_WITH_REPLACEMENT("kSecAccessControlCompanion", macos(10.15, 15.0), ios(NA, NA), macCatalyst(13.0, 18.0)) = 1u << 5,
    kSecAccessControlCompanion              API_AVAILABLE(macos(15.0), ios(18.0), macCatalyst(18.0)) = 1u << 5,

    //...//

} __OSX_AVAILABLE_STARTING(__MAC_10_10, __IPHONE_8_0);
```

### Generated Swift interface

```swift
/*!
 @typedef SecAccessControlCreateFlags
 
 //...//

 @constant kSecAccessControlWatch
 Deprecated, please use kSecAccessControlCompanion instead.

 @constant kSecAccessControlCompanion
 Constraint: Paired companion device
 
 //...//

*/
@available(macOS 10.10, macCatalyst 13.0, iOS 8.0, *)
@available(tvOS, unavailable)
@available(watchOS, unavailable)
public struct SecAccessControlCreateFlags : OptionSet, @unchecked Sendable {

    //...//
    
    @available(macOS, introduced: 10.15, deprecated: 15.0, renamed: "SecAccessControlCreateFlags.companion")
    @available(macCatalyst, introduced: 13.0, deprecated: 18.0, renamed: "SecAccessControlCreateFlags.companion")
    @available(iOS, unavailable)
    @available(tvOS, unavailable)
    @available(watchOS, unavailable)
    public static var watch: SecAccessControlCreateFlags { get }
    
    @available(macOS 15.0, macCatalyst 18.0, iOS 18.0, *)
    @available(tvOS, unavailable)
    @available(watchOS, unavailable)
    public static var companion: SecAccessControlCreateFlags { get }

    //...//
    
}
```

## Impact on platform quality

* As the proposed symbol use the word "companion" that covers any paired device, if there are new ways of authentication on a paired device in the future those will be enabled automatically for adopters without the necessity to update their existing code. That is very desirable as it follows the sentiment of being able to authenticate on any supported paired device without the necessity to allow-list them explicitly.


## Impact on existing code

* Starting in Glow, adopters who currently use `kSecAccessControlWatch` will get the deprecation warning but functionality of their code will remain the same.

## Alternatives considered

* We considered not deprecating the existing symbol and just introducing the new access control flag case that would be specific for the new paired device authentication option e.g. `kSecAccessControlPhoneWithMac`. However, this contradicts the sentiment of being able to authenticate on any paired device available and adopters would have to allow-list all options explicitly. Also, it would require an API update each time a new paired device authentication feature would be implemented.