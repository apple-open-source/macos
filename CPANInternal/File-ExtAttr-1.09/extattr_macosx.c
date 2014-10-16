#include "extattr_os.h"

#ifdef EXTATTR_MACOSX

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "flags.h"

int
macosx_setxattr (const char *path,
		 const char *attrname,
		 const char *attrvalue,
		 const size_t slen,
		 struct hv *flags)
{
  int ok = 1;
  File_ExtAttr_setflags_t setflags = 0;
  int xflags = 0;
  int ret = -1;

  setflags = File_ExtAttr_flags2setflags(flags);
  switch (setflags)
  {
  case SET_CREATEIFNEEDED: break;
  case SET_CREATE:         xflags |= XATTR_CREATE; break;
  case SET_REPLACE:        xflags |= XATTR_REPLACE; break;
  }

  if (!File_ExtAttr_valid_default_namespace(flags))
  {
    errno = EOPNOTSUPP;
    ret = -errno;
    ok = 0;
  }

  if (ok) {
    ret = setxattr(path, attrname, attrvalue, slen, 0, xflags);
    if (ret < 0) {
      ret = -errno;
      ok = 0;
    }
  }

  return ret;
}

int
macosx_fsetxattr (const int fd,
		  const char *attrname,
		  const char *attrvalue,
		  const size_t slen,
		  struct hv *flags)
{
  int ok = 1;
  File_ExtAttr_setflags_t setflags;
  int xflags = 0;
  int ret = -1;

  setflags = File_ExtAttr_flags2setflags(flags);
  switch (setflags)
  {
  case SET_CREATEIFNEEDED: break;
  case SET_CREATE:         xflags |= XATTR_CREATE; break;
  case SET_REPLACE:        xflags |= XATTR_REPLACE; break;
  }

  if (!File_ExtAttr_valid_default_namespace(flags))
  {
    errno = EOPNOTSUPP;
    ret = -errno;
    ok = 0;
  }

  if (ok) {
    ret = fsetxattr(fd, attrname, attrvalue, slen, 0, xflags);
    if (ret < 0) {
      ret = -errno;
      ok = 0;
    }
  }

  return ret;
}

int
macosx_getxattr (const char *path,
		 const char *attrname,
		 void *attrvalue,
		 const size_t slen,
		 struct hv *flags)
{
  int ok = 1;
  int xflags = 0;
  int ret = -1;

  if (!File_ExtAttr_valid_default_namespace(flags))
  {
    errno = EOPNOTSUPP;
    ret = -errno;
    ok = 0;
  }

  if (ok) {
    ret = getxattr(path, attrname, attrvalue, slen, 0, xflags);
    if (ret < 0) {
      ret = -errno;
      ok = 0;
    }
  }

  return ret;
}

int
macosx_fgetxattr (const int fd,
		  const char *attrname,
		  void *attrvalue,
		  const size_t slen,
		  struct hv *flags)
{
  int ok = 1;
  int xflags = 0;
  int ret = -1;

  if (!File_ExtAttr_valid_default_namespace(flags))
  {
    errno = EOPNOTSUPP;
    ret = -errno;
    ok = 0;
  }

  if (ok) {
    ret = fgetxattr(fd, attrname, attrvalue, slen, 0, xflags);
    if (ret < 0) {
      ret = -errno;
      ok = 0;
    }
  }

  return ret;
}

int
macosx_removexattr (const char *path,
		    const char *attrname,
		    struct hv *flags)
{
  int ok = 1;
  int xflags = 0;
  int ret = -1;

  if (!File_ExtAttr_valid_default_namespace(flags))
  {
    errno = EOPNOTSUPP;
    ret = -errno;
    ok = 0;
  }

  if (ok) {
    ret = removexattr(path, attrname, xflags);
    if (ret < 0) {
      ret = -errno;
      ok = 0;
    }
  }

  return ret;
}

int
macosx_fremovexattr (const int fd,
		     const char *attrname,
		     struct hv *flags)
{
  int ok = 1;
  int xflags = 0;
  int ret = -1;

  if (!File_ExtAttr_valid_default_namespace(flags))
  {
    errno = EOPNOTSUPP;
    ret = -errno;
    ok = 0;
  }

  if (ok) {
    ret = fremovexattr(fd, attrname, xflags);
    if (ret < 0) {
      ret = -errno;
      ok = 0;
    }
  }

  return ret;
}

ssize_t
macosx_listxattr (const char *path,
		  char *buf,
		  const size_t buflen,
		  struct hv *flags)
{
  int ok = 1;
  int ret = -1;

  if (!File_ExtAttr_valid_default_namespace(flags))
  {
    errno = EOPNOTSUPP;
    ret = -errno;
    ok = 0;
  }

  if (ok) {
    ret = listxattr(path, buf, buflen, 0 /* XXX: flags? */);
    if (ret < 0) {
      ret = -errno;
      ok = 0;
    }
  }

  return ret;
}

ssize_t
macosx_flistxattr (const int fd,
		   char *buf,
		   const size_t buflen,
		   struct hv *flags)
{
  int ok = 1;
  int ret = -1;

  if (!File_ExtAttr_valid_default_namespace(flags))
  {
    errno = EOPNOTSUPP;
    ret = -errno;
    ok = 0;
  }

  if (ok) {
    ret = flistxattr(fd, buf, buflen, 0 /* XXX: flags? */);
    if (ret < 0) {
      ret = -errno;
      ok = 0;
    }
  }

  return ret;
}

ssize_t
macosx_listxattrns (const char *path,
                    char *buf,
                    const size_t buflen,
                    struct hv *flags)
{
  ssize_t ret = listxattr(path, NULL, 0, 0 /* XXX: flags? */);

  if (ret > 0) {
    ret = File_ExtAttr_default_listxattrns(buf, buflen);
  } else if (ret < 0) {
    ret = -errno;
  }

  return ret;
}

ssize_t
macosx_flistxattrns (const int fd,
                     char *buf,
                     const size_t buflen,
                     struct hv *flags)
{
  ssize_t ret = flistxattr(fd, NULL, 0, 0 /* XXX: flags? */);

  if (ret > 0) {
    ret = File_ExtAttr_default_listxattrns(buf, buflen);
  } else if (ret < 0) {
    ret = -errno;
  }

  return ret;
}

#endif /* EXTATTR_MACOSX */
