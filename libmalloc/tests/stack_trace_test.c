//
//  stack_trace_test.c
//  libmalloc
//
//  Unit tests for collecting compressed stack traces.
//

#include <darwintest.h>

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(TRUE));

#include "../src/stack_trace.c"

#include <stdlib.h>  // arc4random_buf()


T_DECL(zigzag_encode, "zigzag_encode")
{
	T_EXPECT_EQ(zigzag_encode(0), 0ul, NULL);
	T_EXPECT_EQ(zigzag_encode(1), 2ul, NULL);
	T_EXPECT_EQ(zigzag_encode(2), 4ul, NULL);

	T_EXPECT_EQ(zigzag_encode(-1), 1ul, NULL);
	T_EXPECT_EQ(zigzag_encode(-2), 3ul, NULL);
	T_EXPECT_EQ(zigzag_encode(-3), 5ul, NULL);

	T_EXPECT_EQ(zigzag_encode(INTPTR_MAX), UINTPTR_MAX - 1, NULL);
	T_EXPECT_EQ(zigzag_encode(INTPTR_MAX - 1), UINTPTR_MAX - 3, NULL);
	T_EXPECT_EQ(zigzag_encode(INTPTR_MAX - 2), UINTPTR_MAX - 5, NULL);

	T_EXPECT_EQ(zigzag_encode(INTPTR_MIN), UINTPTR_MAX, NULL);
	T_EXPECT_EQ(zigzag_encode(INTPTR_MIN + 1), UINTPTR_MAX - 2, NULL);
	T_EXPECT_EQ(zigzag_encode(INTPTR_MIN + 2), UINTPTR_MAX - 4, NULL);
}

T_DECL(zigzag_decode, "zigzag_decode")
{
	T_EXPECT_EQ(zigzag_decode(0),  0ul, NULL);
	T_EXPECT_EQ(zigzag_decode(1), -1ul, NULL);
	T_EXPECT_EQ(zigzag_decode(2),  1ul, NULL);
	T_EXPECT_EQ(zigzag_decode(3), -2ul, NULL);
	T_EXPECT_EQ(zigzag_decode(4),  2ul, NULL);

	T_EXPECT_EQ(zigzag_decode(-1),  INTPTR_MIN, NULL);
	T_EXPECT_EQ(zigzag_decode(-2),  INTPTR_MAX, NULL);
	T_EXPECT_EQ(zigzag_decode(-3),  INTPTR_MIN + 1, NULL);
	T_EXPECT_EQ(zigzag_decode(-4),  INTPTR_MAX - 1, NULL);
}

T_DECL(codeoffset_encode_decode, "codeoffset_encode_decode")
{
	#define CO_roundtrip(x) codeoffset_decode(codeoffset_encode(x))

	intptr_t val = (vm_address_t)&trace_collect;
	T_EXPECT_EQ(CO_roundtrip(val), val, "code offset round-trips");
	T_EXPECT_EQ(CO_roundtrip(-val), -val, "negative code offset round-trips");

	arc4random_buf(&val, sizeof(val));
	if (TARGET_CPU_ARM64) val &= ~3;
	T_EXPECT_EQ(CO_roundtrip(val), val, "random code offset round-trips");
}

T_DECL(varint_encode, "varint_encode")
{
	uint8_t buffer[2] = {};

	T_EXPECT_EQ(varint_encode(NULL, 0, 5), 0ul, "zero-sized buffer");

	T_EXPECT_EQ(varint_encode(buffer, 1, 0), 1ul, "encode zero");
	T_EXPECT_EQ(buffer[0], 0x80, "stop bit");

	T_EXPECT_EQ(varint_encode(buffer, 1, 5), 1ul, "encode small value");
	T_EXPECT_EQ(buffer[0], 0x85, "0x5 + stop bit");
	T_EXPECT_EQ(buffer[1], 0, "no write to buffer[1]");

	T_EXPECT_EQ(varint_encode(buffer, 2, 0x105), 2ul, "encode multi-byte value");
	T_EXPECT_EQ(buffer[0], 0x05, "first 7 bits");
	T_EXPECT_EQ(buffer[1], 0x82, "next 7 bits + stop bit");

	buffer[1] = 0;
	T_EXPECT_EQ(varint_encode(buffer, 1, 0x106), 0ul, "buffer too small");
	T_EXPECT_EQ(buffer[0], 0x06, "first 7 bits");
	T_EXPECT_EQ(buffer[1], 0, "no write to buffer[1]");
}

T_DECL(varint_decode, "varint_decode")
{
	uint8_t buffer[2] = {};
	uintptr_t val = 0xAA;

	T_EXPECT_EQ(varint_decode(NULL, 0, NULL), 0ul, "zero-sized buffer");

	buffer[0] = 0x80;
	T_EXPECT_EQ(varint_decode(buffer, 1, &val), 1ul, "decode zero");
	T_EXPECT_EQ(val, 0ul, NULL);

	buffer[0] = 0x85;
	T_EXPECT_EQ(varint_decode(buffer, 1, &val), 1ul, "decode small value");
	T_EXPECT_EQ(val, 5ul, NULL);

	buffer[0] = 0x05;
	buffer[1] = 0x82;
	T_EXPECT_EQ(varint_decode(buffer, 2, &val), 2ul, "decode multi-byte value");
	T_EXPECT_EQ(val, 0x105ul, NULL);

	T_EXPECT_EQ(varint_decode(buffer, 1, NULL), 0ul, "missing stop bit");
}

