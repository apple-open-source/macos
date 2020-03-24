// Copyright (C) 2018 Apple Inc. All Rights Reserved.

import CloudDeviceTest
import CoreDeviceAutomation
import CoreDeviceAutomationFrameworkFacade
import Foundation
import OctagonTestHarnessXPCServiceProtocol

extension CDAIOSDevice {
    func octagonFacade(block: @escaping (AnyObject) -> Void) throws {
        try self.withFrameworkFacade(bundlePath: "/AppleInternal/Library/Frameworks/OctagonTestHarness.framework",
                                     xpcService: "com.apple.trieste.OctagonTestHarnessXPCService",
                                     ofType: OctagonTestHarnessXPCServiceProtocol.self,
                                     block: block)
    }
}

final class OctagonTests: CDTTestCase {

    let username: String? = nil
    let password: String? = nil
    var signedIn: Bool = false

    let ckksTool = "/usr/sbin/ckksctl"
    let securityTool = "/usr/local/bin/security"

    private func getDevice() throws -> CDAIOSDevice {
        let common = CDAIOSCapabilities(
            .buildTrain("Yukon"),
            .location(country: "SE", building: "AP1"),
            .installedFrameworkFacade(true),
            .allowAnyCodeSignature(true)
        )

        if let rawPrkitUrl = ProcessInfo.processInfo.environment["PRKIT_URL"] {
            if let prkitUrl = URL(string: rawPrkitUrl) {
                let capabilities = common

                let device = try self.trieste.getIOSDevice(capabilities: capabilities)

                addTeardownBlock {
                    device.relinquish()
                }

                try device.installPRKit(at: prkitUrl)

                return device
            } else {
                throw CDTFail("PRKIT_URL was set, but can't be parsed as URL, aborting: '\(rawPrkitUrl)'")
            }
        } else {
            let device: CDAIOSDevice

            if !ProcessInfo.processInfo.environment.keys.contains("NO_XCODE") {
                let capabilities = common.merging(CDAIOSCapabilities(
                    .setup(.complete),
                    .collectSysdiagnose()
                ))

                device = try self.trieste.getIOSDevice(capabilities: capabilities)

                let targets = CDAInstalledXcodeTargetsSpec(
                    CDAInstalledXcodeTargetsSpec.Target(projectFile: "Security.xcodeproj", configuration: "Debug", scheme: "OctagonTestHarness", sdk: "iphoneos13.0.internal", roots:
                        CDAInstalledXcodeTargetsSpec.Root(product: "OctagonTestHarness.framework", installDirectory: "/AppleInternal/Library/Frameworks"),
                                                        CDAInstalledXcodeTargetsSpec.Root(product: "Security.framework", installDirectory: "/System/Library/Frameworks")
                    )
                )

                try device.installXcodeTargets(targets, rebootWhenDone: true)
            } else {
                let capabilities = common.merging(CDAIOSCapabilities(
                    .setup(.complete),
                    .collectSysdiagnose()
                ))

                device = try self.trieste.getIOSDevice(capabilities: capabilities)
            }

            addTeardownBlock {
                if self.signedIn {
                    do {
                        let listAccounts = try device.executeFile(atPath: "/usr/local/bin/accounts_tool", withArguments: ["--no-confirmation", "deleteAccountsForUsername", self.username!])
                        XCTAssertEqual(listAccounts.returnCode, 0, "deleteAccountsForUsername")
                        print("accounts_tool deleteAccountsForUsername\n\(String(data: listAccounts.standardOutput, encoding: .utf8)!)\n")
                    } catch {
                    }
                }

                device.relinquish()
            }

            if self.username != nil {
                CDALog(at: .infoLevel, "Signing in to iCloud here \(self.username!)")

                let listAccounts = try device.executeFile(atPath: "/usr/local/bin/accounts_tool", withArguments: ["listAccounts", "-v"])
                print("accounts_tool listAccounts -v\n\(String(data: listAccounts.standardOutput, encoding: .utf8)!)\n")

                let result = try device.executeFile(atPath: "/usr/local/bin/appleAccountSetupTool", withArguments: [self.username!, self.password!])
                XCTAssertEqual(result.returnCode, 0, "appleAccountSetupTool failed")
                print("appleAccountSetupTool\n\(String(data: result.standardOutput, encoding: .utf8)!)\n")

                let enableKVS = try device.executeFile(atPath: "/bin/sh", withArguments: ["-c", "accounts_tool enableDataclass  $(accounts_tool listAccountsForType com.apple.account.AppleAccount | grep identifier: | cut -f2 -d: ) com.apple.Dataclass.KeyValue"])
                XCTAssertEqual(enableKVS.returnCode, 0, "enableKVS failed")
                print("enableKVS\n\(String(data: enableKVS.standardOutput, encoding: .utf8)!)\n")

                self.signedIn = true
            }

            return device
        }
    }

