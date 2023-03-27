//
//  ctf_test.swift
//  ctf-test
//
//  Created by tjedlicka on 5/11/22.
//

import XCTest

var enumerator: [String:Int] = [:]
var structmem: [String:Int] = [:]

class ctf_id: XCTestCase {

	var ctf_container: UnsafeMutablePointer<ctf_file_t>?

	override func setUpWithError() throws {
		var err: Int32 = 0
		self.ctf_container = ctf_create(&err)

		ctf_setmodel(self.ctf_container, CTF_MODEL_LP64)
	}

	override func tearDownWithError() throws {
		ctf_close(ctf_container)
	}

	func getName(ctf_id: Int) -> String {
		let name = UnsafeMutablePointer<CChar>.allocate(capacity: 200)
		let ret = ctf_type_name(self.ctf_container, ctf_id_t(ctf_id), name, 200)
		XCTAssertEqual(ctf_errno(self.ctf_container), 0)
		XCTAssertEqual(ret, name)

		return String(cString:name)
	}

	func addInteger() -> Int {
		var encoding = ctf_encoding_t(cte_format:UInt32(CTF_INT_SIGNED), cte_offset: 0, cte_bits: 32)

		// Add simple CTF_K_INTEGER to the container
		let ctf_id = ctf_add_integer(self.ctf_container, UInt32(CTF_ADD_ROOT), "int32", &encoding)
		XCTAssertNotEqual(ctf_id, -1)

		return ctf_id
	}

	func testInteger() {
		let ctf_id = addInteger()

		// Update the container
		XCTAssertEqual(ctf_update(self.ctf_container), 0)

		// Test ID -> name
		let name = getName(ctf_id: ctf_id)
		XCTAssertEqual(name, "int32")

		// Test ID -> size
		let size = ctf_type_size(self.ctf_container, ctf_id)
		XCTAssertEqual(size, 4)
	}

	func testArray() {
		// int [long]
		var encval = ctf_encoding_t(cte_format:UInt32(CTF_INT_SIGNED), cte_offset: 0, cte_bits: 32)
		let ctf_val_id = ctf_add_integer(self.ctf_container, UInt32(CTF_ADD_ROOT), "int32", &encval)
		XCTAssertEqual(ctf_val_id, 1)

		var encidx = ctf_encoding_t(cte_format: 0, cte_offset: 0, cte_bits: 64)
		let ctf_idx_id = ctf_add_integer(self.ctf_container, UInt32(CTF_ADD_ROOT), "long-idx", &encidx)
		XCTAssertEqual(ctf_idx_id, 2)

		var arrayinfo = ctf_arinfo_t(ctr_contents: ctf_val_id, ctr_index: ctf_idx_id, ctr_nelems: 100)
		let ctf_arr_id = ctf_add_array(self.ctf_container, UInt32(CTF_ADD_ROOT), &arrayinfo)
		XCTAssertEqual(ctf_arr_id, 3)

		// Update container
		XCTAssertEqual(ctf_update(self.ctf_container), 0)

		// Check getters
		var tstinfo: ctf_arinfo_t = ctf_arinfo_t()
		let ret = ctf_array_info(self.ctf_container, ctf_arr_id, &tstinfo)
		XCTAssertEqual(ret, 0)

		XCTAssertEqual(getName(ctf_id: tstinfo.ctr_contents), "int32")
		XCTAssertEqual(getName(ctf_id: tstinfo.ctr_index), "long-idx")
		XCTAssertEqual(tstinfo.ctr_nelems, 100)
	}

