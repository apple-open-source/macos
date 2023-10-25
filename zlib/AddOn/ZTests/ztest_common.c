// ZLIB tests tools
// CM 2022/10/18
#include "ztest_common.h"

#pragma mark - BENCHMARKING

#include <kperf/kpc.h>

static int kKPC_OK = 0; // Did we init properly?

// Init counters. Return 0 on success, -1 on failure.
int kpc_cycles_init(void) // OK
{
  if (kpc_force_all_ctrs_set(1)) { perror("kpc_force_all_ctrs_set"); return -1; }
  
  // Enable only fixed counters
  if (kpc_set_counting(KPC_CLASS_FIXED_MASK)) { perror("kpc_set_counting"); return -1; }
  if (kpc_set_thread_counting(KPC_CLASS_FIXED_MASK)) { perror("kpc_set_thread_counting"); return -1; }
  
  kKPC_OK = 1; // OK
  return 0;
}

// Get cycles
/*
 ARM
 PMC0 is always CORE_CYCLE
 PMC1 is always number of retired instructions
 
 INTEL
 0 is retired instructions
 1 is cycles
 2 is TSC
 */
uint64_t kpc_get_cycles(void) // OK
{
  static uint64_t fallback = 0;
  uint64_t counters[32];
  
  if (!kKPC_OK) return fallback++; // Init failed, return pseudo value
  
  kpc_get_thread_counters(0, 32, counters);
#if defined(__x86_64__) || defined(__i386__)
  return counters[1]; // intel
#else
  return counters[0]; // arm
#endif
}

#pragma mark - BUFFER API

// Encode buffer using zlib. Return number of compressed bytes, 0 on failure.
size_t zlib_encode_buffer(uint8_t* dst_buffer, size_t dst_size,
                          uint8_t* src_buffer, size_t src_size, int level, int rfc1950, int fixed) // OK
{
  z_stream z;
  int ok = 1;
  
  // Handle edge cases
  if (dst_size > UINT32_MAX) dst_size = UINT32_MAX;
  if (src_size > UINT32_MAX) return 0;
  
  // Setup stream
  bzero(&z, sizeof(z_stream));
  z.avail_in  = (uint32_t)src_size;
  z.avail_out = (uint32_t)dst_size;
  z.next_in  = src_buffer;
  z.next_out = dst_buffer;
  
  // Encode buffer
  if (deflateInit2(&z, level, Z_DEFLATED,
                   rfc1950 ? 15 : -15, 8,
                   fixed ? Z_FIXED : Z_DEFAULT_STRATEGY) != Z_OK) return 0; // Failed
  if (deflate(&z, Z_FINISH) != Z_STREAM_END) ok = 0;
  if (deflateEnd(&z) != Z_OK) ok = 0;
  
  return ok ? z.total_out : 0;
}

// Decode buffer using zlib. Return number of uncompressed bytes, 0 on failure.
// Supports truncated decodes.
size_t zlib_decode_buffer(uint8_t* dst_buffer, size_t dst_size,
                          uint8_t* src_buffer, size_t src_size, int rfc1950) // OK
{
  z_stream z;
  int ok = 1;
  int status;
  
  // Handle edge cases
  if (dst_size > UINT32_MAX) dst_size = UINT32_MAX;
  if (src_size > UINT32_MAX) return 0;
  
  // Setup stream
  bzero(&z, sizeof(z));
  z.avail_in  = (uint32_t)src_size;
  z.avail_out = (uint32_t)dst_size;
  z.next_in  = src_buffer;
  z.next_out = dst_buffer;
  
  // Decode buffer
  if (inflateInit2(&z, rfc1950 ? 15: -15) != Z_OK) return 0; // Failed
  status = inflate(&z, Z_FINISH);
  
  // No success (EoS or partial decode)?
  if (!   ((status == Z_STREAM_END) ||
           ((status == Z_BUF_ERROR) && (z.total_out == dst_size)))) ok = 0;
  
  // Cleanup and return
  if (inflateEnd(&z) != Z_OK) ok = 0;
  return ok ? z.total_out : 0;
}