    func compareCKKSZone(name zone: String, status1: NSDictionary, status2: NSDictionary) -> Bool {

        let zone1 = status1[zone] as! NSDictionary
        let zone2 = status2[zone] as! NSDictionary

        let keystate1 = zone1["keystate"] as! NSString
        let keystate2 = zone2["keystate"] as! NSString

        XCTAssertEqual(keystate1, "ready", "keystate should be ready for zone \(zone)")
        XCTAssertEqual(keystate2, "ready", "keystate should be ready for zone \(zone)")

        let ckaccountstatus1 = zone1["ckaccountstatus"] as! NSString
        let ckaccountstatus2 = zone2["ckaccountstatus"] as! NSString

        XCTAssertEqual(ckaccountstatus1, "logged in", "ckaccountstatus should be 'logged in' for zone \(zone)")
        XCTAssertEqual(ckaccountstatus2, "logged in", "ckaccountstatus should be 'logged in' for zone \(zone)")

        let currentTLK1 = zone1["currentTLK"] as? NSString
        let currentTLK2 = zone2["currentTLK"] as? NSString

        XCTAssertEqual(currentTLK1, currentTLK2, "TLK for zone \(zone) should be the same")

        return true
    }

    func compareCKKSStatus(c1: NSDictionary, c2: NSDictionary) -> Bool {

        let status1 = c1["status"] as! NSDictionary
        let status2 = c2["status"] as! NSDictionary

        XCTAssert(compareCKKSZone(name: "ApplePay", status1: status1, status2: status2))
        XCTAssert(compareCKKSZone(name: "Home", status1: status1, status2: status2))
        XCTAssert(compareCKKSZone(name: "Manatee", status1: status1, status2: status2))
        XCTAssert(compareCKKSZone(name: "AutoUnlock", status1: status1, status2: status2))

        return true
    }

    func sosStatus(_ device: CDAIOSDevice, verbose: Bool = false) throws {
        let result = try device.executeFile(atPath: securityTool, withArguments: ["sync", "-i"])
        if verbose {
            print("security sync -i\n\(String(data: result.standardOutput, encoding: .utf8)!)\n")
        }
    }
    func ckksStatus(_ device: CDAIOSDevice, verbose: Bool = false) throws -> NSDictionary {
        let ckks = try device.executeFile(atPath: self.ckksTool, withArguments: ["status", "--json"])
        if verbose {
            print("ckks status\n\(String(data: ckks.standardOutput, encoding: .utf8)!)\n")
        }

        return try JSONSerialization.jsonObject(with: ckks.standardOutput) as! NSDictionary
    }

    func sosApplication(_ device: CDAIOSDevice, verbose: Bool = false) throws {
        if self.password != nil {

            print("submitting application\n")

            let password = try device.executeFile(atPath: securityTool, withArguments: ["sync", "-P", self.password!])
            XCTAssertEqual(password.returnCode, 0, "setting password worked")

            let application = try device.executeFile(atPath: securityTool, withArguments: ["sync", "-e"])
            XCTAssertEqual(application.returnCode, 0, "submissing application worked password worked")

            try self.sosStatus(device, verbose: true)

            print("waiting after application done\n")
            sleep(4)
        }
    }

