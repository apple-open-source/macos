import Foundation
import os

let tplogDebug = OSLog(subsystem: "com.apple.security.trustedpeers", category: "debug")

// This should definitely use the ArgumentParser library from the Utility package.
// However, right now that's not accessible from this code due to build system issues.
// Do it the hard way.

let programName = CommandLine.arguments[0]
var args: ArraySlice<String> = CommandLine.arguments[1...]

var context: String = OTDefaultContext
var container: String = "com.apple.security.keychain"

// Only used for prepare, but in the absence of a real command line library this is the easiest way to get them
var modelID: String?
var machineID: String?
var epoch: UInt64 = 1
var bottleSalt: String = ""

// Only used for join, establish and update
var preapprovedKeys: [Data]?
var deviceName: String?
var serialNumber: String?
var osVersion: String?
var policyVersion: NSNumber?
var policySecrets: [String: Data]?

enum Command {
    case dump
    case depart
    case distrust(Set<String>)
    case join(Data, Data)
    case establish
    case localReset
    case prepare
    case healthInquiry
    case update
    case reset
    case validate
    case viableBottles
    case vouch(String, Data, Data, Data, Data)
    case vouchWithBottle(String, Data, String)
    case allow(Set<String>, Bool)
    case supportApp
}

func printUsage() {
    print("Usage:", (CommandLine.arguments[0] as NSString).lastPathComponent,
          "[--container CONTAINER] [--context CONTEXT] COMMAND")
    print()
    print("Commands:")
    print("  allow [--idms] MACHINEID ...")
    print("                            Set the (space-separated) list of machine IDs allowed in the account. If --idms is provided, append the IDMS trusted device list")
    print("  dump                      Print the state of the world as this peer knows it")
    print("  depart                    Attempt to depart the account and mark yourself as untrusted")
    print("  distrust PEERID ...       Distrust one or more peers by peer ID")
    print("  establish                 Calls Cuttlefish Establish, creating a new account-wide trust arena with a single peer (previously generated with prepare")
    print("  healthInquiry             Request peers to check in with reportHealth")
    print("  join VOUCHER VOUCHERSIG   Join a circle using this (base64) voucher and voucherSig")
    print("  local-reset               Resets the local cuttlefish database, and ignores all previous information. Does not change anything off-device")
    print("  prepare [--modelid MODELID] [--machineid MACHINEID] [--epoch EPOCH] [--bottlesalt BOTTLESALT]")
    print("                            Creates a new identity and returns its attributes. If not provided, modelid and machineid will be given some defaults (ignoring the local device)")
    print("  supportApp                Get SupportApp information from Cuttlefish")
    print("  update                    Fetch new information from Cuttlefish, and perform any actions this node deems necessary")
    print("  validate                  Vvalidate SOS and Octagon data structures from server side")
    print("  viable-bottles            Show bottles in preference order of server")
    print("  vouch PEERID PERMANENTINFO PERMANENTINFOSIG STABLEINFO STABLEINFOSIG")
    print("                            Create a voucher for a new peer. permanentInfo, permanentInfoSig, stableInfo, stableInfoSig should be base64 data")
    print("  vouchWithBottle BOTTLEID ENTROPY SALT")
    print("                            Create a voucher for the ego peer using the given bottle. entropy should be base64 data.")
    print("  reset                     Resets Cuttlefish for this account")
    print()
    print("Options applying to `join', `establish' and `update'")
    print("  --preapprove KEY...       Sets the (space-separated base64) list of public keys that are preapproved.")
    print("  --device-name NAME        Sets the device name string.")
    print("  --os-version VERSION      Sets the OS version string.")
    print("  --policy-version VERSION  Sets the policy version.")
    print("  --policy-secret NAME DATA Adds a name-value pair to policy secrets. DATA must be base-64.")
    print("Options applying to `vouch' and `join'")
    print("  --config FILE             Configuration file with json data.")
    print()
}

func exitUsage(_ code: Int32) -> Never {
    printUsage()
    exit(code)
}

func extractJSONData(dictionary: [String: Any], key: String) -> Data? {
    guard let b64string = dictionary[key] as? String else {
        return nil
    }
    guard let data = Data(base64Encoded: b64string) else {
        return nil
    }
    return data
}

