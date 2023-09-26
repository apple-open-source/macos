// ZLIB compression/decompression tests
// CM 10/19/2022
#include "ztest_common.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <dispatch/queue.h>
#include <compression_private.h>

#pragma mark - COMMAND LINE OPTIONS

// Defaults for command line options
static int use_fixed   = 0;   // Default to dymanic Huffman
static int use_rfc1950 = 0;   // Use RFC 1950 (defaults to RFC 1951 == raw deflate w/o checksums)
static int use_torture = 0;   // Torture stream API, checksums, truncated decodes w/ inflate/infback API
static int n_its_enc   = 10;  // Number of encoder iterations
static int n_its_dec   = 100; // Number of decoder iterations

#pragma mark - Verify threaded decodes

// Decodes buffer in multiple threads.
void zlib_worker(const uint8_t* enc_buffer, const size_t enc_size,
                 const uint8_t* src_buffer, const size_t src_size,
                 const int use_rfc1950,
                 const int use_fixed,
                 const size_t idx)
{
  // Allocate data
  void* data = calloc(1, src_size);
  if (data == NULL) PFAIL("malloc");

  // Decode and verify
  const size_t decoded = zlib_decode_buffer(data, src_size, (uint8_t*)enc_buffer, enc_size, use_rfc1950);
  if ((decoded != src_size) || (memcmp(data, src_buffer, src_size))) PFAIL("zlib_worker %zu", idx);

  // Free data
  free(data);
}

// Decodes in multiple threads. Triggers decoder bug found here: rdar://102994670
void torture_threads(uint8_t* dst_buffer, size_t dst_size,
                     uint8_t* src_buffer, size_t src_size, int use_rfc1950, int use_fixed)
{
  const size_t n_jobs = (1 << 30) / src_size; // Process 1GB
  
  // Encode using given options
  const size_t enc_size = zlib_encode_buffer(dst_buffer, dst_size,
                                             src_buffer, src_size, 5, use_rfc1950, use_fixed);
  if (enc_size == 0) PFAIL("zlib_encode_buffer");
  
  // Decode with multiple threads
  dispatch_apply(n_jobs, DISPATCH_APPLY_AUTO, ^(size_t idx)
  {
    zlib_worker(dst_buffer, enc_size,
                src_buffer, src_size, use_rfc1950, use_fixed, idx);
  });
}

#pragma mark - Verify CRC-32 / Adler32

// Verify small buffer edge cases
void torture_checksums(void* data, size_t size) // OK
{
  // Clamp buffer size?
  if (size > 0x1000) size = 0x1000;
  
  // Torture Adler32 and Crc32...
  for (size_t i = 0; i <= size; i++)
  {
    if (crc32_z(0, data, i) != simple_crc32(data, i)) PFAIL("CRC32");
    if (adler32_z(1, data, i) != simple_adler32(data, i)) PFAIL("Adler-32");
  }
}

// Make sure, that CRC32 produces expected results by ignoring bad high bits.
static void verify_crc32_undefined_behavior(void)
{
  const Byte data[80] = {0};
  
  // CRC32 algorithm has a scalar and a vector part.
  // - The vector part ignores the 32 upper bits by design.
  // - The scalar part is processed before the vector part, to enforce alignment requirements.
  // - We try different alignments, to trigger both parts of the algorithm.
  for (int i = 0; i < 16; i++)
  {
    if (crc32_z(0x0000000000000000, data + i, 64) !=
        crc32_z(0xffffffff00000000, data + i, 64)) PFAIL("CRC32 undefined behavior");
  }
}

#pragma mark - BENCHMARK

