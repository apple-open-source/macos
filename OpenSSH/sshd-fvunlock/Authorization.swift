import Foundation
import Security
import Security_Private.AuthorizationPriv
import Security_Private.AuthorizationTagsPriv
import os

public final class Authorization {
    private let ref: AuthorizationRef
    private let destructionFlags: AuthorizationFlags

    init(flags: AuthorizationFlags, destructionFlags: AuthorizationFlags = []) throws {
        var authRef: AuthorizationRef?
        let status = AuthorizationCreate(nil, nil, flags, &authRef)
        guard status == noErr, let authRef else {
            throw Error.internalError(status)
        }
        self.ref = authRef
        self.destructionFlags = destructionFlags
    }

    func copyRights(
        _ rights: AuthorizationRights, environment: AuthorizationEnvironment,
        flags: AuthorizationFlags
    ) throws(Error) {
        let status = withUnsafePointer(to: rights) { rights in
            withUnsafePointer(to: environment) { environment in
                AuthorizationCopyRights(self.ref, rights, environment, flags, nil)
            }
        }
        guard status == noErr else {
            throw Error.internalError(status)
        }
    }

    static func copyPreLoginUserDB(volume uuid: UUID?, flags: UInt32) throws(Error) -> [[String:
        Any]]
    {
        // AuthorizationCopyPreloginUserDatabase has type nullability annotations that confuse swift
        // create a bogus CFArray even though it will be overwritten
        let discardableArray = [] as CFArray
        var db: Unmanaged<CFArray> = .passUnretained(discardableArray)
        let status: OSStatus

        if let uuid {
            status = uuid.uuidString.withCString { uuidStr in
                AuthorizationCopyPreloginUserDatabase(uuidStr, flags, &db)
            }
        } else {
            status = AuthorizationCopyPreloginUserDatabase(nil, flags, &db)
        }

        guard status == noErr else {
            throw Error.internalError(status)
        }
        return db.takeRetainedValue() as! [[String: Any]]
    }

    static func airUserRecord(for user: String, volume: UUID) throws(Error) -> [String: Any]? {
        let db = try copyPreLoginUserDB(volume: volume, flags: 0)
        return db.first { air in
            air[PLUDB_USERNAME] as? String == user
        }
    }

    deinit {
        AuthorizationFree(ref, destructionFlags)
    }
}

extension Authorization {
    public enum Error: Swift.Error {
        case internalError(OSStatus)
    }
}