    func sosApprove(_ device: CDAIOSDevice, verbose: Bool = false) throws {
        if self.password != nil {

            print("approving applications\n")

            let password = try device.executeFile(atPath: securityTool, withArguments: ["sync", "-P", self.password!])
            XCTAssertEqual(password.returnCode, 0, "setting password worked")

            let approve = try device.executeFile(atPath: securityTool, withArguments: ["sync", "-a"])
            XCTAssertEqual(approve.returnCode, 0, "submissing application worked password worked")

            try self.sosStatus(device, verbose: true)

            print("waiting after approving applications\n")
            sleep(4)
        }
    }

    func forceResetSOS(_ device: CDAIOSDevice, resetCKKS: Bool = false) throws {
        if self.password != nil {
            _ = try device.executeFile(atPath: securityTool, withArguments: ["sync", "-P", self.password!])
            _ = try device.executeFile(atPath: securityTool, withArguments: ["sync", "-R"])
            _ = try device.executeFile(atPath: securityTool, withArguments: ["sync", "-C"])
            _ = try device.executeFile(atPath: securityTool, withArguments: ["sync", "-P", self.password!])
            _ = try device.executeFile(atPath: securityTool, withArguments: ["sync", "-R"])
            _ = try device.executeFile(atPath: securityTool, withArguments: ["sync", "-O"])

            try self.sosStatus(device, verbose: true)

            print("sleeping some to allow cdpd, cloudd and friends to catch up \n")
            sleep(4)

            if resetCKKS {
                _ = try device.executeFile(atPath: self.ckksTool, withArguments: ["reset-cloudkit"])

                print("sleeps some after ckksctl reset (should be removed)\n")
                sleep(4)
            }
        }
    }

    func testBasiciCloudSignInSignOut() throws {
        let device = try getDevice()

        let result = try device.executeFile(atPath: "/usr/local/bin/accounts_tool", withArguments: ["listAccounts", "-v"])
        print("accounts_tool listAccounts -v\n\(String(data: result.standardOutput, encoding: .utf8)!)\n")

        try forceResetSOS(device, resetCKKS: true)

        _ = try self.ckksStatus(device)
    }

    func test2DeviceSOS() throws {
        if self.password == nil {
            print("this test only works with password")
            return
        }

        let device1 = try getDevice()
        let device2 = try getDevice()

        try forceResetSOS(device1, resetCKKS: true)
        try sosApplication(device2)
        try sosApprove(device1, verbose: true)
        try sosStatus(device2, verbose: true)

        print("waiting some \(Date())")
        Thread.sleep(until: Date().addingTimeInterval(30.0)) // CK really really slow to get us data
        print("waiting done \(Date())")

        let ckks1 = try ckksStatus(device1)
        let ckks2 = try ckksStatus(device2)

        print("comparing status \(Date())")
        XCTAssert(self.compareCKKSStatus(c1: ckks1, c2: ckks2), "compare of CKKS status")

        print("Done \(Date())")
    }

    func testOctagonReset() throws {
        let device = try getDevice()

        try forceResetSOS(device, resetCKKS: true)

        CDALog(at: .infoLevel, "Obtained signed in device \(device)")

        try device.octagonFacade { octagon in
            CDALog(at: .infoLevel, "Obtained framework facade \(octagon)")

            for i in 0..<2 {
                CDALog(at: .infoLevel, "Reset \(i)")
                octagon.octagonReset("altDSID") { _, error in
                    CDTAssert(error == nil, "Octagon wasn't reset, error was \(String(describing: error))")
                }
            }
        }

        CDALog(at: .infoLevel, "Done running")
    }
}