func jsonFromFile(filename: String) -> [String: Any] {
    let data: Data
    let json: [String: Any]?
    do {
        data = try Data(contentsOf: URL(fileURLWithPath: filename), options: .mappedIfSafe)
        json = try JSONSerialization.jsonObject(with: data) as? [String: Any]
    } catch {
        print("Error: failed to parse json file \(filename): \(error)")
        exit(EXIT_FAILURE)
    }
    guard let dictionary = json else {
        print("Error: failed to get dictionary in file \(filename)")
        exit(EXIT_FAILURE)
    }
    return dictionary
}

var commands: [Command] = []
var argIterator = args.makeIterator()
var configurationData: [String: Any]?

while let arg = argIterator.next() {
    switch arg {
    case "--container":
        let newContainer = argIterator.next()
        guard newContainer != nil  else {
            print("Error: --container takes a value")
            print()
            exitUsage(1)
        }
        container = newContainer!

    case "--context":
        let newContext = argIterator.next()
        guard newContext != nil  else {
            print("Error: --context takes a value")
            print()
            exitUsage(1)
        }
        context = newContext!

    case "--modelid":
        let newModelID = argIterator.next()
        guard newModelID != nil else {
            print("Error: --modelid takes a value")
            print()
            exitUsage(1)
        }
        modelID = newModelID!

    case "--machineid":
        let newMachineID = argIterator.next()
        guard newMachineID != nil else {
            print("Error: --machineid takes a value")
            print()
            exitUsage(1)
        }
        machineID = newMachineID!

    case "--epoch":
        let newEpoch = argIterator.next()
        guard newEpoch != nil else {
            print("Error: --epoch takes a value")
            print()
            exitUsage(1)
        }
        epoch = UInt64(newEpoch!)!

    case "--bottlesalt":
        let salt = argIterator.next()
        guard salt != nil else {
            print("Error: --bottlesalt takes a value")
            print()
            exitUsage(1)
        }
        bottleSalt = salt!

    case "--preapprove":
        var newPreapprovedKeys: [Data] = []
        while let arg = argIterator.next() {
            let data = Data(base64Encoded: arg)
            guard let key = data else {
                print("Error: preapproved keys must be base-64 data")
                exitUsage(1)
            }
            newPreapprovedKeys.append(key)
        }
        preapprovedKeys = newPreapprovedKeys

    case "--device-name":
        guard let newDeviceName = argIterator.next() else {
            print("Error: --device-name takes a string argument")
            exitUsage(1)
        }
        deviceName = newDeviceName

    case "--serial-number":
        guard let newSerialNumber = argIterator.next() else {
            print("Error: --serial-number takes a string argument")
            exitUsage(1)
        }
        serialNumber = newSerialNumber

    case "--os-version":
        guard let newOsVersion = argIterator.next() else {
            print("Error: --os-version takes a string argument")
            exitUsage(1)
        }
        osVersion = newOsVersion

    case "--policy-version":
        guard let newPolicyVersion = UInt64(argIterator.next() ?? "") else {
            print("Error: --policy-version takes an integer argument")
            exitUsage(1)
        }
        policyVersion = NSNumber(value: newPolicyVersion)

    case "--policy-secret":
        guard let name = argIterator.next(), let dataBase64 = argIterator.next() else {
            print("Error: --policy-secret takes a name and data")
            exitUsage(1)
        }
        guard let data = Data(base64Encoded: dataBase64) else {
            print("Error: --policy-secret data must be base-64")
            exitUsage(1)
        }
        if nil == policySecrets {
            policySecrets = [:]
        }
        policySecrets![name] = data

    case "--config":
        guard let filename = argIterator.next() else {
            print("Error: --config file argument missing")
            exitUsage(1)
        }

        configurationData = jsonFromFile(filename: filename)

    case "--help":
        exitUsage(0)

    case "dump":
        commands.append(.dump)

    case "depart":
        commands.append(.depart)

    case "distrust":
        var peerIDs = Set<String>()
        while let arg = argIterator.next() {
            peerIDs.insert(arg)
        }
        commands.append(.distrust(peerIDs))

    case "establish":
        commands.append(.establish)

    case "join":
        let voucher: Data
        let voucherSig: Data

        if let configuration = configurationData {
            guard let voucherData = extractJSONData(dictionary: configuration, key: "voucher") else {
                print("Error: join needs a voucher")
                exitUsage(EXIT_FAILURE)
            }
            guard let voucherSigData = extractJSONData(dictionary: configuration, key: "voucherSig") else {
                print("Error: join needs a voucherSig")
                exitUsage(EXIT_FAILURE)
            }
            voucher = voucherData
            voucherSig = voucherSigData

        } else {
            guard let voucherBase64 = argIterator.next() else {
                print("Error: join needs a voucher")
                print()
                exitUsage(1)
            }

            guard let voucherData = Data(base64Encoded: voucherBase64) else {
                print("Error: voucher must be base-64 data")
                print()
                exitUsage(1)
            }

            guard let voucherSigBase64 = argIterator.next() else {
                print("Error: join needs a voucherSig")
                print()
                exitUsage(1)
            }

            guard let voucherSigData = Data(base64Encoded: voucherSigBase64) else {
                print("Error: voucherSig must be base-64 data")
                print()
                exitUsage(1)
            }

            voucher = voucherData
            voucherSig = voucherSigData
        }
        commands.append(.join(voucher, voucherSig))

    case "local-reset":
        commands.append(.localReset)

    case "prepare":
        commands.append(.prepare)

    case "healthInquiry":
        commands.append(.healthInquiry)

    case "reset":
        commands.append(.reset)

    case "update":
        commands.append(.update)

    case "supportApp":
        commands.append(.supportApp)

    case "validate":
        commands.append(.validate)

    case "viable-bottles":
        commands.append(.viableBottles)

    case "vouch":
        let peerID: String
        let permanentInfo: Data
        let permanentInfoSig: Data
        let stableInfo: Data
        let stableInfoSig: Data

        if let configuration = configurationData {
            guard let peerIDString = configuration["peerID"] as? String else {
                print("Error: vouch needs a peerID")
                exitUsage(EXIT_FAILURE)
            }

            guard let permanentInfoData = extractJSONData(dictionary: configuration, key: "permanentInfo") else {
                print("Error: vouch needs a permanentInfo")
                exitUsage(EXIT_FAILURE)
            }
            guard let permanentInfoSigData = extractJSONData(dictionary: configuration, key: "permanentInfoSig") else {
                print("Error: vouch needs a permanentInfoSig")
                exitUsage(EXIT_FAILURE)
            }
            guard let stableInfoData = extractJSONData(dictionary: configuration, key: "stableInfo") else {
                print("Error: vouch needs a stableInfo")
                exitUsage(EXIT_FAILURE)
            }
            guard let stableInfoSigData = extractJSONData(dictionary: configuration, key: "stableInfoSig") else {
                print("Error: vouch needs a stableInfoSig")
                exitUsage(EXIT_FAILURE)
            }

            peerID = peerIDString
            permanentInfo = permanentInfoData
            permanentInfoSig = permanentInfoSigData
            stableInfo = stableInfoData
            stableInfoSig = stableInfoSigData

        } else {

            guard let peerIDString = argIterator.next() else {
                print("Error: vouch needs a peerID")
                print()
                exitUsage(1)
            }
            guard let permanentInfoBase64 = argIterator.next() else {
                print("Error: vouch needs a permanentInfo")
                print()
                exitUsage(1)
            }
            guard let permanentInfoSigBase64 = argIterator.next() else {
                print("Error: vouch needs a permanentInfoSig")
                print()
                exitUsage(1)
            }
            guard let stableInfoBase64 = argIterator.next() else {
                print("Error: vouch needs a stableInfo")
                print()
                exitUsage(1)
            }
            guard let stableInfoSigBase64 = argIterator.next() else {
                print("Error: vouch needs a stableInfoSig")
                print()
                exitUsage(1)
            }

            guard let permanentInfoData = Data(base64Encoded: permanentInfoBase64) else {
                print("Error: permanentInfo must be base-64 data")
                print()
                exitUsage(1)
            }

            guard let permanentInfoSigData = Data(base64Encoded: permanentInfoSigBase64) else {
                print("Error: permanentInfoSig must be base-64 data")
                print()
                exitUsage(1)
            }
            guard let stableInfoData = Data(base64Encoded: stableInfoBase64) else {
                print("Error: stableInfo must be base-64 data")
                print()
                exitUsage(1)
            }

            guard let stableInfoSigData = Data(base64Encoded: stableInfoSigBase64) else {
                print("Error: stableInfoSig must be base-64 data")
                print()
                exitUsage(1)
            }

            peerID = peerIDString
            permanentInfo = permanentInfoData
            permanentInfoSig = permanentInfoSigData
            stableInfo = stableInfoData
            stableInfoSig = stableInfoSigData
        }

        commands.append(.vouch(peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig))

    case "vouchWithBottle":
        guard let bottleID = argIterator.next() else {
            print("Error: vouchWithBottle needs a bottleID")
            print()
            exitUsage(1)
        }
        guard let entropyBase64 = argIterator.next() else {
            print("Error: vouchWithBottle needs entropy")
            print()
            exitUsage(1)
        }
        guard let salt = argIterator.next() else {
            print("Error: vouchWithBottle needs a salt")
            print()
            exitUsage(1)
        }

        guard let entropy = Data(base64Encoded: entropyBase64) else {
            print("Error: entropy must be base-64 data")
            print()
            exitUsage(1)
        }

        commands.append(.vouchWithBottle(bottleID, entropy, salt))

    case "allow":
        var machineIDs = Set<String>()
        var performIDMS = false
        while let arg = argIterator.next() {
            if(arg == "--idms") {
                performIDMS = true
            } else {
                machineIDs.insert(arg)
            }
        }
        commands.append(.allow(machineIDs, performIDMS))

    default:
        print("Unknown argument:", arg)
        exitUsage(1)
    }
}

