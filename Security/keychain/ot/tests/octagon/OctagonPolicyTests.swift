import Foundation

class OctagonPolicyTests: XCTestCase {
    func prevailingPolicy() throws -> TPPolicy {
        let policyDocument = builtInPolicyDocuments().filter { $0.version.versionNumber == prevailingPolicyVersion.versionNumber }.first
        XCTAssertNotNil(policyDocument, "Should have a prevailing policy document")

        let policy = try policyDocument!.policy(withSecrets: [:], decrypter: Decrypter())
        return policy
    }

    func testPrevailingPolicyHasModelsForAllViewRules() throws {
        let policy = try self.prevailingPolicy()

        // There's a rule for 'NotSynced', but we explicitly don't want to bring up that view
        let viewsWithRules = Set(policy.keyViewMapping.compactMap { $0.view }).subtracting(Set(["NotSynced"]))
        XCTAssertEqual(Set(policy.categoriesByView.keys),
                       viewsWithRules,
                       "Every view should have a rule")
    }

    func testPrevailingPolicyIntroducerGraphIsClosed() throws {
        // Let's check that, for each model, introducing a new peer doesn't give them more introduction power
        let policy = try self.prevailingPolicy()

        policy.introducersByCategory.forEach { category, introducers in
            let allowedIntroducers = Set(introducers)
            let allAccessible = Set(allowedIntroducers.compactMap { category in policy.introducersByCategory[category] }.joined())

            XCTAssertEqual(allowedIntroducers, allAccessible, "A device class shouldn't be able to introduce a peer which can introduce a peer that the original device couldn't introduce")
        }
    }

    func testPrePolicyViews() throws {
        let policy = try self.prevailingPolicy()

        XCTAssert(policy.keyViewMapping.count >= 7, "Should always have 7 basic views")

        let firstSevenRules = policy.keyViewMapping[..<7]

        let firstSevenViews = firstSevenRules.compactMap { $0.view }
        XCTAssertEqual(Set(firstSevenViews),
                       Set(["ApplePay", "AutoUnlock", "Engram", "Health", "Home", "Manatee", "LimitedPeersAllowed"]),
                       "First seven rules should be for prepolicy views")

        // Now, ensure that the first seven rules match vwht only
        firstSevenRules.forEach { viewRule in
            XCTAssertNotNil(viewRule.view, "View rule should have a name")
            XCTAssertNotNil(viewRule.matchingRule, "Rule should have a matching rule")
            let rule = viewRule.matchingRule!

            XCTAssertNotNil(rule.match, "Rule should have a match subrule")
            XCTAssertEqual(rule.match.fieldName, "vwht", "Rule should match field vwht")
            XCTAssertEqual(rule.match.regex, "^\(viewRule.view!)$", "Regex should match field name")
        }
    }
}