	func testStruct() {
		// struct test_struct {
		//     int32   mem_int;
		//     long    mem_long;
		// }
		var encint = ctf_encoding_t(cte_format:UInt32(CTF_INT_SIGNED), cte_offset: 0, cte_bits: 32)
		let mem_int_id = ctf_add_integer(self.ctf_container, UInt32(CTF_ADD_ROOT), "int32", &encint)
		XCTAssertEqual(mem_int_id, 1)

		var enclong = ctf_encoding_t(cte_format: 0, cte_offset: 0, cte_bits: 64)
		let mem_long_id = ctf_add_integer(self.ctf_container, UInt32(CTF_ADD_ROOT), "long", &enclong)
		XCTAssertEqual(mem_long_id, 2)

		let struct_id = ctf_add_struct(self.ctf_container, UInt32(CTF_ADD_ROOT), String("test_struct").cString(using: .utf8))
		XCTAssertEqual(struct_id, 3)

		// Update container
		XCTAssertEqual(ctf_update(self.ctf_container), 0)

		ctf_add_member(self.ctf_container, struct_id, String("mem_int").cString(using: .utf8), mem_int_id)
		ctf_add_member(self.ctf_container, struct_id, String("mem_long").cString(using: .utf8), mem_long_id)

		// Update container
		XCTAssertEqual(ctf_update(self.ctf_container), 0)
		XCTAssertEqual(getName(ctf_id: struct_id), "struct test_struct")

		// Enumerate members
		ctf_member_iter(self.ctf_container, struct_id, { (name, ctf_id, _, _) -> Int32 in
			structmem[String(cString: name!)] = ctf_id
			return 0
		}, nil)
		XCTAssertEqual(structmem, [ "mem_int": mem_int_id, "mem_long": mem_long_id])

		// member info
		var mi: ctf_membinfo_t = ctf_membinfo_t()
		XCTAssertEqual(ctf_member_info(self.ctf_container, struct_id, String("mem_int").cString(using: .utf8), &mi), 0)
		XCTAssertEqual(mi.ctm_type, mem_int_id)
		XCTAssertEqual(mi.ctm_offset, 0)
	}

	func testStructLargeMember() {
		// struct test_struct {
		//     int32   mem_array[20000];
		//     long    mem_long;   <-- enforces use of lmember due to large offset.
		// }

		// Create array definition
		// int [long]
		var encval = ctf_encoding_t(cte_format:UInt32(CTF_INT_SIGNED), cte_offset: 0, cte_bits: 32)
		let ctf_val_id = ctf_add_integer(self.ctf_container, UInt32(CTF_ADD_ROOT), "int32", &encval)
		XCTAssertEqual(ctf_val_id, 1)

		var encidx = ctf_encoding_t(cte_format: 0, cte_offset: 0, cte_bits: 64)
		let ctf_idx_id = ctf_add_integer(self.ctf_container, UInt32(CTF_ADD_ROOT), "long", &encidx)
		XCTAssertEqual(ctf_idx_id, 2)

		var arrayinfo = ctf_arinfo_t(ctr_contents: ctf_val_id, ctr_index: ctf_idx_id, ctr_nelems: 20000)
		let mem_array_id = ctf_add_array(self.ctf_container, UInt32(CTF_ADD_ROOT), &arrayinfo)
		XCTAssertEqual(mem_array_id, 3)

		// Update container
		XCTAssertEqual(ctf_update(self.ctf_container), 0)
		XCTAssertEqual(ctf_type_size(self.ctf_container, mem_array_id), 4 * 20000)

		var enclong = ctf_encoding_t(cte_format: 0, cte_offset: 0, cte_bits: 64)
		let mem_long_id = ctf_add_integer(self.ctf_container, UInt32(CTF_ADD_ROOT), "long", &enclong)
		XCTAssertEqual(mem_long_id, 4)

		let struct_id = ctf_add_struct(self.ctf_container, UInt32(CTF_ADD_ROOT), String("test_struct").cString(using: .utf8))
		XCTAssertEqual(struct_id, 5)

		// Update container
		XCTAssertEqual(ctf_update(self.ctf_container), 0)

		ctf_add_member(self.ctf_container, struct_id, String("mem_array").cString(using: .utf8), mem_array_id)
		ctf_add_member(self.ctf_container, struct_id, String("mem_long").cString(using: .utf8), mem_long_id)

		// Update container
		XCTAssertEqual(ctf_update(self.ctf_container), 0)
	}

