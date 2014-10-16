#include "extattr_os.h"

#ifdef EXTATTR_SOLARIS

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "flags.h"

static const mode_t ATTRMODE = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP;

static int
writexattr (const int attrfd,
	    const char *attrvalue,
	    const size_t slen)
{
  int ok = 1;

  if (ftruncate(attrfd, 0) == -1)
    ok = 0;
  if (ok && (write(attrfd, attrvalue, slen) != slen))
    ok = 0;

  return ok ? 0 : -errno;
}

static int
readclose (const int attrfd,
	   void *attrvalue,
	   const size_t slen)
{
  int sz = 0;
  int saved_errno = 0;
  int ok = 1;

  if (attrfd == -1)
    ok = 0;

  if (ok)
  {
    if (slen)
    {
      sz = read(attrfd, attrvalue, slen);
    }
    else
    {
      /* Request to see how much data is there. */
      struct stat sbuf;

      if (fstat(attrfd, &sbuf) == 0)
	sz = sbuf.st_size;
      else
	sz = -1;
    }

    if (sz == -1)
      ok = 0;
  }

  if (!ok)
    saved_errno = errno;
  if ((attrfd >= 0) && (close(attrfd) == -1) && !saved_errno)
    saved_errno = errno;
  if (saved_errno)
    errno = saved_errno;

  return ok ? sz : -errno;
}

static int
unlinkclose (const int attrdirfd, const char *attrname)
{
  int sz = 0;
  int saved_errno = 0;
  int ok = 1;

  if (attrdirfd == -1)
    ok = 0;

  if (ok && (unlinkat(attrdirfd, attrname, 0) == -1))
    ok = 0;

  if (!ok)
    saved_errno = errno;
  if ((attrdirfd >= 0) && (close(attrdirfd) == -1) && !saved_errno)
    saved_errno = errno;
  if (saved_errno)
    errno = saved_errno;

  return ok ? sz : -errno; 
}

static ssize_t
listclose (const int attrdirfd, char *buf, const size_t buflen)
{
  int saved_errno = 0;
  int ok = 1;
  ssize_t len = 0;
  DIR *dirp;

  if (attrdirfd == -1)
    ok = 0;

  if (ok)
  {
    dirp = fdopendir(attrdirfd);
    if (dirp == NULL)
    {
      ok = 0;
    }  
  }

  if (ok)
  {
    struct dirent *de;

    while ((de = readdir(dirp)))
    {
      const size_t namelen = strlen(de->d_name);

      /* Ignore "." and ".." entries */
      if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
	continue;

      if (buflen)
      {
	/* Check for space, then copy directory name + nul into list. */
	if ((len + namelen + 1) > buflen)
	{
	  saved_errno = errno = ERANGE;
	  ok = 0;
	  break;
	}
	else
	{
	  strcpy(buf + len, de->d_name);
	  len += namelen;
	  buf[len] = '\0';
	  ++len;
	}
      }
      else
      {
	/* Seeing how much space is needed? */
	len += namelen + 1;
      }
    }
  }

  if (!ok)
    saved_errno = errno;
  if ((attrdirfd >= 0) && (close(attrdirfd) == -1) && !saved_errno)
    saved_errno = errno;
  if (dirp && (closedir(dirp) == -1) && !saved_errno)
    saved_errno = errno;
  if (saved_errno)
    errno = saved_errno;

  return ok ? len : -errno;
}

static int
hasattrclose (const int attrdirfd)
{
  int saved_errno = 0;
  int ret = 0; /* Not by default */
  DIR *dirp = NULL;

  if (attrdirfd == -1)
    ret = -1;

  if (ret >= 0)
  {
    dirp = fdopendir(attrdirfd);
    if (dirp == NULL)
    {
      ret = -1;
    }
  }

  if (ret >= 0)
  {
    struct dirent *de;

    while ((de = readdir(dirp)))
    {
      /* Ignore "." and ".." entries */
      if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
	continue;

      /* Found a file */
      ret = 1;
      break;
    }
  }

  if (ret == -1)
    saved_errno = errno;
  if ((attrdirfd >= 0) && (close(attrdirfd) == -1) && !saved_errno)
    saved_errno = errno;
  if (dirp && (closedir(dirp) == -1) && !saved_errno)
    saved_errno = errno;
  if (saved_errno)
    errno = saved_errno;

  return (ret >= 0) ? ret : -errno;
}

int
solaris_setxattr (const char *path,
		  const char *attrname,
		  const char *attrvalue,
		  const size_t slen,
		  struct hv *flags)
{
  /* XXX: Support overwrite/no overwrite flags */
  int saved_errno = 0;
  int ok = 1;
  File_ExtAttr_setflags_t setflags;
  int openflags = O_RDWR;
  int attrfd = -1;

  setflags = File_ExtAttr_flags2setflags(flags);
  switch (setflags)
  {
  case SET_CREATEIFNEEDED: openflags |= O_CREAT; break;
  case SET_CREATE:         openflags |= O_CREAT | O_EXCL; break;
  case SET_REPLACE:        break;
  }

  if (!File_ExtAttr_valid_default_namespace(flags))
  {
    errno = EOPNOTSUPP;
    ok = 0;
  }

  if (ok)
    attrfd = attropen(path, attrname, openflags, ATTRMODE);

