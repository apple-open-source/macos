#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include "srm.h"

/* reseed arc4random after reading this many bytes of random data */
#define RESEED_BYTES    (512*1024*1024)

static int urand_file = -1;
static off_t random_bytes_read = 0;

void seed_random(void) {
#ifdef USE_ARC4RANDOM
  arc4random_stir();
#else
  unsigned int rseed;
  struct timeval tv;
  struct timezone tz;

  if (urand_file != -1) {
    read(urand_file, &rseed, sizeof(rseed));
  } else {
    rseed = rand();
  }
  (void)gettimeofday(&tv, &tz);
  rseed ^= tv.tv_sec + tv.tv_usec + getpid();
  srand(rseed);
#endif
}

void init_random(const unsigned int seed) {
#ifdef USE_ARC4RANDOM
  arc4random_addrandom((unsigned char *)&seed, sizeof(seed));
#else
  struct stat statbuf;

  if (stat("/dev/urandom", &statbuf) == 0 && S_ISCHR(statbuf.st_mode)) {
    urand_file = open("/dev/urandom", O_RDONLY);
  } else {
    srand(seed);
  }
  seed_random();
#endif
}

char random_char(void) {
#ifdef USE_ARC4RANDOM
  random_bytes_read += 4;
  return arc4random();
#else
  char buf[4];

  if (urand_file != -1) {
    read(urand_file, &buf, 1);
    return buf[0];
  }
  return rand();
#endif
}

void randomize_buffer(unsigned char *buffer, unsigned int length) {
  unsigned int i;

#ifdef USE_ARC4RANDOM
  u_int32_t *p = (u_int32_t *)buffer;
  u_int32_t mod4length = length - (length % 4);

  for (i = 0; i < mod4length; i += 4) {
    *p++ = arc4random();
  }
  while (i < length) {
    buffer[i++] = arc4random();
  }
  random_bytes_read += (mod4length + ((length - mod4length) * 4));
  if (random_bytes_read > RESEED_BYTES) {
    random_bytes_read = 0;
    seed_random();
  }
#else
  if (urand_file != -1) {
    read(urand_file, buffer, length);
  } else {
    for (i = 0; i < length; i++)
      buffer[i] = rand();
  }
#endif
}