// Benchmark
void process_file(const char* name) // OK
{
  uint8_t* src_buffer = NULL;
  uint8_t* dst_buffer = NULL;
  uint8_t* dec_buffer = NULL;
  size_t src_size = 0;
  size_t dst_size = 0;
  size_t dec_size = 0;
  size_t compressed_zlib = 0;
  size_t compressed_comp = 0;
  uint64_t cycles_crc   = 0;
  uint64_t cycles_adler = 0;
  uint64_t cycles_zlib_enc = 0;
  uint64_t cycles_zlib_dec = 0;
  uint64_t cycles_comp_enc = 0;
  uint64_t cycles_comp_dec = 0;
  uint32_t sum_crc   = 0;
  uint32_t sum_adler = 0;
  struct stat st;
  int fd = -1;

  //---------------------------------------------------------------------------- Setup benchmark
  
  // Open input and query size
  fd = open(name, O_RDONLY, 0);
  if (fd < 0) PFAIL("Can't open %s", name);
  if (fstat(fd, &st) != 0) PFAIL("fstat");
  src_size = st.st_size;
  dst_size = (st.st_size * 9 >> 3) + 258; // Add extra space for already compressed data / small files

  // Allocate buffers
  src_buffer = malloc(src_size);
  dst_buffer = malloc(dst_size);
  dec_buffer = malloc(dst_size); // Don't hint decoder uncompressed size
  if ((src_buffer == NULL) ||
      (dst_buffer == NULL) ||
      (dec_buffer == NULL)) PFAIL("malloc");

  // Read data in GB chunks
  for (size_t k, pos = 0; pos < src_size; pos += k)
  {
    k = 1 << 30;
    if (pos + k > src_size) k = src_size - pos;
    if (k != (size_t)read(fd, src_buffer + pos, k)) PFAIL("read");
  }

  //---------------------------------------------------------------------------- CHECKSUMS
  // Benchmark Crc32
  for (int i = 0; i <= n_its_dec; i++)
  {
    if (i <= 1) cycles_crc = kpc_get_cycles(); // Skip 0 == heat up
    sum_crc = (uint32_t)crc32_z(0, src_buffer, src_size);
  }
  cycles_crc = kpc_get_cycles() - cycles_crc;
  if (sum_crc != simple_crc32(src_buffer, src_size)) PFAIL("CRC32");

  // Benchmark Adler32
  for (int i = 0; i <= n_its_dec; i++)
  {
    if (i <= 1) cycles_adler = kpc_get_cycles(); // Skip 0 == heat up
    sum_adler = (uint32_t)adler32_z(1, src_buffer, src_size);
  }
  cycles_adler = kpc_get_cycles() - cycles_adler;
  if (sum_adler != simple_adler32(src_buffer, src_size)) PFAIL("Adler-32");
  
  //---------------------------------------------------------------------------- ENCODER COMPRESSION
  for (int i = 0; i <= n_its_enc; i++)
  {
    if (i <= 1) cycles_comp_enc = kpc_get_cycles();
    compressed_comp = compression_encode_buffer(dst_buffer, dst_size, src_buffer, src_size, NULL, use_rfc1950 ? COMPRESSION_ZLIB5_RFC1950 : COMPRESSION_ZLIB5);
    if (compressed_comp == 0) PFAIL("compression_encode_buffer");
  }
  cycles_comp_enc = kpc_get_cycles() - cycles_comp_enc;
  
  //---------------------------------------------------------------------------- ENCODER ZLIB
  for (int i = 0; i <= n_its_enc; i++)
  {
    if (i <= 1) cycles_zlib_enc = kpc_get_cycles();
    compressed_zlib = zlib_encode_buffer(dst_buffer, dst_size, src_buffer, src_size, 5, use_rfc1950, use_fixed);
    if (compressed_zlib == 0) PFAIL("zlib_encode_buffer");
  }
  cycles_zlib_enc = kpc_get_cycles() - cycles_zlib_enc;

  //---------------------------------------------------------------------------- DECODER COMPRESSION
  bzero(dec_buffer, dst_size);
  for (int i = 0; i <= n_its_dec; i++)
  {
    if (i <= 1) cycles_comp_dec = kpc_get_cycles();
    dec_size = compression_decode_buffer(dec_buffer, dst_size, dst_buffer, compressed_zlib, NULL, use_rfc1950 ? COMPRESSION_ZLIB_RFC1950 : COMPRESSION_ZLIB);
    if (dec_size != src_size) PFAIL("compression_decode_buffer");
  }
  cycles_comp_dec = kpc_get_cycles() - cycles_comp_dec;
  if (memcmp(src_buffer, dec_buffer, src_size)) PFAIL("data mismatch");
  
  //---------------------------------------------------------------------------- DECODER ZLIB
  bzero(dec_buffer, dst_size);
  for (int i = 0; i <= n_its_dec; i++)
  {
    if (i <= 1) cycles_zlib_dec = kpc_get_cycles();
    dec_size = zlib_decode_buffer(dec_buffer, dst_size, dst_buffer, compressed_zlib, use_rfc1950);
    if (dec_size != src_size) PFAIL("zlib_decode_buffer");
  }
  cycles_zlib_dec = kpc_get_cycles() - cycles_zlib_dec;
  if (memcmp(src_buffer, dec_buffer, src_size)) PFAIL("data mismatch");

  //---------------------------------------------------------------------------- TORTURE
  if (use_torture)
  {
    // Torture checksums
    torture_checksums(src_buffer, src_size);
    
    // Torture stream API
    for (int i = 0; i <= n_its_dec; i++)
    {
      bzero(dec_buffer, src_size);
      dec_size = zlib_decode_torture(dec_buffer, dst_size, dst_buffer, compressed_zlib, use_rfc1950);
      if ((dec_size != src_size) || (memcmp(dec_buffer, src_buffer, src_size))) PFAIL("zlib_decode_torture");
    }

    // Truncated decodes
    for (size_t size = 1;; size += (size + 31) >> 5)
    {
      // Clamp size?
      if (size > src_size) size = src_size;
      
      // Torture inflate API
      bzero(dec_buffer, size);
      dec_size = zlib_decode_buffer(dec_buffer, size, dst_buffer, compressed_zlib, use_rfc1950);
      if ((size != dec_size) || (memcmp(dec_buffer, src_buffer, size))) PFAIL("buffer torture, size=%zu", size);

      // Torture infback API?
      if (!use_rfc1950)
      {
        bzero(dec_buffer, size);
        dec_size = zlib_decode_infback(dec_buffer, size, dst_buffer, compressed_zlib);
        if ((size != dec_size) || (memcmp(dec_buffer, src_buffer, size))) PFAIL("infback torture, size=%zu", size);
      }

      // Done?
      if (size == src_size) break;
    }
    
    // Torture multi threaded decoding with and w/o fixed Huffman profile
    // rdar://104176699
    torture_threads(dst_buffer, dst_size, src_buffer, src_size, use_rfc1950, 0);
    torture_threads(dst_buffer, dst_size, src_buffer, src_size, use_rfc1950, 1);
  }
  
  //----------------------------------------------------------------------------
  // Stats and cleanup
  n_its_dec += (n_its_dec == 0); // Heat up == benchmark
  n_its_enc += (n_its_enc == 0); // Heat up == benchmark
  PLOG("%9zu B -> %9zu/%9zu B |"
       " enc %6.2f/%6.2f c/B |"
       " dec %5.2f/%5.2f c/B |"
       " CRC32/Adler-32 %0.2f/%0.2f c/B %s| %s",
       src_size, compressed_zlib, compressed_comp,
       (float)cycles_zlib_enc / (n_its_enc * src_size),
       (float)cycles_comp_enc / (n_its_enc * src_size),
       (float)cycles_zlib_dec / (n_its_dec * src_size),
       (float)cycles_comp_dec / (n_its_dec * src_size),
       (float)cycles_crc      / (n_its_dec * src_size),
       (float)cycles_adler    / (n_its_dec * src_size),
       use_torture ? "| torture OK " : "",
       name);
  
  // Free buffers and close file
  free(src_buffer);
  free(dst_buffer);
  free(dec_buffer);
  close(fd);
}

