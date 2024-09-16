#ifndef __TMO_TEST_DEFS_H__
#define __TMO_TEST_DEFS_H__

#include <stdint.h>

// Real TMO

typedef void *test_voidpointer_t;

#define TMO_TEST_STRUCTS_L1(action, block, f1, f2, f3, f4, f5, f6, f7) \
	action(block, f1, f2, f3, f4, f5, f6, f7, uint64_t) \
	action(block, f1, f2, f3, f4, f5, f6, f7, test_voidpointer_t)

#define TMO_TEST_STRUCTS_L2(action, block, f1, f2, f3, f4, f5, f6) \
	TMO_TEST_STRUCTS_L1(action, block, f1, f2, f3, f4, f5, f6, uint64_t) \
	TMO_TEST_STRUCTS_L1(action, block, f1, f2, f3, f4, f5, f6, test_voidpointer_t)

#define TMO_TEST_STRUCTS_L3(action, block, f1, f2, f3, f4, f5) \
	TMO_TEST_STRUCTS_L2(action, block, f1, f2, f3, f4, f5, uint64_t) \
	TMO_TEST_STRUCTS_L2(action, block, f1, f2, f3, f4, f5, test_voidpointer_t)

#define TMO_TEST_STRUCTS_L4(action, block, f1, f2, f3, f4) \
	TMO_TEST_STRUCTS_L3(action, block, f1, f2, f3, f4, uint64_t) \
	TMO_TEST_STRUCTS_L3(action, block, f1, f2, f3, f4, test_voidpointer_t)

#define TMO_TEST_STRUCTS_L5(action, block, f1, f2, f3) \
	TMO_TEST_STRUCTS_L4(action, block, f1, f2, f3, uint64_t) \
	TMO_TEST_STRUCTS_L4(action, block, f1, f2, f3, test_voidpointer_t)

#define TMO_TEST_STRUCTS_L6(action, block, f1, f2) \
	TMO_TEST_STRUCTS_L5(action, block, f1, f2, uint64_t) \
	TMO_TEST_STRUCTS_L5(action, block, f1, f2, test_voidpointer_t)

#define TMO_TEST_STRUCTS_L7(action, block, f1) \
	TMO_TEST_STRUCTS_L6(action, block, f1, uint64_t) \
	TMO_TEST_STRUCTS_L6(action, block, f1, test_voidpointer_t)

#define FOREACH_TMO_TEST_STRUCT(action, block) \
	TMO_TEST_STRUCTS_L7(action, block, uint64_t) \
	TMO_TEST_STRUCTS_L7(action, block, test_voidpointer_t)

#define DEFINE_TMO_TEST_STRUCT_TYPE(block, f1, f2, f3, f4, f5, f6, f7, f8) \
	struct tmo_test_##f1##_##f2##_##_##f3##_##f4##_##f5##_##f6##_##f7##_##f8 { \
		f1 m1; \
		f2 m2; \
		f3 m3; \
		f4 m4; \
		f5 m5; \
		f6 m6; \
		f7 m7; \
		f8 m8; \
		test_voidpointer_t p; \
		uint8_t pad[440]; \
	};

FOREACH_TMO_TEST_STRUCT(DEFINE_TMO_TEST_STRUCT_TYPE, "")

#define INVOKE_FOR_TMO_TEST_STRUCT_TYPE(block, f1, f2, f3, f4, f5, f6, f7, f8) \
	block(struct tmo_test_##f1##_##f2##_##_##f3##_##f4##_##f5##_##f6##_##f7##_##f8);

#define INVOKE_FOR_TMO_TEST_STRUCT_CPP_TYPE(block, f1, f2, f3, f4, f5, f6, f7, f8) \
	block(tmo_test_##f1##_##f2##_##_##f3##_##f4##_##f5##_##f6##_##f7##_##f8);

#define N_TMO_TEST_STRUCTS 256

// Callsite TMO

static volatile int dummy;

// The dummy stores are to prevent all of the callsites from being the same
// number of instructions apart, as that can make it through our scrambling of
// the type descriptor and cause us to fail to distribute across the buckets as
// intended
#define _CALL_10(expr) \
		(expr); \
		(expr); \
		dummy = 42; \
		(expr); \
		dummy = 42; \
		dummy = 42; \
		(expr); \
		(expr); \
		(expr); \
		dummy = 42; \
		(expr); \
		(expr); \
		(expr); \
		(expr)

#define CALL_100(expr) \
		_CALL_10(expr); \
		_CALL_10(expr); \
		_CALL_10(expr); \
		_CALL_10(expr); \
		_CALL_10(expr); \
		_CALL_10(expr); \
		_CALL_10(expr); \
		_CALL_10(expr); \
		_CALL_10(expr); \
		_CALL_10(expr)

#define N_UNIQUE_CALLSITES 200

// must match constant above
#define CALL_N_CALLSITES(expr) \
		CALL_100(expr); \
		CALL_100(expr)

// Swift TMO

#define N_TEST_SWIFT_CLASSES 200

#endif // __TMO_TEST_DEFS_H__
