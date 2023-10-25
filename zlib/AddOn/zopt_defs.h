#pragma once

// Dirty way to disable optimizations
//#undef VEC_OPTIMIZE
//#undef INFFAST_OPT

#pragma mark - CRC32 and ADLER32

#ifdef VEC_OPTIMIZE

extern uLong adler32_vec(unsigned int adler, unsigned int sum2, const Bytef* buf, int len);
extern uint32_t crc32_little_aligned_vector(uint32_t crc, const unsigned char *buf, uint32_t len);

#endif

#pragma mark - INFFAST

#ifdef INFFAST_OPT

typedef __attribute__((__ext_vector_type__(16))) uint8_t vector_uchar16;
typedef __attribute__((__ext_vector_type__(16),__aligned__(1))) uint8_t packed_uchar16;
typedef __attribute__((__ext_vector_type__(8),__aligned__(2))) unsigned short packed_ushort8;
typedef __attribute__((__aligned__(1))) uint64_t packed_uint64_t;
typedef __attribute__((__aligned__(1))) uint32_t packed_uint32_t;
typedef __attribute__((__aligned__(1))) uint16_t packed_uint16_t;

// Build Huffman tables. Return 0 on success, 1 on error.
int inffast_tables(z_streamp strm);

#define ASMINF // Disable inffast
#undef  INFLATE_MIN_INPUT
#define INFLATE_MIN_INPUT ((15+15+(15+5)+(15+13)+64+64)/8) // Be safe, use generous upper bound
#undef  INFLATE_MIN_OUTPUT
#define INFLATE_MIN_OUTPUT (2+258+63) // 2 literals, match + excess

/*
 zlib defaults to 9 lenbits and 6 distbits.  Thus, zlib reserves ENOUGH (=1444)
 codes in its state.  We try to use larger Huffman lookup tables.  Usually,
 1444 codes are enough for 10 lenbits and 7 distbits.  If tree construction
 fails, we fallback to the default values.
 */
#define INFLATE_LEN_BITS_OPT 10 // See ENOUGH*, max 1334 entries needed
#define INFLATE_DIST_BITS_OPT 7 // See ENOUGH*, max 400 entries needed

/*
 INFFAST_OPT decodes the next Huffman symbol while processing the current one.
 To step over the current symbol, we need to consume the extra bits of a
 length/distance symbol in one step.  Therefore, we modify the Huffman codes.
 We need to transform the codes back for the classic "wrapper" code to work.
 */
#define INFLATE_ADD_EXTRA_BITS(here)                  \
    {                                                 \
    const int8_t mask = -((here.op >> 4) & 1) & 15;   \
    here.bits += here.op & mask; /* add extra bits */ \
    }
#define INFLATE_SUB_EXTRA_BITS(here)                  \
    {                                                 \
    const int8_t mask = -((here.op >> 4) & 1) & 15;   \
    here.bits -= here.op & mask; /* sub extra bits */ \
    }

#endif
