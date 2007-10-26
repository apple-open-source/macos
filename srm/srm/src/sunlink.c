#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#if HAVE_LINUX_EXT2_FS_H
#include <linux/ext2_fs.h>
#endif

#if HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif

#if HAVE_SYS_PARAM_H && HAVE_SYS_MOUNT_H
#include <sys/param.h>
#include <sys/mount.h>
#endif

#if __APPLE__
#include <sys/disk.h>
#endif

#if HAVE_CHFLAGS
/* define unsupported flags as 0 */
# if !defined UF_IMMUTABLE
#  define UF_IMMUTABLE 0
# endif
# if !defined UF_APPEND
#  define UF_APPEND 0
# endif
# if !defined UF_NOUNLINK
#  define UF_NOUNLINK 0
# endif
# if !defined SF_IMMUTABLE
#  define SF_IMMUTABLE 0
# endif
# if !defined SF_APPEND
#  define SF_APPEND 0
# endif
# if !defined SF_NOUNLINK
#  define SF_NOUNLINK 0
# endif
#endif

#include "srm.h"

enum {
  W_SINGLE  = 0,
  W_RANDOM  = 1,
  W_TRIPLE  = 2
};

static int file;
static off_t file_size;
static unsigned char *buffer = NULL;
static unsigned char *verify_buffer = NULL;
static u_int32_t buffsize, allocated_buffsize = 0;
static off_t bytes_total;
static off_t bytes_completed;

int init_write_buffer(struct stat *statbuf, struct statfs *fs_stats) {
  u_int64_t maxbytecount;
  u_int32_t tmp_buffsize;

  file_size = statbuf->st_size;
  buffsize = statbuf->st_blksize;

#if HAVE_SYS_PARAM_H
  /* try to determine an optimal write buffer size */
  buffsize = (u_int32_t)(statbuf->st_size / statbuf->st_blksize) * statbuf->st_blksize;
  if ((statbuf->st_size % statbuf->st_blksize) != 0) {
    /* add full size of last block */
    buffsize += statbuf->st_blksize;
  } else if (buffsize < statbuf->st_blksize) {
    /* no smaller than one device block */
    buffsize = statbuf->st_blksize;
  }
  tmp_buffsize = MAXBSIZE;
  if (buffsize > tmp_buffsize) {
    /* no larger than the largest file system buffer size */
    buffsize = tmp_buffsize;
  }
#endif

  if (opt_buffsize) {
    /* allow command-line override */
    buffsize = opt_buffsize;
  }

  /* Allocated buffer must be at least 2 bytes larger than logical buffsize.
     This lets us align repeating 3-byte patterns across multiple buffer
     writes by using a variable offset (0..2) from the start of the buffer. */

  tmp_buffsize = buffsize + 4;

  if (buffer) {
    if (tmp_buffsize > allocated_buffsize) {
      free(buffer);
      buffer = NULL;
    } else {
      return 0; /* use existing buffer */
    }
  }
  if ((buffer = (unsigned char *)malloc(tmp_buffsize)) == NULL) {
    errno = ENOMEM;
    return -1;
  }
  if (options & OPT_VERIFY) {
    if ((verify_buffer = (unsigned char *)malloc(buffsize)) == NULL) {
      errno = ENOMEM;
      return -1;
    }
  }
  allocated_buffsize = tmp_buffsize;
  return 0;
}

void flush(int fd) {
  /* force buffered writes to be flushed to disk */
#if defined F_FULLFSYNC
  /* F_FULLFSYNC is equivalent to fsync plus device flush to media */
  if (fcntl(fd, F_FULLFSYNC, NULL) != 0) {
    /* we're not on a fs that supports this; fall back to plain fsync */
    fsync(fd);
  }
#elif HAVE_FDATASYNC
  fdatasync(fd);
#else
  fsync(fd);
#endif
}