	func testUnion() {
		// union test_struct {
		//     int32   mem_int;
		//     long    mem_long;
		// }

		var encint = ctf_encoding_t(cte_format:UInt32(CTF_INT_SIGNED), cte_offset: 0, cte_bits: 32)
		let mem_int_id = ctf_add_integer(self.ctf_container, UInt32(CTF_ADD_ROOT), "int32", &encint)
		XCTAssertEqual(mem_int_id, 1)

		var enclong = ctf_encoding_t(cte_format: 0, cte_offset: 0, cte_bits: 64)
		let mem_long_id = ctf_add_integer(self.ctf_container, UInt32(CTF_ADD_ROOT), "long", &enclong)
		XCTAssertEqual(mem_long_id, 2)

		let struct_id = ctf_add_union(self.ctf_container, UInt32(CTF_ADD_ROOT), String("test_struct").cString(using: .utf8))
		XCTAssertEqual(struct_id, 3)

		ctf_add_member(self.ctf_container, struct_id, String("mem_int").cString(using: .utf8), mem_int_id)
		ctf_add_member(self.ctf_container, struct_id, String("mem_long").cString(using: .utf8), mem_long_id)

		// Update container
		let err = ctf_update(self.ctf_container)
		XCTAssertEqual(err, 0)

		XCTAssertEqual(getName(ctf_id: struct_id), "union test_struct")

		// Not testing memebers as that's covered with struct tests.
	}

	func testEnum() {
		// enum test_enum {
		//    ENUM_ONE = 100;
		//    ENUM_TWO = 200;
		// }

		let enum_id = ctf_add_enum(self.ctf_container, UInt32(CTF_ADD_ROOT), String("test_enum").cString(using: .utf8))
		XCTAssertEqual(enum_id, 1)

		XCTAssertEqual(ctf_add_enumerator(self.ctf_container, enum_id, String("ENUM_ONE"), 100), 0)
		XCTAssertEqual(ctf_add_enumerator(self.ctf_container, enum_id, String("ENUM_TWO"), 200), 0)

		// Update the container
		let err = ctf_update(self.ctf_container)
		XCTAssertEqual(err, 0)

		XCTAssertEqual(getName(ctf_id: enum_id), "enum test_enum")

		// Walk all of the members
		ctf_enum_iter(self.ctf_container, enum_id, { (name, value, _) -> Int32 in
			// keep this outside of this class. We can't capture self in C pointer.
			enumerator[String(cString: name!)] = Int(value)
			return 0
		}, nil)

		// Check enumartor lookups
		XCTAssertEqual(enumerator, [ "ENUM_ONE": 100, "ENUM_TWO": 200])

		let ename = String(String(cString: ctf_enum_name(self.ctf_container, enum_id, 200)))
		XCTAssertEqual(ename, "ENUM_TWO")

		XCTAssertNil(ctf_enum_name(self.ctf_container, enum_id, 5000))

		var evalue: Int32 = 0
		XCTAssertEqual(ctf_enum_value(self.ctf_container, enum_id, String("ENUM_ONE").cString(using: .utf8), &evalue), 0)
		XCTAssertEqual(evalue, 100)
	}

	func testFloat() {

	}

	func testConst() {
		let int_id = addInteger()
		let const_id = ctf_add_const(self.ctf_container, UInt32(CTF_ADD_ROOT), int_id)
		XCTAssertNotEqual(const_id, -1)
		let ptr_id = addPointer(ctf_id: const_id)

		XCTAssertEqual(ctf_update(self.ctf_container), 0)

		XCTAssertEqual(getName(ctf_id: ptr_id), "const int32 *")
	}

	func testForward() {
		let fwd_id = ctf_add_forward(self.ctf_container, UInt32(CTF_ADD_ROOT), String("vnode_t").cString(using: .utf8), UInt32(CTF_K_STRUCT))
		XCTAssertNotEqual(fwd_id, -1)

		XCTAssertEqual(ctf_update(self.ctf_container), 0)

		XCTAssertEqual(getName(ctf_id: fwd_id), "struct vnode_t")
		XCTAssertEqual(ctf_type_kind(self.ctf_container, fwd_id), CTF_K_FORWARD)
	}

	func addPointer(ctf_id: Int) -> Int {
		let ptr_id = ctf_add_pointer(self.ctf_container, UInt32(CTF_ADD_ROOT), ctf_id)
		XCTAssertNotEqual(ptr_id, -1)

		return ptr_id
	}

	func testPointer() {
		let int_id = addInteger()
		let ptr_id = addPointer(ctf_id: int_id)
		XCTAssertNotEqual(ptr_id, -1)

		// Update container
		XCTAssertEqual(ctf_update(self.ctf_container), 0)

		XCTAssertEqual(getName(ctf_id: ptr_id), "int32 *")

		let p_id = ctf_type_pointer(self.ctf_container, int_id)
		XCTAssertEqual(p_id, ptr_id)
	}

