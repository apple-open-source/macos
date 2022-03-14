import CoreData
import Foundation

private let logger = Logger(subsystem: "com.apple.security.trustedpeers", category: "SetValueTransformer")

@objc(SetValueTransformer)
class SetValueTransformer: ValueTransformer {

    override class func transformedValueClass() -> AnyClass {
        return NSData.self
    }

    override class func allowsReverseTransformation() -> Bool {
        return true
    }

    override func transformedValue(_ value: Any?) -> Any? {
        do {
            guard let value = value else {
                return nil
            }
            return try NSKeyedArchiver.archivedData(withRootObject: value, requiringSecureCoding: true)
        } catch {
            logger.debug("Failed to serialize a Set: \(String(describing: error), privacy: .public)")
            return nil
        }
    }

    override func reverseTransformedValue(_ value: Any?) -> Any? {
        do {
            guard let dataOp = value as? Data? else {
                return nil
            }
            guard let data = dataOp else {
                return nil
            }

            let unarchiver = try NSKeyedUnarchiver(forReadingFrom: data)
            return unarchiver.decodeObject(of: [NSSet.self, NSString.self], forKey: NSKeyedArchiveRootObjectKey)
        } catch {
            logger.debug("Failed to deserialize a purported Set: \(String(describing: error), privacy: .public)")
            return nil
        }
    }

    static let name = NSValueTransformerName(rawValue: "SetValueTransformer")
}