void update_progress(u_int32_t bytes_written) {
  u_int32_t cur_percent, new_percent;

  if (!bytes_total)
    return;

  cur_percent = (u_int32_t)((off_t)(bytes_completed*100)/bytes_total);
  bytes_completed += bytes_written;
  new_percent = (u_int32_t)((off_t)(bytes_completed*100)/bytes_total);

  if (cur_percent != new_percent) {
    printf("\r%d%%", new_percent);
    if (bytes_completed == bytes_total)
      printf("\rdone\n");
    fflush(stdout);
  }
}

unsigned char *align_buffer(unsigned char *buf, off_t pos) {
  /* return a pointer to the start of the buffer which should be written,
     offset from the given buffer by 0, 1, or 2 bytes, so that the 3-byte
     pattern which the buffer contains is aligned with the previous write. */
  return (unsigned char *)((uintptr_t)buf + (unsigned int)(pos % 3));
}

void verification_failure(off_t count) {
  if (sizeof(off_t) == 4)
    printf("warning: failed to verify write at offset %d\n", count);
  else if (sizeof(off_t) == 8)
    printf("warning: failed to verify write at offset %lld\n", count);
  else
    printf("warning: previous write failed to verify!\n");
  fflush(stdout);
}

void overwrite(int stage) {
  u_int32_t i, j;
  off_t count = 0;
  unsigned char *buffptr = buffer;

  lseek(file, 0, SEEK_SET);
  while (count < file_size - buffsize) {
    if (stage == W_RANDOM) {
      randomize_buffer(buffer, buffsize);
    } else if (stage == W_TRIPLE) {
      buffptr = align_buffer(buffer, count);
    }
    i = write(file, buffptr, buffsize);
    if (options & OPT_VERIFY) {
        /* verify the write */
        lseek(file, count, SEEK_SET);
        j = read(file, verify_buffer, buffsize);
        if (!(i == j && !memcmp(verify_buffer, buffptr, buffsize))) {
          verification_failure(count);
        }
    }
    if (options & OPT_V) {
      update_progress(i);
    }
	count += i;
  }
  if (stage == W_RANDOM) {
    randomize_buffer(buffer, file_size - count);
  } else if (stage == W_TRIPLE) {
    buffptr = align_buffer(buffer, count);
  }
  i = write(file, buffptr, file_size - count);
  if (options & OPT_VERIFY) {
    /* verify the write */
    lseek(file, count, SEEK_SET);
    j = read(file, verify_buffer, file_size - count);
    if (!(i == j && !memcmp(verify_buffer, buffptr, file_size - count))) {
      verification_failure(count);
    }
  }
  if (options & OPT_V) {
    update_progress(i);
  }
  flush(file);
  lseek(file, 0, SEEK_SET);
}

void overwrite_random(int num_passes) {
  int i;

  for (i = 0; i < num_passes; i++) {
    overwrite(W_RANDOM);
  }
}

void overwrite_byte(int byte) {
  memset(buffer, byte, buffsize);
  overwrite(W_SINGLE);
}

void overwrite_bytes(unsigned int byte1, unsigned int byte2, unsigned int byte3) {
  u_int32_t val[3], *p = (u_int32_t *)buffer;
  unsigned int i, mod12buffsize = allocated_buffsize - (allocated_buffsize % 12);
  
  val[0] = (byte1 << 24) | (byte2 << 16) | (byte3 << 8) | byte1;
  val[1] = (byte2 << 24) | (byte3 << 16) | (byte1 << 8) | byte2;
  val[2] = (byte3 << 24) | (byte1 << 16) | (byte2 << 8) | byte3;

  /* fill buffer 12 bytes at a time, optimized for 4-byte alignment */
  for (i = 0; i < mod12buffsize; i += 12) {
    *p++ = val[0];
    *p++ = val[1];
    *p++ = val[2];
  }
  while (i < allocated_buffsize) {
    buffer[i] = ((unsigned char *)&val[0])[i % 3];
    i++;
  }
  overwrite(W_TRIPLE);
}