  /* XXX: More common code? */
  if (ok && (attrfd == -1))
    ok = 0;
  if (ok && (writexattr(attrfd, attrvalue, slen) == -1))
    ok = 0;

  if (!ok)
    saved_errno = errno;
  if ((attrfd >= 0) && (close(attrfd) == -1) && !saved_errno)
    saved_errno = errno;
  if (saved_errno)
    errno = saved_errno;

  return ok ? 0 : -errno;
}

int solaris_fsetxattr (const int fd,
		       const char *attrname,
		       const char *attrvalue,
		       const size_t slen,
		       struct hv *flags)
{
  /* XXX: Support overwrite/no overwrite flags */
  int saved_errno = 0;
  int ok = 1;
  int openflags = O_RDWR | O_XATTR;
  File_ExtAttr_setflags_t setflags;
  int attrfd = -1;

  setflags = File_ExtAttr_flags2setflags(flags);
  switch (setflags)
  {
  case SET_CREATEIFNEEDED: openflags |= O_CREAT; break;
  case SET_CREATE:         openflags |= O_CREAT | O_EXCL; break;
  case SET_REPLACE:        break;
  }

  if (!File_ExtAttr_valid_default_namespace(flags))
  {
    errno = EOPNOTSUPP;
    ok = 0;
  }

  if (ok)
    attrfd = openat(fd, attrname, openflags, ATTRMODE);

  /* XXX: More common code? */
  if (ok && (attrfd == -1))
    ok = 0;
  if (ok && (writexattr(attrfd, attrvalue, slen) == -1))
    ok = 0;

  if (!ok)
    saved_errno = errno;
  if ((attrfd >= 0) && (close(attrfd) == -1) && !saved_errno)
    saved_errno = errno;
  if (saved_errno)
    errno = saved_errno;

  return ok ? 0 : -errno;
}

int
solaris_getxattr (const char *path,
		  const char *attrname,
		  void *attrvalue,
		  const size_t slen,
		  struct hv *flags)
{
  int attrfd = -1;
  int ok = 1;

  if (!File_ExtAttr_valid_default_namespace(flags))
  {
    errno = EOPNOTSUPP;
    ok = 0;
  }

  if (ok)
    attrfd = attropen(path, attrname, O_RDONLY);

  return ok ? readclose(attrfd, attrvalue, slen) : -errno;
}

int
solaris_fgetxattr (const int fd,
		   const char *attrname,
		   void *attrvalue,
		   const size_t slen,
		   struct hv *flags)
{
  int attrfd = -1;
  int ok = 1;

  if (!File_ExtAttr_valid_default_namespace(flags))
  {
    errno = EOPNOTSUPP;
    ok = 0;
  }

  if (ok)
    attrfd = openat(fd, attrname, O_RDONLY|O_XATTR);

  return ok ? readclose(attrfd, attrvalue, slen) : -errno;
}

int
solaris_removexattr (const char *path,
		     const char *attrname,
		     struct hv *flags)
{
  int attrdirfd = -1;
  int ok = 1;

  if (!File_ExtAttr_valid_default_namespace(flags))
  {
    errno = EOPNOTSUPP;
    ok = 0;
  }

  if (ok)
    attrdirfd = attropen(path, ".", O_RDONLY);

  return ok ? unlinkclose(attrdirfd, attrname) : -errno;
}

int
solaris_fremovexattr (const int fd,
		      const char *attrname,
		      struct hv *flags)
{
  int attrdirfd = -1;
  int ok = 1;

  if (!File_ExtAttr_valid_default_namespace(flags))
  {
    errno = EOPNOTSUPP;
    ok = 0;
  }

  if (ok)
    attrdirfd = openat(fd, ".", O_RDONLY|O_XATTR);

  return ok ? unlinkclose(attrdirfd, attrname) : -errno;
}

ssize_t
solaris_listxattr (const char *path,
		   char *buf,
		   const size_t buflen,
		   struct hv *flags)
{
  int attrdirfd = attropen(path, ".", O_RDONLY);
  return listclose(attrdirfd, buf, buflen);
}

ssize_t
solaris_flistxattr (const int fd,
		    char *buf,
		    const size_t buflen,
		    struct hv *flags)
{
  int attrdirfd = openat(fd, ".", O_RDONLY|O_XATTR);
  return listclose(attrdirfd, buf, buflen);
}

ssize_t solaris_listxattrns (const char *path,
			     char *buf,
			     const size_t buflen,
			     struct hv *flags)
{
  int attrdirfd;
  ssize_t ret;

  attrdirfd = attropen(path, ".", O_RDONLY);
  ret = hasattrclose(attrdirfd);
  if (ret > 0)
    ret = File_ExtAttr_default_listxattrns(buf, buflen);

  return ret;
}

ssize_t solaris_flistxattrns (const int fd,
			      char *buf,
			      const size_t buflen,
			      struct hv *flags)
{
  int attrdirfd;
  ssize_t ret;

  attrdirfd = openat(fd, ".", O_RDONLY|O_XATTR);
  ret = hasattrclose(attrdirfd);
  if (ret > 0)
    ret = File_ExtAttr_default_listxattrns(buf, buflen);

  return ret;
}

#endif /* EXTATTR_SOLARIS */