if commands.count == 0 {
    exitUsage(0)
}

// JSONSerialization has no idea how to handle NSData. Help it out.
func cleanDictionaryForJSON(_ d: [AnyHashable: Any]) -> [AnyHashable: Any] {
    func cleanValue(_ value: Any) -> Any {
        switch value {
        case let subDict as [AnyHashable: Any]:
            return cleanDictionaryForJSON(subDict)
        case let subArray as [Any]:
            return subArray.map(cleanValue)
        case let data as Data:
            return data.base64EncodedString()
        default:
            return value
        }
    }

    return d.mapValues(cleanValue)
}

// Bring up a connection to TrustedPeersHelper
let connection = NSXPCConnection(serviceName: "com.apple.TrustedPeersHelper")
connection.remoteObjectInterface = TrustedPeersHelperSetupProtocol(NSXPCInterface(with: TrustedPeersHelperProtocol.self))
connection.resume()
let tpHelper = connection.synchronousRemoteObjectProxyWithErrorHandler { error in print("Unable to connect to TPHelper:", error) } as! TrustedPeersHelperProtocol

for command in commands {
    switch command {
    case .dump:
        os_log("dumping (%@, %@)", log: tplogDebug, type: .default, container, context)
        tpHelper.dump(withContainer: container, context: context) { reply, error in
            guard error == nil else {
                print("Error dumping:", error!)
                return
            }

            if let reply = reply {
                do {
                    print(try TPCTLObjectiveC.jsonSerialize(cleanDictionaryForJSON(reply)))
                } catch {
                    print("Error encoding JSON: \(error)")
                }
            } else {
                print("Error: no results, but no error either?")
            }
        }

    case .depart:
        os_log("departing (%@, %@)", log: tplogDebug, type: .default, container, context)
        tpHelper.departByDistrustingSelf(withContainer: container, context: context) { error in
            guard error == nil else {
                print("Error departing:", error!)
                return
            }

            print("Depart successful")
        }

    case .distrust(let peerIDs):
        os_log("distrusting %@ for (%@, %@)", log: tplogDebug, type: .default, peerIDs, container, context)
        tpHelper.distrustPeerIDs(withContainer: container, context: context, peerIDs: peerIDs) { error in
            guard error == nil else {
                print("Error distrusting:", error!)
                return
            }
            print("Distrust successful")
        }

    case .join(let voucher, let voucherSig):
        os_log("joining (%@, %@)", log: tplogDebug, type: .default, container, context)
        tpHelper.join(withContainer: container,
                      context: context,
                      voucherData: voucher,
                      voucherSig: voucherSig,
                      ckksKeys: [],
                      tlkShares: [],
                      preapprovedKeys: preapprovedKeys ?? []) { peerID, _, error in
                        guard error == nil else {
                            print("Error joining:", error!)
                            return
                        }
                        print("Join successful. PeerID:", peerID!)
        }

    case .establish:
        os_log("establishing (%@, %@)", log: tplogDebug, type: .default, container, context)
        tpHelper.establish(withContainer: container,
                           context: context,
                           ckksKeys: [],
                           tlkShares: [],
                           preapprovedKeys: preapprovedKeys ?? []) { peerID, _, error in
                            guard error == nil else {
                                print("Error establishing:", error!)
                                return
                            }
                            print("Establish successful. Peer ID:", peerID!)
        }

    case .healthInquiry:
        os_log("healthInquiry (%@, %@)", log: tplogDebug, type: .default, container, context)
        tpHelper.pushHealthInquiry(withContainer: container, context: context) { error in
            guard error == nil else {
                print("Error healthInquiry: \(String(describing: error))")
                return
            }
            print("healthInquiry successful")
        }

    case .localReset:
        os_log("local-reset (%@, %@)", log: tplogDebug, type: .default, container, context)
        tpHelper.localReset(withContainer: container, context: context) { error in
            guard error == nil else {
                print("Error resetting:", error!)
                return
            }

            os_log("local-reset (%@, %@): successful", log: tplogDebug, type: .default, container, context)
            print("Local reset successful")
        }

    case .supportApp:
        os_log("supportApp (%@, %@)", log: tplogDebug, type: .default, container, context)

        tpHelper.getSupportAppInfo(withContainer: container, context: context) { data, error in
            guard error == nil else {
                print("Error getting supportApp:", error!)
                return
            }

            if let data = data {
                do {
                    let string = try GetSupportAppInfoResponse(serializedData: data).jsonString()
                    print("\(string)")
                } catch {
                    print("Error decoding protobuf: \(error)")
                }
            } else {
                print("Error: no results, but no error either?")
            }
        }

    case .prepare:
        os_log("preparing (%@, %@)", log: tplogDebug, type: .default, container, context)

        if machineID == nil {
            let anisetteController = AKAnisetteProvisioningController()

            let anisetteData = try anisetteController.anisetteData()
            machineID = anisetteData.machineID
            guard machineID != nil else {
                print("failed to get machineid from anisette data")
                abort()
            }
        }

        let deviceInfo = OTDeviceInformationActualAdapter()

        tpHelper.prepare(withContainer: container,
                         context: context,
                         epoch: epoch,
                         machineID: machineID!,
                         bottleSalt: bottleSalt,
                         bottleID: UUID().uuidString,
                         modelID: modelID ?? deviceInfo.modelID(),
                         deviceName: deviceName ?? deviceInfo.deviceName(),
                         serialNumber: serialNumber ?? deviceInfo.serialNumber(),
                         osVersion: osVersion ?? deviceInfo.osVersion(),
                         policyVersion: policyVersion,
                         policySecrets: policySecrets,
                         signingPrivKeyPersistentRef: nil,
                         encPrivKeyPersistentRef: nil) {
                            peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, error in
                            guard error == nil else {
                                print("Error preparing:", error!)
                                return
                            }

                            let result = [
                                "peerID": peerID!,
                                "permanentInfo": permanentInfo!.base64EncodedString(),
                                "permanentInfoSig": permanentInfoSig!.base64EncodedString(),
                                "stableInfo": stableInfo!.base64EncodedString(),
                                "stableInfoSig": stableInfoSig!.base64EncodedString(),
                                "machineID": machineID!,
                                ]
                            do {
                                print(try TPCTLObjectiveC.jsonSerialize(cleanDictionaryForJSON(result)))
                            } catch {
                                print("Error encoding JSON: \(error)")
                            }
        }

    case .update:
        os_log("updating (%@, %@)", log: tplogDebug, type: .default, container, context)
        tpHelper.update(withContainer: container,
                        context: context,
                        deviceName: deviceName,
                        serialNumber: serialNumber,
                        osVersion: osVersion,
                        policyVersion: policyVersion,
                        policySecrets: policySecrets) { _, error in
                            guard error == nil else {
                                print("Error updating:", error!)
                                return
                            }

                            print("Update complete")
        }

    case .reset:
        os_log("resetting (%@, %@)", log: tplogDebug, type: .default, container, context)
        tpHelper.reset(withContainer: container, context: context, resetReason: .userInitiatedReset) { error in
            guard error == nil else {
                print("Error during reset:", error!)
                return
            }

            print("Reset complete")
        }

    case .validate:
        os_log("validate (%@, %@)", log: tplogDebug, type: .default, container, context)
        tpHelper.validatePeers(withContainer: container, context: context) { reply, error in
            guard error == nil else {
                print("Error validating:", error!)
                return
            }

            if let reply = reply {
                do {
                    print(try TPCTLObjectiveC.jsonSerialize(cleanDictionaryForJSON(reply)))
                } catch {
                    print("Error encoding JSON: \(error)")
                }
            } else {
                print("Error: no results, but no error either?")
            }
        }

    case .viableBottles:
        os_log("viableBottles (%@, %@)", log: tplogDebug, type: .default, container, context)
        tpHelper.fetchViableBottles(withContainer: container, context: context) { sortedBottleIDs, partialBottleIDs, error in
            guard error == nil else {
                print("Error fetching viable bottles:", error!)
                return
            }
            var result: [String: [String]] = [:]
            result["sortedBottleIDs"] = sortedBottleIDs
            result["partialBottleIDs"] = partialBottleIDs
            do {
                print(try TPCTLObjectiveC.jsonSerialize(cleanDictionaryForJSON(result)))
            } catch {
                print("Error encoding JSON: \(error)")
            }
        }

    case .vouch(let peerID, let permanentInfo, let permanentInfoSig, let stableInfo, let stableInfoSig):
        os_log("vouching (%@, %@)", log: tplogDebug, type: .default, container, context)
        tpHelper.vouch(withContainer: container,
                       context: context,
                       peerID: peerID,
                       permanentInfo: permanentInfo,
                       permanentInfoSig: permanentInfoSig,
                       stableInfo: stableInfo,
                       stableInfoSig: stableInfoSig,
                       ckksKeys: []
        ) { voucher, voucherSig, error in
            guard error == nil else {
                print("Error during vouch:", error!)
                return
            }

            do {
                let result = ["voucher": voucher!.base64EncodedString(), "voucherSig": voucherSig!.base64EncodedString()]
                print(try TPCTLObjectiveC.jsonSerialize(cleanDictionaryForJSON(result)))
            } catch {
                print("Error during processing vouch results: \(error)")
            }
        }

    case .vouchWithBottle(let bottleID, let entropy, let salt):
        os_log("vouching with bottle (%@, %@)", log: tplogDebug, type: .default, container, context)
        tpHelper.vouchWithBottle(withContainer: container,
                                 context: context,
                                 bottleID: bottleID,
                                 entropy: entropy,
                                 bottleSalt: salt,
                                 tlkShares: []) { voucher, voucherSig, error in
                                    guard error == nil else {
                                        print("Error during vouchWithBottle", error!)
                                        return
                                    }
                                    do {
                                        let result = ["voucher": voucher!.base64EncodedString(), "voucherSig": voucherSig!.base64EncodedString()]
                                        print(try TPCTLObjectiveC.jsonSerialize(cleanDictionaryForJSON(result)))
                                    } catch {
                                        print("Error during processing vouch results: \(error)")
                                    }
        }

    case .allow(let machineIDs, let performIDMS):
        os_log("allow-listing (%@, %@)", log: tplogDebug, type: .default, container, context)

        var idmsDeviceIDs: Set<String> = Set()

        if(performIDMS) {
            let store = ACAccountStore()
            guard let account = store.aa_primaryAppleAccount() else {
                print("Unable to fetch primary Apple account!")
                abort()
            }

            let requestArguments = AKDeviceListRequestContext()
            requestArguments.altDSID = account.aa_altDSID
            requestArguments.services = [AKServiceNameiCloud]

            guard let controller = AKAppleIDAuthenticationController() else {
                print("Unable to create AKAppleIDAuthenticationController!")
                abort()
            }
            let semaphore = DispatchSemaphore(value: 0)

            controller.fetchDeviceList(with: requestArguments) { deviceList, error in
                guard error == nil else {
                    print("Unable to fetch IDMS device list: \(error!)")
                    abort()
                }
                guard let deviceList = deviceList else {
                    print("IDMS returned empty device list")
                    return
                }

                idmsDeviceIDs = Set(deviceList.map { $0.machineId })
                semaphore.signal()
            }
            semaphore.wait()
        }

        let allMachineIDs = machineIDs.union(idmsDeviceIDs)
        print("Setting allowed machineIDs to \(allMachineIDs)")
        tpHelper.setAllowedMachineIDsWithContainer(container, context: context, allowedMachineIDs: allMachineIDs) { listChanged, error in
            guard error == nil else {
                print("Error during allow:", error!)
                return
            }

            print("Allow complete, differences: \(listChanged)")
        }
    }
}