void overwrite_file() {
  bytes_completed = 0;
  bytes_total = 0;
  
  if (!file_size) {
    /* nothing to overwrite in a zero-length file */
    if (options & OPT_V) {
      printf("\rdone\n");
      fflush(stdout);
    }
    return;
  }

  if (options & OPT_ZERO) {
    bytes_total = file_size;
  }
  if (seclevel==1) {
    /* simple one-pass overwrite */
    bytes_total += file_size*1;
    overwrite_random(1);
  } else if (seclevel==7) {
    /* DoD-compliant 7-pass overwrite */
    bytes_total += file_size*7;
    overwrite_byte(0xF6);
    overwrite_byte(0x00);
    overwrite_byte(0xFF);
    overwrite_random(1);
    overwrite_byte(0x00);
    overwrite_byte(0xFF);
    overwrite_random(1);
  } else {
    /* Gutmann 35-pass overwrite */
    bytes_total += file_size*35;
    overwrite_random(4);
    overwrite_byte(0x55);
    overwrite_byte(0xAA);
    overwrite_bytes(0x92, 0x49, 0x24);
    overwrite_bytes(0x49, 0x24, 0x92);
    overwrite_bytes(0x24, 0x92, 0x49);
    overwrite_byte(0x00);
    overwrite_byte(0x11);
    overwrite_byte(0x22);
    overwrite_byte(0x33);
    overwrite_byte(0x44);
    overwrite_byte(0x55);
    overwrite_byte(0x66);
    overwrite_byte(0x77);
    overwrite_byte(0x88);
    overwrite_byte(0x99);
    overwrite_byte(0xAA);
    overwrite_byte(0xBB);
    overwrite_byte(0xCC);
    overwrite_byte(0xDD);
    overwrite_byte(0xEE);
    overwrite_byte(0xFF);
    overwrite_bytes(0x92, 0x49, 0x24);
    overwrite_bytes(0x49, 0x24, 0x92);
    overwrite_bytes(0x24, 0x92, 0x49);
    overwrite_bytes(0x6D, 0xB6, 0xDB);
    overwrite_bytes(0xB6, 0xDB, 0x6D);
    overwrite_bytes(0xDB, 0x6D, 0xB6);
    overwrite_random(4);
  }
  if (options & OPT_ZERO) {
    overwrite_byte(0x00);
  }
}

