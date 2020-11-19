#if OCTAGON
import Foundation

class OTMockSecureBackup: NSObject, OctagonEscrowRecovererPrococol {
    let bottleID: String?
    let entropy: Data?

    init(bottleID: String?, entropy: Data?) {
        self.bottleID = bottleID
        self.entropy = entropy

        super.init()
    }

    func recover(withInfo info: [AnyHashable: Any]?,
                 results: AutoreleasingUnsafeMutablePointer<NSDictionary?>?) -> Error? {
        if self.bottleID == nil && self.entropy == nil {
            results!.pointee = [
                "bottleValid": "invalid",
            ]
        } else if self.bottleID == nil && self.entropy != nil {
            results!.pointee = [
                "EscrowServiceEscrowData": ["BottledPeerEntropy": self.entropy],
                "bottleValid": "invalid",
            ]
        } else if self.bottleID != nil && self.entropy == nil {
            results!.pointee = [
                "bottleID": self.bottleID!,
                "bottleValid": "invalid",
            ]
        } else { //entropy and bottleID must exist, so its a good bottle.
            results!.pointee = [
                "bottleID": self.bottleID!,
                "bottleValid": "valid",
                "EscrowServiceEscrowData": ["BottledPeerEntropy": self.entropy],
            ]
        }
        return nil
    }

    func disable(withInfo info: [AnyHashable: Any]!) -> Error? {
        return nil
    }

    @objc func getAccountInfo(withInfo info: [AnyHashable: Any]!, results: AutoreleasingUnsafeMutablePointer<NSDictionary?>!) -> Error? {
        let recordData = accountInfoWithInfoSample.data(using: .utf8)!
        var propertyListFormat = PropertyListSerialization.PropertyListFormat.xml
        do {
            results.pointee = try PropertyListSerialization.propertyList(from: recordData, options: .mutableContainersAndLeaves, format: &propertyListFormat) as! [String: AnyObject] as NSDictionary
        } catch {
            print("Error reading plist: \(error), format: \(propertyListFormat)")
        }
        return nil
    }
}

class OTMockFollowUpController: NSObject, OctagonFollowUpControllerProtocol {
    var postedFollowUp: Bool = false

    override init() {
        super.init()
    }

    func postFollowUp(with context: CDPFollowUpContext) throws {
        self.postedFollowUp = true
    }

    func clearFollowUp(with context: CDPFollowUpContext) throws {
    }
}

#endif // OCTAGON
