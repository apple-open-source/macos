/* cache control */

#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <sys/types.h>

static const unsigned int kCacheOptionsSyncForExecution = 0x1;

int
sys_cache_control(unsigned int options, caddr_t start, size_t len)
{
     if (options == kCacheOptionsSyncForExecution) {
	  sys_icache_invalidate(start, len);
	  return 0;
     }
     return ENOTSUP;
}