// varint encoding uses one bit per byte for stop bit
static const uint32_t k_max_bytes_per_uint64 = 8 + 2;

T_DECL(varint_encode_decode, "varint_encode_decode")
{
	uint8_t buffer[k_max_bytes_per_uint64];
	uintptr_t val, decoded_val;

	for (uint32_t i = 0; i < 10; i++) {
		arc4random_buf(&val, sizeof(val));
		size_t encoded_len = varint_encode(buffer, sizeof(buffer), val);
		T_QUIET; T_ASSERT_GT(encoded_len, 0ul, NULL);
		T_QUIET; T_ASSERT_LE(encoded_len, sizeof(buffer), NULL);

		size_t decoded_len = varint_decode(buffer, sizeof(buffer), &decoded_val);
		T_QUIET; T_EXPECT_EQ(decoded_len, encoded_len, NULL);
		T_EXPECT_EQ(decoded_val, val, "64-bit value round-trips");
	}
}

T_DECL(trace_encode, "trace_encode")
{
	uint8_t expected_buffer[10] = {
		0x88, // 0x04 << 1 = 0x08 + stop bit
		0x78, // 0x80 - 0x04 = 0x7c << 1 = 0xf8 -> [0x1, 0x78]
		0x81, // 0x1 + stop bit
		0xbf, // 0x60 - 0x80 = -0x20 << 1 = -0x40 ~ = 0x3f + stop bit
		0xAA
	};
	vm_address_t addrs[] = {0x04, 0x80, 0x60};
	if (TARGET_CPU_ARM64) for (uint32_t i = 0; i < 3; i++) addrs[i] <<= 2;

	uint8_t buffer[10] = {0, 0, 0, 0, 0xAA};
	size_t size = trace_encode(buffer, sizeof(buffer), addrs, 3);
	T_EXPECT_EQ(size, 4ul, NULL);

	int res = memcmp(buffer, expected_buffer, sizeof(buffer));
	T_EXPECT_EQ(res, 0, "encoded addresses");
}

T_DECL(trace_decode, "trace_decode")
{
	uint8_t buffer[10] = {
		0x88, // 0x04 << 1 = 0x08 + stop bit
		0x78, // 0x80 - 0x04 = 0x7c << 1 = 0xf8 -> [0x1, 0x78]
		0x81, // 0x1 + stop bit
		0xbf, // 0x60 - 0x80 = -0x20 << 1 = -0x40 ~ = 0x3f + stop bit
		0xAA
	};
	vm_address_t expected_addrs[] = {0x04, 0x80, 0x60};
	if (TARGET_CPU_ARM64) for (uint32_t i = 0; i < 3; i++) expected_addrs[i] <<= 2;

	vm_address_t addrs[3];
	uint32_t num_addrs = trace_decode(buffer, 4, addrs, 3 + 1/*fudge*/);
	T_EXPECT_EQ(num_addrs, 3, NULL);

	int res = memcmp(addrs, expected_addrs, sizeof(addrs));
	T_EXPECT_EQ(res, 0, "decoded addresses");
}

T_DECL(trace_encode_decode, "trace_encode_decode")
{
	vm_address_t addrs[32];
	uint32_t num_addrs = arc4random_uniform(32);
	arc4random_buf(addrs, num_addrs * sizeof(vm_address_t));
	if (TARGET_CPU_ARM64) for (uint32_t i = 0; i < num_addrs; i++) addrs[i] &= ~3;

	uint8_t buffer[32 * k_max_bytes_per_uint64];
	size_t size = trace_encode(buffer, sizeof(buffer), addrs, num_addrs);
	T_ASSERT_LT(size, sizeof(buffer), NULL);

	vm_address_t decoded_addrs[32];
	uint32_t decoded_num_addrs = trace_decode(buffer, size, decoded_addrs, 32);
	T_EXPECT_EQ(decoded_num_addrs, num_addrs, NULL);

	int res = memcmp(decoded_addrs, addrs, num_addrs * sizeof(vm_address_t));
	T_EXPECT_EQ(res, 0, "addresses round-trips");
}

T_DECL(trace_collect, "trace_collect")
{
	vm_address_t frames[34];
	uint32_t num_frames;
	thread_stack_pcs(frames, 34, &num_frames);
	num_frames -= 2;
	T_ASSERT_GE(num_frames, 3, "at least 3 good frames");

	uint8_t buffer[32 * k_max_bytes_per_uint64];
	size_t size = trace_collect(buffer, sizeof(buffer));
	T_ASSERT_LT(size, sizeof(buffer), NULL);

	vm_address_t decoded_frames[32];
	uint32_t decoded_num_frames = trace_decode(buffer, size, decoded_frames, 32);
	T_EXPECT_EQ(decoded_num_frames, num_frames, NULL);

	uint32_t start = 1; // call sites in this function don't match
	for (uint32_t i = start; i < num_frames; i++) {
		T_EXPECT_EQ(decoded_frames[i], frames[i + 1], "frame %u: %p", i, (void *)frames[i]);
	}
}
