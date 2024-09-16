import Foundation

class OctagonPolicyTests: XCTestCase {
    func prevailingPolicy() throws -> TPPolicy {
        let policyDocument = prevailingPolicyDoc
        XCTAssertNotNil(policyDocument, "Should have a prevailing policy document")

        let policy = try policyDocument!.policy(withSecrets: [:], decrypter: PolicyRedactionCrypter())
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

    func testPrevailingPolicyOutranksKnownPolicies() throws {
        let prevailingPolicy = try self.prevailingPolicy()

        let builtinPoliciesWithHigherVersionNumbers = builtInPolicyDocumentsFilteredByVersion { $0 > prevailingPolicy.version.versionNumber }
        XCTAssert(builtinPoliciesWithHigherVersionNumbers.isEmpty, "Should have no built-in policies with a higher version number than the prevailing policy")
    }

    // Run this when you need to generate the encoded data & hash for a new policy
    func testValidatePolicies() throws {
        XCTAssert(validatePolicies(), "policies need to validate")
    }

    func testPolicyRedactionMerging() throws {
        let policyCrypto = PolicyRedactionCrypter()
        let redactionSecretKey = policyCrypto.randomKey()

        // The new iCycle is a completely new category of device.
        // Note: The introducersByCategory redaction should _merge_, not replace.
        // Note: having an icycle be able to sponsor in audio/tv peers is not semantically useful (as it doesn't have all the views they have); this test is for the machinery only
        let redaction: TPPBPolicyRedaction = try TPPolicyDocument.redaction(with: policyCrypto,
                                                                             redactionName: "r1",
                                                                             encryptionKey: redactionSecretKey,
                                                                             modelToCategory: [
                                                                                TPCategoryRule(prefix: "iCycle", category: "icycle"),
                                                                             ],
                                                                             categoriesByView: [ "LimitedPeersAllowed": ["icycle"]],
                                                                             introducersByCategory: [
                                                                                "audio": ["icycle"],
                                                                                "tv": ["icycle"],
                                                                                "watch": ["icycle"],
                                                                                "windows": ["icycle"],
                                                                                "icycle": ["icycle", "full", "watch", "tv", "audio", "windows"],
                                                                             ],
                                                                             keyViewMapping: [])

        let prevailingDoc = try XCTUnwrap(prevailingPolicyDoc)
        let newPolicyDocWithRedaction = prevailingDoc.clone(withVersionNumber: prevailingDoc.version.versionNumber + 1,
                                                            prependingCategoriesByView: nil,
                                                            prependingKeyViewMapping: nil,
                                                            prepending: [redaction])

        let newPolicyRedacted = try XCTUnwrap(newPolicyDocWithRedaction.policy(
            withSecrets: [:],
            decrypter: PolicyRedactionCrypter()
        ))

        let newPolicyWithCleartext = try XCTUnwrap(newPolicyDocWithRedaction.policy(
            withSecrets: ["r1": redactionSecretKey],
            decrypter: PolicyRedactionCrypter()
        ))

        XCTAssertNil(newPolicyRedacted.category(forModel: "iCycle1,1"), "Redacted policy should not know about icycle model IDs")
        XCTAssertEqual(newPolicyWithCleartext.category(forModel: "iCycle1,1"), "icycle", "Redacted policy should know about icycle model IDs")

        XCTAssertNil(newPolicyRedacted.introducersByCategory["icycle"], "Redacted policy should not know about icycle introducers")
        XCTAssertEqual(try XCTUnwrap(newPolicyWithCleartext.introducersByCategory["icycle"]), ["icycle", "full", "watch", "tv", "audio", "windows"], "Unredacted policy should have the full list of introducers for 'icycle'")

        XCTAssertEqual(try XCTUnwrap(newPolicyRedacted.introducersByCategory["full"]), ["full", "watch"], "Redacted policy should have the full list of introducers for 'full'")
        XCTAssertEqual(try XCTUnwrap(newPolicyWithCleartext.introducersByCategory["full"]), ["full", "watch"], "Unredacted policy should have the full list of introducers for 'full'")

        XCTAssertEqual(try XCTUnwrap(newPolicyRedacted.introducersByCategory["audio"]), ["full", "watch", "audio"], "Redacted policy should have the original list of introducers for 'audio'")
        XCTAssertEqual(try XCTUnwrap(newPolicyWithCleartext.introducersByCategory["audio"]), ["full", "watch", "audio", "icycle"], "Unredacted policy should have the original list of introducers, plus icycle, for 'audio'")

        XCTAssertEqual(try XCTUnwrap(newPolicyRedacted.categoriesByView["LimitedPeersAllowed"]), ["full", "watch", "tv", "audio", "windows"], "Redacted policy should have the previous list of categories for 'LimitedPeersAllowed'")
        XCTAssertEqual(try XCTUnwrap(newPolicyWithCleartext.categoriesByView["LimitedPeersAllowed"]), ["full", "watch", "tv", "audio", "windows", "icycle"], "Unredacted policy should include 'icycle' in list of categories for 'LimitedPeersAllowed'")
    }
}
