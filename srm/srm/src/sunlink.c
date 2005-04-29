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

static int file;
static off_t file_size;
static unsigned char *buffer;
static u_int32_t buffsize;
static off_t bytes_total;
static off_t bytes_completed;

void flush(int fd) {
  /* force buffered writes to be flushed to disk */
#if HAVE_FDATASYNC
  fdatasync(fd);
#else
  fsync(fd);
#endif

#if __APPLE__
  sync();
  /* if we're root, we can issue an ioctl to flush the device's cache */
  if (getuid() == 0) {
    int err;
    struct statfs sfs;
	if (fstatfs(fd, &sfs) != 0) {
      fprintf(stderr, "\ncannot stat file (%s)\n", strerror(errno));
      fflush(stderr);
    } else {
	  int devfd;
      char *volname;
      char rawdevname[MNAMELEN], *ptr;
      strcpy(rawdevname, sfs.f_mntfromname);
	  ptr = strrchr(rawdevname, '/');
	  if (ptr != NULL) {
	    memmove(ptr+2, ptr+1, strlen(ptr) + 1);
	    ptr[1] = 'r';
      }
      devfd = open(rawdevname, O_RDONLY);
	  if (devfd < 0) {
	    fprintf(stderr, "\ncannot open %s : %s\n",
		  rawdevname, strerror(errno));
        fflush(stderr);
      } else {
        err = ioctl(devfd, DKIOCSYNCHRONIZECACHE, NULL);
		if (err) {
          fprintf(stderr, "\nflushing cache on %s returned: %s\n",
		    sfs.f_mntfromname, strerror(errno));
          fflush(stderr);
        }
        close(devfd);
      }
    }
  }
#endif /* __APPLE__ */
}

void update_progress(u_int32_t bytes_written) {
  u_int32_t cur_percent, new_percent;

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

static void overwrite() {
  u_int32_t i;
  off_t count = 0;

  lseek(file, 0, SEEK_SET);
  while (count < file_size - buffsize) {
    i = write(file, buffer, buffsize);
    if (options & OPT_V) update_progress(i);
	count += i;
  }
  i = write(file, buffer, file_size - count);
  if (options & OPT_V) update_progress(i);
  flush(file);
  lseek(file, 0, SEEK_SET);
}

void overwrite_random(int num_passes) {
  int i;

  for (i = 0; i < num_passes; i++) {
    randomize_buffer(buffer, buffsize);
    overwrite();
  }
}

void overwrite_byte(int byte) {
  memset(buffer, byte, buffsize);
  overwrite();
}

void overwrite_bytes(int byte1, int byte2, int byte3) {
  int i;

  memset(buffer, byte1, buffsize);
  for (i = 1; i < buffsize; i += 3) {
    buffer[i] = byte2;
    buffer[i+1] = byte3;
  }
  overwrite();
}

void overwrite_file() {
  bytes_completed = 0;
  bytes_total = 0;

  if (options & OPT_Z)
    bytes_total = file_size;

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
  if (options & OPT_Z)
    overwrite_byte(0x00);
}

int sunlink(const char *path) {
  struct stat statbuf;
  struct statfs fs_stats;
#if HAVE_LINUX_EXT2_FS_H
  int flags = 0;
#endif
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

  file_size = statbuf.st_size;
  buffsize = statbuf.st_blksize;

  if ( (buffer = (unsigned char *)alloca(buffsize)) == NULL ) {
    errno = ENOMEM;
    return -1;
  }
  
  if ( (file = open(path, O_WRONLY)) == -1) /* BSD doesn't support O_SYNC */
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
      file_size = rsrc_fork_size;

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