// Decode buffer using zlib. Torture streaming API. Return number of uncompressed bytes, 0 on failure.
size_t zlib_decode_torture(uint8_t* dst_buffer, size_t dst_size,
                           uint8_t* src_buffer, size_t src_size, int rfc1950) // OK
{
  const size_t tmp_size = 1000;
  uint8_t tmp0[tmp_size];
  uint8_t tmp1[tmp_size];
  z_stream z;
  int ok = 1;
  int status;
  
  // Handle edge cases
  if (dst_size > UINT32_MAX) dst_size = UINT32_MAX;
  if (src_size > UINT32_MAX) return 0;
  
  // Setup stream
  bzero(&z, sizeof(z));
  status = inflateInit2(&z, rfc1950 ? 15: -15);
  if (status != Z_OK) return 0; // Failed
  
  // Decode step by step
  for (;;)
  {
    // Use small random input/output buffers
    z.avail_in  = (uint32_t)(src_size - z.total_in);
    z.avail_out = (uint32_t)(dst_size - z.total_out);
    z.avail_in  %= (rand() % tmp_size) + 1;
    z.avail_out %= (rand() % tmp_size) + 1;
    z.next_in  = rand() & 1 ? tmp0 : tmp0 + tmp_size - z.avail_in;  // Randomly align the start/end
    z.next_out = rand() & 1 ? tmp1 : tmp1 + tmp_size - z.avail_out; // Randomly align the start/end
    
    // Copy input to small buffer
    memcpy(z.next_in, src_buffer + z.total_in, z.avail_in);

    // Decode
    size_t n_written = z.total_out;
    status = inflate(&z, Z_NO_FLUSH);
    n_written = z.total_out - n_written;

    // Copy small buffer to output
    if (n_written)
    {
      memcpy(dst_buffer + z.total_out - n_written, z.next_out - n_written, n_written);
    }
    
    // Continue?
    if (status == Z_OK) continue;
    
    // Are we done?
    if (status == Z_STREAM_END) break;
    
    // One more try?
    if ((status == Z_BUF_ERROR) &&
        ((src_size - z.total_in > 0) || (dst_size - z.total_out >= 258))) continue;
    
    // Something wrong!
    {
      PLOG("status = %d, msg = %s\n", status, z.msg);
      ok = 0;
      break;
    }
  }
  if (inflateEnd(&z) != Z_OK) ok = 0;
  
  return ok ? z.total_out : 0;
}

#pragma mark - INFBACK API

typedef struct
{
  uint8_t* src_buffer;
  uint8_t* dst_buffer;
  size_t src_avail;
  size_t dst_avail;
} s_infback;

// Callback to read data. Return number of bytes available.
static unsigned infback_read(void* arg, z_const unsigned char** data) // OK
{
  s_infback* s = arg;
  size_t k = s->src_avail;

  // Setup data
  *data = s->src_buffer;
  
  // Update state
  if (k > 0x400) k = 0x400; // Clamp to 1K
  s->src_buffer += k;
  s->src_avail -= k;
  
  return (unsigned)k;
}

// Callback to write data. Return on success.
static int infback_write(void* arg, unsigned char* data, unsigned size) // OK
{
  s_infback* s = arg;
  int ok = 1;

  // Truncate write?
  if (size > s->dst_avail) { ok = 0; size = (unsigned)s->dst_avail; }
  
  // Copy data and update state
  bcopy(data, s->dst_buffer, size);
  s->dst_buffer += size;
  s->dst_avail -= size;
  
  return ok ? 0 : -1;
}

// Decode buffer using zlib using infback interface. Return number of uncompressed bytes, 0 on failure.
// Supports truncated decodes.
size_t zlib_decode_infback(uint8_t* dst_buffer, size_t dst_size,
                           uint8_t* src_buffer, size_t src_size) // OK
{
  z_stream z;
  s_infback s;
  uint8_t* window = NULL;
  int ok = 1;
  int status;
  
  // Allocate window
  window = malloc(1 << 15);
  if (window == NULL) { ok = 0; goto END; }
  
  // Setup state
  bzero(&z, sizeof(z));
  if (inflateBackInit(&z, 15, window) != Z_OK) { ok = 0; goto END; }
  
  // Call inflateBack
  s.dst_buffer = dst_buffer; s.dst_avail = dst_size;
  s.src_buffer = src_buffer; s.src_avail = src_size;
  status = inflateBack(&z, infback_read, &s, infback_write, &s);

  // Not successful or truncated decode?
  if (!(status == Z_STREAM_END) &&
      !((status == Z_BUF_ERROR) && (s.dst_avail == 0))) ok = 0;
  
  // Free state
  if (inflateBackEnd(&z) != Z_OK) ok = 0;
  
END:
  free(window);
  return ok ? dst_size - s.dst_avail : 0;
}

#pragma mark - CHECKSUMS

// Return Crc32 of DATA[LEN]. Naive implementation.
uint32_t simple_crc32(uint8_t* src_buffer, const size_t src_size) // OK
{
  uint32_t r = 0xffffffff;
  
  for (size_t i = 0; i < src_size; i++)
  {
    r ^= src_buffer[i];
    for (int j = 0; j < 8; j++)
    {
      r = (r >> 1) ^ (-(r & 1) & 0xedb88320);
    }
  }
  
  return r ^ 0xffffffff;
}

// Return Adler32 of DATA[LEN]. Naive implementation.
uint32_t simple_adler32(const unsigned char* src, const size_t src_size) // OK
{
  uint32_t a = 1, b = 0;
  
  // Process each byte of the data in order
  for (size_t i = 0; i < src_size; i++)
  {
    a = (a + src[i]) % 65521;
    b = (b + a) % 65521;
  }
  
  return (b << 16) | a;
}
