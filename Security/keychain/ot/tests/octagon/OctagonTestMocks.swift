#if OCTAGON
import Foundation

class OTMockSecureBackup: NSObject, OctagonEscrowRecovererPrococol {
    let bottleID : String
    let entropy : Data

    init(bottleID: String, entropy: Data) {
        self.bottleID = bottleID
        self.entropy = entropy

        super.init()
    }

    func recover(withInfo info: [AnyHashable : Any]!,
                 results: AutoreleasingUnsafeMutablePointer<NSDictionary?>!) -> Error! {
        results.pointee = [
            "bottleID": self.bottleID,
            "bottleValid": "valid",
            "EscrowServiceEscrowData" : ["BottledPeerEntropy": entropy],
        ]
        return nil
    }
}

class OTMockFollowUpController: NSObject, OctagonFollowUpControllerProtocol {
    var postedFollowUp : Bool = false

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
