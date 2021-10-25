#if OCTAGON

import Foundation

class OctagonStateMachineConsistencyTests: OctagonTestsBase {
    func testMaps() throws {
        let forwardMap = OTStates.octagonStateMap()
        let reverseMap = OTStates.octagonStateInverseMap()
        XCTAssertEqual(forwardMap.count, reverseMap.count)
        var names = Set<String>()
        for (_, stateString) in reverseMap {
            if names.contains(stateString) {
                XCTFail("Duplicate state name \(stateString)")
            }
            names.insert(stateString)
        }
        var inverseMap = [NSNumber: String]()
        for (k, v) in forwardMap {
            if inverseMap[v] != nil {
                XCTFail("Duplicate mapping for \(v): \(inverseMap[v]!) and \(k)")
            }
            inverseMap[v] = k
        }
    }
}

#endif