int sunlink(const char *path) {
  struct stat statbuf;
  struct statfs fs_stats;
#if HAVE_LINUX_EXT2_FS_H
  int flags = 0;
#endif
  int fmode = (options & OPT_VERIFY) ? O_RDWR : O_WRONLY;
  struct flock flock;

  if (lstat(path, &statbuf) == -1) 
    return -1;
  if (!S_ISREG(statbuf.st_mode))
    return rename_unlink(path);

  if (statbuf.st_nlink > 1) {
    rename_unlink(path);
    errno = EMLINK;
    return -1;
  }

  if ( (file = open(path, fmode)) == -1) /* BSD doesn't support O_SYNC */
    return -1;

  if (fcntl(file, F_WRLCK, &flock) == -1) {
    close(file);
    return -1;
  }

  if (fstatfs(file, &fs_stats) == -1 && errno != ENOSYS) {
    close(file);
    return -1;
  }

  /* warn when trying to overwrite files on a non-local fs,
     since there are no guarantees that writes will not be
     buffered on the server, or will overwrite the same spot. */
  if (options & OPT_V) {
    if ((fs_stats.f_flags & MNT_LOCAL) == 0) {
      printf("warning: %s is not on a local filesystem!\n", path);
      fflush(stdout);
    }
  }

#if HAVE_LINUX_EXT2_FS_H
  if (fs_stats.f_type == EXT2_SUPER_MAGIC) 
    if (ioctl(file, EXT2_IOC_GETFLAGS, &flags) == -1) {
      close(file);
      return -1;
    } 

  if ( (flags & EXT2_UNRM_FL) || (flags & EXT2_IMMUTABLE_FL) ||
      (flags & EXT2_APPEND_FL) )  
    {
      close(file);
      errno = EPERM;
      return -1;
    }

#endif /* HAVE_LINUX_EXT2_FS_H */

/* chflags(2) turns out to be a different system call in every BSD
   derivative. The important thing is to make sure we'll be able to
   unlink it after we're through messing around. Unlinking it first
   would remove the need for any of these checks, but would leave the
   user with no way to overwrite the file if the process was
   interrupted during the overwriting. So, instead we assume that the
   open() above will fail on immutable and append-only files and try
   and catch only platforms supporting NOUNLINK here.

   FreeBSD - supports NOUNLINK (from 4.4 on?)
   MacOS X - doesn't support NOUNLINK (as of 10.3.5)
   OpenBSD - doesn't support NOUNLINK (as of 3.1)
   Tru64   - unknown
   
   Note: unsupported flags are defined as 0 at the top of this file,
   so a specific platform check is not required here.
*/

#if HAVE_CHFLAGS
  if ((statbuf.st_flags & UF_IMMUTABLE) || 
      (statbuf.st_flags & UF_APPEND) ||
      (statbuf.st_flags & UF_NOUNLINK) || 
      (statbuf.st_flags & SF_IMMUTABLE) ||
      (statbuf.st_flags & SF_APPEND) ||
      (statbuf.st_flags & SF_NOUNLINK)) 
    {
      close(file);
      errno = EPERM;
      return -1;
    }
#endif /* HAVE_CHFLAGS */

  if (init_write_buffer(&statbuf, &fs_stats) == -1) {
    close(file);
    return -1;
  }
#if defined F_NOCACHE
  /* before performing file I/O, set F_NOCACHE to prevent caching */
  (void)fcntl(file, F_NOCACHE, 1);
#endif

  overwrite_file();

#if HAVE_LINUX_EXT2_FS_H
  ioctl(file, EXT2_IOC_SETFLAGS, EXT2_SECRM_FL);
#endif

  if ((options & OPT_N) == 0) {
    if (ftruncate(file, 0) == -1) {
      close(file);
      return -1;
    }
  }

  close(file);

#if __APPLE__
  /* Also overwrite the file's resource fork, if present. */
  {
    static const char *RSRCFORKSPEC = "/..namedfork/rsrc";
    size_t rsrc_fork_size;
    size_t rsrc_path_size = strlen(path) + strlen(RSRCFORKSPEC) + 1;
    char *rsrc_path = (char *)alloca(rsrc_path_size);
    if (rsrc_path == NULL) {
      errno = ENOMEM;
      return -1;
    }
    if (snprintf(rsrc_path, MAXPATHLEN,
        "%s%s", path, RSRCFORKSPEC ) > MAXPATHLEN - 1) {
      errno = ENAMETOOLONG;
      return -1;
    }

    if (lstat(rsrc_path, &statbuf) != 0) {
      int err = errno;
      if (err == ENOENT || err == ENOTDIR) {
        rsrc_fork_size = 0;
      } else {
        return -1;
      }
    } else {
      rsrc_fork_size = statbuf.st_size;
    }

    if (rsrc_fork_size > 0) {

      if ((file = open(rsrc_path, O_WRONLY)) == -1) {
        return -1;
      }
      if (fcntl(file, F_WRLCK, &flock) == -1) {
        close(file);
        return -1;
      }

      if (options & OPT_V) {
        printf("removing %s\n", rsrc_path);
        fflush(stdout);
      }

      if (init_write_buffer(&statbuf, &fs_stats) == -1) {
        close(file);
        return -1;
      }
    #if defined F_NOCACHE
      /* before performing file I/O, set F_NOCACHE to prevent caching */
      (void)fcntl(file, F_NOCACHE, 1);
    #endif

      overwrite_file();

      if ((options & OPT_N) == 0) {
        if (ftruncate(file, 0) == -1) {
          close(file);
          return -1;
        }
      }
      close(file);
    }
  }
#endif /* __APPLE__ */

  if (options & OPT_N)
    return 0;

  return rename_unlink(path);
}
