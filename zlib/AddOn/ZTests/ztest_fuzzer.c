#include "zlib.h"
#include "ztest_common.h"

// Ref. https://confluence.sd.apple.com/display/DT/Fuzz+Testing+with+LibFuzzer

int LLVMFuzzerTestOneInput(const uint8_t* buffer, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* buffer, size_t size)
{
  // Allocate buffer
  const size_t dst_size = 1 << 20;
  uint8_t* dst_buffer = malloc(dst_size);
  if (dst_buffer == NULL) return -1; // Malloc failed

  // From time to time, show that we are still working
  if ((rand() & 0xfff) == 0) printf(" -> alive\n");
  
  // Decode
  zlib_decode_torture(dst_buffer, dst_size, (uint8_t*)buffer, size, 0);

  // Free buffer
  free(dst_buffer);
  return 0;
}
