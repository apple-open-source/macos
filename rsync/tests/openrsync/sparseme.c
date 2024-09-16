#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mount.h>
#ifdef __linux__
#include <sys/statfs.h>
#include <sys/vfs.h>
#endif

int main (int argc, char *argv[])
{
  int fd;
  char buf[4096] = "meh";
  long long size = 16 * 1024 * 1024;
  char *buf2;
  char *mapped;
  struct stat status;
  struct statfs statusfs;
  const int blocksize = 512;

  if (argc != 2) {
    fprintf(stderr, "usage: different\n");
    exit(1);
  }

  buf2 = malloc(size);

  if (unlink("sparsefile") == -1) {
    if (errno != 2) { // 'no such file or directory' is ok
      perror("unlick");
      printf("errno = %d\n", errno);
      exit(2);
    }
  }
  if ((fd = open("sparsefile", O_RDWR|O_CREAT, 0666)) == -1) {
    perror("open write");
    exit(2);
  }
  if (lseek(fd, size, SEEK_SET) == -1) {
    perror("lseek");
    exit(2);
  }
  if (write(fd, buf, sizeof(buf)) == -1) {
    perror("write");
    exit(2);
  }

  /* Option 'w' - write(2) a block of zeros into the null area */

  if (strchr(argv[1], 'w')) {
    if (lseek(fd, 0, SEEK_SET) == -1) {
      perror("lseek 2");
      exit(2);
    }
    bzero(buf2, size);
    if (write(fd, buf2, size) == -1) {
      perror("write 2");
      exit(2);
    }
  }

  /* Option 'm' - write zeros via mmap into the null area */

  if (strchr(argv[1], 'm')) {
    mapped = mmap(0, size, PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
      perror("mmap");
      exit(2);
    }
    for (long long i = 0; i < size; i++) {
      mapped[i] = 0;
    }
  }
  if (close(fd) == -1) {
    perror("close");
  }

  if (stat("sparsefile", &status) == -1) {
    perror("stat");
    exit(2);
  }

  if (statfs("sparsefile", &statusfs) == -1) {
    perror("statfs");
    exit(2);
  }
  printf("%lld bytes %lld blocks theory %lld blocks actual\n", 
	 (long long)status.st_size, 
	 (long long)status.st_size / (long long)blocksize, 
	 (long long)status.st_blocks);

  if (!strchr(argv[1], 'l')) {
    if (unlink("sparsefile") == -1) {
      perror("unlink");
      printf("errno = %d\n", errno);
      exit(2);
    }
  }

  free(buf2);
  return 0;
}
