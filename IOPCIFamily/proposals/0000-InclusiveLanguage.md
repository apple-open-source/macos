# IOPCIFamily Inclusive Language changes

* Proposal: PCI-0000
* Author(s): Gage Eads <geads@apple.com>
* Intended Release: StarE

##### Related radars

* rdar://73472405 (Kernel PCIDriverKit - .kIOPCIStatusMasterAbortActive - Converting API to Inclusive Language)
* rdar://73472366 (Kernel PCIDriverKit - .kIOPCICommandBusMaster - Converting API to Inclusive Language)
* rdar://73478328 (Kernel - IOPCIDevice.setBusMasterEnable - Converting API to Inclusive Language)

## Introduction

Deprecate existing API and introduce replacement symbols with new names to comply with the Inclusive Language TLF.

## Proposed solution

Mark the following symbols as deprecated in macOS 12.4, iOS 15.4, watchOS 8.5, tvOS 15.4, and bridgeOS 6.4, and introduce replacements that forward to the same underlying implementation:

- `IOPCIDevice::setBusMasterEnable` becomes `IOPCIDevice::setBusLeadEnable`
- `kIOPCICommandBusMaster` becomes `kIOPCICommandBusLead`
- `kIOPCIStatusMasterAbortActive` becomes `kIOPCIStatusLeadAbortActive`

## Detailed design

```
-/*! @function setBusMasterEnable
-    @abstract Sets the device's bus master enable.
-    @discussion This method sets the bus master enable bit in the device's command config space register to the passed value, and returns the previous state of the enable.
-    @param enable True or false to enable or disable bus mastering.
-    @result True if bus mastering was previously enabled, false otherwise. */
+    bool setBusMasterEnable( bool enable ) API_DEPRECATED_WITH_REPLACEMENT("setBusLeadEnable", macos(10.0, 12.4), ios(1.0, 15.4), watchos(1.0, 8.5), tvos(1.0, 15.4), bridgeos(1.0, 6.4));

-    virtual bool setBusMasterEnable( bool enable );
+/*! @function setBusLeadEnable
+    @abstract Sets the device's bus lead enable.
+    @discussion This method sets the bus lead enable bit in the device's command config space register to the passed value, and returns the previous state of the enable.
+    @param enable True or false to enable or disable bus leading capability.
+    @result True if bus leading was previously enabled, false otherwise. */
+
+    virtual bool setBusLeadEnable( bool enable );
```

```
-    kIOPCICommandBusMaster        = 0x0004,
+    kIOPCICommandBusLead          = 0x0004,
+    kIOPCICommandBusMaster        API_DEPRECATED_WITH_REPLACEMENT("kIOPCICommandBusLead", macos(10.0, 12.4), ios(1.0, 15.4), watchos(1.0, 8.5), tvos(1.0, 15.4), bridgeos(1.0, 6.4)) = kIOPCICommandBusLead,

-    kIOPCIStatusMasterAbortActive  = 0x2000,
+    kIOPCIStatusLeadAbortActive    = 0x2000,
+    kIOPCIStatusMasterAbortActive  API_DEPRECATED_WITH_REPLACEMENT("kIOPCIStatusLeadAbortActive", macos(10.0, 12.4), ios(1.0, 15.4), watchos(1.0, 8.5), tvos(1.0, 15.4), bridgeos(1.0, 6.4)) = kIOPCIStatusLeadAbortActive,
```

## Impact on existing code

Developers building for the aforementioned OS versions will receive compiler warnings.

## Alternatives considered

None considered. The choice of Master/Slave -> Lead/Follower is based on PCI-SIG guidance.