	func testTypedef() {
		let ptr_id = addPointer(ctf_id: addInteger())
		XCTAssertNotEqual(ptr_id, -1)

		let tdef_id = ctf_add_typedef(self.ctf_container, UInt32(CTF_ADD_ROOT), String("ptr_t").cString(using: .utf8), ptr_id)

		// Update container
		XCTAssertEqual(ctf_update(self.ctf_container), 0)
		XCTAssertEqual(getName(ctf_id: tdef_id), "ptr_t")
		XCTAssertEqual(getName(ctf_id: ctf_type_resolve(self.ctf_container, tdef_id)), "int32 *")
	}

	func testRestrict() {
		let int_id = addInteger()
		let vol_id = ctf_add_restrict(self.ctf_container, UInt32(CTF_ADD_ROOT), int_id)
		let ptr_id = addPointer(ctf_id: vol_id)

		XCTAssertEqual(ctf_update(self.ctf_container), 0)

		XCTAssertEqual(getName(ctf_id: ptr_id), "restrict int32 *")

	}

	func testVolatile() {
		let int_id = addInteger()
		let vol_id = ctf_add_volatile(self.ctf_container, UInt32(CTF_ADD_ROOT), int_id)
		let ptr_id = addPointer(ctf_id: vol_id)

		XCTAssertEqual(ctf_update(self.ctf_container), 0)

		XCTAssertEqual(getName(ctf_id: ptr_id), "volatile int32 *")
	}

	func testLargeID() {
		var encint = ctf_encoding_t(cte_format:UInt32(CTF_INT_SIGNED), cte_offset: 0, cte_bits: 32)

		// Fill in the container
		for i in 1...80000 {
			let ctf_id = ctf_add_integer(self.ctf_container, UInt32(CTF_ADD_ROOT), "int32", &encint)
			XCTAssertEqual(ctf_id, i)
		}

		// Add extra type
		let last_id = ctf_add_integer(self.ctf_container, UInt32(CTF_ADD_ROOT), "last_int32", &encint)
		XCTAssertEqual(last_id, 80001)

		// Update the container
		measure {
			let err = ctf_update(self.ctf_container)
			XCTAssertEqual(err, 0)
		}

		XCTAssertEqual(getName(ctf_id: last_id), "last_int32")
	}

	func testRemoval() {
		var encoding = ctf_encoding_t(cte_format:UInt32(CTF_INT_SIGNED), cte_offset: 0, cte_bits: 32)

		// Add simple CTF_K_INTEGER to the container
		let ctf_id = ctf_add_integer(self.ctf_container, UInt32(CTF_ADD_ROOT), "int32", &encoding)
		XCTAssertEqual(ctf_id, 1)

		// Update the container
		XCTAssertEqual(ctf_update(self.ctf_container), 0)

		// Test ID -> name
		let name = getName(ctf_id: ctf_id)
		XCTAssertEqual(name, "int32")

		// Test ID -> size
		let size = ctf_type_size(self.ctf_container, ctf_id)
		XCTAssertEqual(size, 4)

		XCTAssertEqual(ctf_delete_type(self.ctf_container, ctf_id), 0)
		XCTAssertEqual(ctf_update(self.ctf_container), 0)

		// Check that the type is gone
		XCTAssertEqual(ctf_type_size(self.ctf_container, ctf_id), -1)
	}

	func testDiscard() {
		// Add simple CTF_K_INTEGER to the container
		let ctf_id_1 = addInteger()
		XCTAssertEqual(ctf_id_1, 1)

		// Update the container
		XCTAssertEqual(ctf_update(self.ctf_container), 0)

		// Add simple CTF_K_INTEGER to the container
		let ctf_id_2 = addInteger()
		XCTAssertEqual(ctf_id_2, 2)

		// Discard last uncommited type
		XCTAssertEqual(ctf_discard(self.ctf_container), 0)

		// Check for expected contents
		XCTAssertEqual(ctf_type_size(self.ctf_container, ctf_id_1), 4)
		XCTAssertEqual(ctf_type_size(self.ctf_container, ctf_id_2), -1)
	}
}