int show_help(const char* name) // OK
{
  PLOG("SYNTAX\n"
       "\t%s [options] <file1> ... <fileN>\n"
       "OPTIONS\n"
       "\t-f\tUse fixed Huffman\n"
       "\t-r\tUse RFC1950 (instead of RFC1951) and disable infback API\n"
       "\t-t\tTorture truncated decodes (both APIs), streaming, CRC-32, Adler32 and threaded decoding\n"
       "\t-e N\tTime N encoder iterations (default=%u)\n"
       "\t-d N\tTime N decoder iterations (default=%u)\n"
       , name, n_its_enc, n_its_dec);
  exit(1);
}

int main(int argc, const char** argv)
{
  // Show syntax?
  if (argc == 1) show_help(argv[0]);
  
  // Check CRC32
  verify_crc32_undefined_behavior();
  
  // Iterate over files...
  kpc_cycles_init();
  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i],"-h") == 0) show_help(argv[0]);
    if (strcmp(argv[i],"-f") == 0) { use_fixed   = 1; continue; }
    if (strcmp(argv[i],"-r") == 0) { use_rfc1950 = 1; continue; }
    if (strcmp(argv[i],"-t") == 0) { use_torture = 1; continue; }
    if ((strcmp(argv[i],"-e") == 0) && (i + 1 < argc)) { n_its_enc = atoi(argv[++i]); continue; }
    if ((strcmp(argv[i],"-d") == 0) && (i + 1 < argc)) { n_its_dec = atoi(argv[++i]); continue; }
    process_file(argv[i]);
  }
  
  return 0;
}
