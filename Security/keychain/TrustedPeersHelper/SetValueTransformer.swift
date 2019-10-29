import CoreData
import Foundation

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
            guard let value = value else { return nil }
            return try NSKeyedArchiver.archivedData(withRootObject: value, requiringSecureCoding: true)
        } catch {
            os_log("Failed to serialize a Set: %@", log: tplogDebug, type: .default, error as CVarArg)
            return nil
        }
    }

    override func reverseTransformedValue(_ value: Any?) -> Any? {
        do {
            guard let dataOp = value as? Data? else { return nil }
            guard let data = dataOp else { return nil }

            let unarchiver = try NSKeyedUnarchiver(forReadingFrom: data)
            return unarchiver.decodeObject(of: [NSSet.self], forKey: NSKeyedArchiveRootObjectKey)
        } catch {
            os_log("Failed to deserialize a purported Set: %@", log: tplogDebug, type: .default, error as CVarArg)
            return nil
        }
    }

    static let name = NSValueTransformerName(rawValue: "SetValueTransformer")
}
