import FeatureFlags
import Foundation

class OctagonTestsMidRoll: OctagonTestsBase {
    struct Key: FeatureFlagsKey {
        let domain: StaticString
        let feature: StaticString
    }

    static func isEnabled() -> Bool {
        return isFeatureEnabled(Key(domain: "Security", feature: "RollIdentityOnMIDRotation"))
    }

    func testFeatureFlag() throws {
        XCTAssertFalse(OctagonTestsMidRoll.isEnabled(), "feature flag should be disabled")
    }

    func testFeatureFlagOverride () throws {
        XCTAssertFalse(IsRollOctagonIdentityEnabled(), "feaature flag should be disabled")

        SetRollOctagonIdentityEnabled(true)
        XCTAssertTrue(IsRollOctagonIdentityEnabled(), "feaature flag should be enabled")

        ClearRollOctagonIdentityEnabledOverride()
        XCTAssertFalse(IsRollOctagonIdentityEnabled(), "feaature flag should be disabled")

        SetRollOctagonIdentityEnabled(true)
        XCTAssertTrue(IsRollOctagonIdentityEnabled(), "feaature flag should be enabled")

        SetRollOctagonIdentityEnabled(false)
        XCTAssertFalse(IsRollOctagonIdentityEnabled(), "feaature flag should be disabled")
    }
}
