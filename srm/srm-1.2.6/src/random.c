#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "srm.h"

int urand_file = -1;

void init_random(const unsigned int seed) {
  struct stat statbuf;

  if (stat("/dev/urandom", &statbuf) == 0 && S_ISCHR(statbuf.st_mode)) {
    urand_file = open("/dev/urandom", O_RDONLY);
  } else {
    srand(seed);
  }
}

char random_char(void) {
  char buf[3];

  if (urand_file != -1) {
    read(urand_file, &buf, 1);
    return buf[0];
  }
  return rand();
}

void randomize_buffer(unsigned char *buffer, int length) {
  int i;

  if (urand_file != -1) {
    read(urand_file, buffer, length);
  } else {
    for (i = 0; i < length; i++)
      buffer[i] = rand();
  }
}
