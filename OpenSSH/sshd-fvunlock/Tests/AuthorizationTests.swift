//  Copyright © 2025 Apple Inc. All rights reserved.

import Foundation
import Security
import Security_Private.AuthorizationPriv
import Security_Private.AuthorizationTagsPriv
import Testing

@Suite
struct AuthorizationTests {

    @Test
    func construction() throws {
        #expect(throws: Never.self) {
            try Authorization(flags: [])
        }

    }

    @Test
    func preloginDB() throws {
        let db = try Authorization.copyPreLoginUserDB(volume: nil, flags: 0)
        #expect(!db.isEmpty)
        #expect(
            db.contains(where: {
                $0[PLUDB_USERNAME] as? String == NSUserName()
            }))
    }
}
