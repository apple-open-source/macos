//
//  ctf_lookup.swift
//  ctf-test
//
//  Created by tjedlicka on 5/13/22.
//

import XCTest

class ctf_lookup: XCTestCase {

	var ctf_container: UnsafeMutablePointer<ctf_file_t>?

	override func setUpWithError() throws {
		var err: Int32 = 0
		self.ctf_container = ctf_create(&err)

		ctf_setmodel(self.ctf_container, CTF_MODEL_LP64)
	}

	override func tearDownWithError() throws {
		ctf_close(ctf_container)
	}

	func getName(fp: UnsafeMutablePointer<ctf_file_t>, ctf_id: Int) -> String {
		let name = UnsafeMutablePointer<CChar>.allocate(capacity: 200)
		let ret = ctf_type_name(self.ctf_container, ctf_id_t(ctf_id), name, 200)
		XCTAssertEqual(ctf_errno(self.ctf_container), 0)
		XCTAssertEqual(ret, name)

		return String(cString:name)
	}

	func testLookupByID() {
		var encoding = ctf_encoding_t(cte_format:UInt32(CTF_INT_SIGNED), cte_offset: 0, cte_bits: 32)

		// Add simple CTF_K_INTEGER to the container
		let ctf_id = ctf_add_integer(self.ctf_container, UInt32(CTF_ADD_ROOT), "int32", &encoding)
		XCTAssertEqual(ctf_id, 1)

		// Update the container
		XCTAssertEqual(ctf_update(self.ctf_container), 0)

		var container = self.ctf_container
		let type = ctf_lookup_by_id(&container, ctf_id).bindMemory(to: ctf_type.self, capacity: 1)
		let name = String(cString: ctf_strptr(container, type.pointee.ctt_name))
		XCTAssertEqual(name, "int32")

		XCTAssertNil(ctf_lookup_by_id(&container, 10000))
	}

	func testLookupByName() {
		var encoding = ctf_encoding_t(cte_format:UInt32(CTF_INT_SIGNED), cte_offset: 0, cte_bits: 32)

		// Add simple pointer type to the container.
		let ctf_id = ctf_add_integer(self.ctf_container, UInt32(CTF_ADD_ROOT), "int32", &encoding)
		XCTAssertEqual(ctf_id, 1)

		let ptr_id = ctf_add_pointer(self.ctf_container, UInt32(CTF_ADD_ROOT), ctf_id)
		XCTAssertEqual(ptr_id, 2)

		let const_id = ctf_add_const(self.ctf_container, UInt32(CTF_ADD_ROOT), ptr_id)
		XCTAssertEqual(const_id, 3)

		let tdef_id = ctf_add_typedef(self.ctf_container, UInt32(CTF_ADD_ROOT), String("test_t"), const_id)
		XCTAssertEqual(tdef_id, 4)

		// Update the container
		XCTAssertEqual(ctf_update(self.ctf_container), 0)

		XCTAssertEqual(ctf_lookup_by_name(self.ctf_container, String("int32").cString(using: .utf8)), 1)
		XCTAssertEqual(ctf_lookup_by_name(self.ctf_container, String("int32 *").cString(using: .utf8)), 2)
		// Skips conts
		XCTAssertEqual(ctf_lookup_by_name(self.ctf_container, String("const int32 *").cString(using: .utf8)), 2)
		XCTAssertEqual(ctf_lookup_by_name(self.ctf_container, String("test_t").cString(using: .utf8)), 4)
		XCTAssertEqual(ctf_lookup_by_name(self.ctf_container, String("foobar_t").cString(using: .utf8)), -1)
	}

	func testLookupParent() {
		var err: Int32 = 0
		let child_container = ctf_create(&err)

		ctf_setmodel(child_container, CTF_MODEL_LP64)
		XCTAssertEqual(ctf_import(child_container, self.ctf_container), 0)

		// Add integer to parent container
		var encoding = ctf_encoding_t(cte_format:UInt32(CTF_INT_SIGNED), cte_offset: 0, cte_bits: 32)

		// Add simple CTF_K_INTEGER to the container
		let ctf_id = ctf_add_integer(self.ctf_container, UInt32(CTF_ADD_ROOT), "int32", &encoding)
		XCTAssertNotEqual(ctf_id, -1)

		XCTAssertEqual(ctf_update(self.ctf_container), 0)

		let id = ctf_lookup_by_name(child_container, String("int32").cString(using: .utf8))
		XCTAssertNotEqual(id, -1)
		XCTAssertEqual(getName(fp: child_container!, ctf_id: id), "int32")

		let tdef_id = ctf_add_typedef(child_container, UInt32(CTF_ADD_ROOT), String("int_t").cString(using: .utf8), id)
		XCTAssertEqual(tdef_id, 0x80000001)

		XCTAssertEqual(ctf_update(child_container), 0)
		XCTAssertEqual(ctf_type_resolve(child_container, tdef_id), id)
	}
}
