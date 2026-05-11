//
//  TPWalrusExtraArgumentsTests.swift
//
//  Copyright (c) 2026 Apple Inc. All Rights Reserved.
//

import Foundation
import XCTest

#if OCTAGON

class TPWalrusExtraArgumentsTests: XCTestCase {

    func testEncodingAndDecoding() throws {
        let originalArgs = TPWalrusExtraArguments()

        #if APPLE_FEATURE_DBR
        originalArgs.isDBRv2 = true
        originalArgs.pdpState = 42
        #endif

        // Encode the object
        let archiver = NSKeyedArchiver(requiringSecureCoding: true)
        archiver.encode(originalArgs, forKey: NSKeyedArchiveRootObjectKey)
        archiver.finishEncoding()
        let data = archiver.encodedData

        // Decode the object
        let unarchiver = try NSKeyedUnarchiver(forReadingFrom: data)
        unarchiver.requiresSecureCoding = true
        let decodedArgs = unarchiver.decodeObject(
            of: TPWalrusExtraArguments.self,
            forKey: NSKeyedArchiveRootObjectKey
        )
        unarchiver.finishDecoding()

        // Verify the decoded object
        XCTAssertNotNil(decodedArgs, "Decoded object should not be nil")

        #if APPLE_FEATURE_DBR
        XCTAssertEqual(decodedArgs?.isDBRv2, originalArgs.isDBRv2,
                      "isDBRv2 should be preserved after encoding/decoding")
        XCTAssertEqual(decodedArgs?.pdpState, originalArgs.pdpState,
                      "pdpState should be preserved after encoding/decoding")
        #endif
    }

    #if APPLE_FEATURE_DBR
    func testDescription() {
        let walrusArgs = TPWalrusExtraArguments()
        walrusArgs.isDBRv2 = true
        walrusArgs.pdpState = 5

        XCTAssertEqual("<TPWalrusExtraArguments(isDBRv2:YES pdpState:5)>", walrusArgs.description)
    }
    #else
    func testDescription() {
        let walrusArgs = TPWalrusExtraArguments()

        XCTAssertEqual("<TPWalrusExtraArguments()>", walrusArgs.description)
    }
    #endif
}

#endif // OCTAGON
