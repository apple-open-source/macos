//
//  ctf_lib.swift
//  dtrace
//
//  Created by tjedlicka on 5/11/22.
//

import XCTest

class ctf_lib: XCTestCase {

	func testVersion() {
		XCTAssertEqual(CTF_VERSION, CTF_VERSION_4)
	}

	func testContainerCreate() {
		var ctf_container: UnsafeMutablePointer<ctf_file_t>?
		var err: Int32 = 0

		ctf_container = ctf_create(&err)

		XCTAssertNotNil(ctf_container)
		XCTAssertEqual(err, 0)

		ctf_close(ctf_container)
	}
}
