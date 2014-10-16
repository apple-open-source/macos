#include "extattr_os.h"

#ifdef EXTATTR_BSD

#include <errno.h>

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "flags.h"

static int
valid_namespace (struct hv *flags, int *pattrnamespace)
{
  const size_t NAMESPACE_KEYLEN = strlen(NAMESPACE_KEY);
  SV **psv_ns;
  char *ns = NULL;
  int ok = 1; /* Default is valid */
  int attrnamespace = EXTATTR_NAMESPACE_USER;

  if (flags && (psv_ns = hv_fetch(flags, NAMESPACE_KEY, NAMESPACE_KEYLEN, 0)))
  {
    /*
     * Undefined => default. Otherwise "user" and "system" are valid.
     */
    if (SvOK(*psv_ns))
    {
      char *s;
      STRLEN len = 0;

      s = SvPV(*psv_ns, len);

      if (len)
      {
	if (memcmp(NAMESPACE_USER, s, len) == 0)
	  attrnamespace = EXTATTR_NAMESPACE_USER;
	else if (memcmp(NAMESPACE_SYSTEM, s, len) == 0)
	  attrnamespace = EXTATTR_NAMESPACE_SYSTEM;
	else
	  ok = 0;
      }
      else
      {
	ok = 0;
      }
    }
  }

  if (ok)
    *pattrnamespace = attrnamespace;

  return ok;
}

/* Helper to convert number of bytes written into success/failure code. */
static inline int
bsd_extattr_set_succeeded (const int expected, const int actual)
{
  int ret = 0;

  if (actual == -1) {
    ret = -errno;
  } else if (actual != expected) {
    /* Pretend there's not enough space for the data. */
    ret = -ENOBUFS;
  }

  return ret;
}

int
bsd_setxattr (const char *path,
	      const char *attrname,
	      const char *attrvalue,
	      const size_t slen,
	      struct hv *flags)
{
  int attrnamespace = -1;
  int ret = 0;

  if (!valid_namespace(flags, &attrnamespace))
  {
    ret = -EOPNOTSUPP;
  }

  if (ret == 0)
  {
    File_ExtAttr_setflags_t setflags = File_ExtAttr_flags2setflags(flags);
    switch (setflags)
    {
    case SET_CREATEIFNEEDED:
    case SET_REPLACE:
      /* Default behaviour */
      break;

    case SET_CREATE:
      /*
       * This needs to be emulated, since the default *BSD calls
       * don't provide a way of failing if the attribute exists.
       * This emulation is inherently racy.
       */
      {
	ssize_t sz = extattr_get_file(path, attrnamespace, attrname, NULL, 0);
	if (sz >= 0)
	{
	  /* Attribute already exists => fail. */
	  ret = -EEXIST;
	}
      }
      break;
    }
  }

  if (ret == 0)
  {
    ret = extattr_set_file(path, attrnamespace, attrname, attrvalue, slen);
    ret = bsd_extattr_set_succeeded(slen, ret);
  }

  return ret;
}

int
bsd_fsetxattr (const int fd,
	       const char *attrname,
	       const char *attrvalue,
	       const size_t slen,
	       struct hv *flags)
{
  int attrnamespace = -1;
  int ret = 0;

  if (!valid_namespace(flags, &attrnamespace))
  {
    ret = -EOPNOTSUPP;
  }

  if (ret == 0)
  {
    File_ExtAttr_setflags_t setflags = File_ExtAttr_flags2setflags(flags);
    switch (setflags)
    {
    case SET_CREATEIFNEEDED:
    case SET_REPLACE:
      /* Default behaviour */
      break;

    case SET_CREATE:
      /*
       * This needs to be emulated, since the default *BSD calls
       * don't provide a way of failing if the attribute exists.
       * This emulation is inherently racy.
       */
      {
	ssize_t sz = extattr_get_fd(fd, attrnamespace, attrname, NULL, 0);
	if (sz >= 0)
	{
	  /* Attribute already exists => fail. */
	  ret = -EEXIST;
	}
      }
      break;
    }
  }

  if (ret == 0)
  {
    ret = extattr_set_fd(fd, attrnamespace, attrname, attrvalue, slen);
    ret = bsd_extattr_set_succeeded(slen, ret);
  }

  return ret;
}

int
bsd_getxattr (const char *path,
	      const char *attrname,
	      void *attrvalue,
	      const size_t slen,
	      struct hv *flags)
{
  int attrnamespace = -1;
  int ret = 0;

  if (!valid_namespace(flags, &attrnamespace))
  {
    ret = -EOPNOTSUPP;
  }

  if (ret == 0) {
    ret = extattr_get_file(path, attrnamespace, attrname, attrvalue, slen);
    if (ret < 0) {
      ret = -errno;
    }
  }

  return ret;
}

int
bsd_fgetxattr (const int fd,
	       const char *attrname,
	       void *attrvalue,
	       const size_t slen,
	       struct hv *flags)
{
  int attrnamespace = -1;
  int ret = 0;

  if (!valid_namespace(flags, &attrnamespace))
  {
    ret = -EOPNOTSUPP;
  }

  if (ret == 0) {
    ret = extattr_get_fd(fd, attrnamespace, attrname, attrvalue, slen);
    if (ret < 0) {
      ret = -errno;
    }
  }

  return ret;
}

int
bsd_removexattr (const char *path,
		 const char *attrname,
		 struct hv *flags)
{
  int attrnamespace = -1;
  int ret = 0;

  if (!valid_namespace(flags, &attrnamespace))
  {
    ret = -EOPNOTSUPP;
  }

  if (ret == 0) {
    ret = extattr_delete_file(path, attrnamespace, attrname);
    if (ret < 0) {
      ret = -errno;
    }
  }

  return ret;
}

int
bsd_fremovexattr (const int fd,
		  const char *attrname,
		  struct hv *flags)
{
  int attrnamespace = -1;
  int ret = 0;

  if (!valid_namespace(flags, &attrnamespace))
  {
    ret = -EOPNOTSUPP;
  }

  if (ret == 0) {
    ret = extattr_delete_fd(fd, attrnamespace, attrname);
    if (ret < 0) {
      ret = -errno;
    }
  }

  return ret;
}

/* Convert the BSD-style list to a nul-separated list. */
static void
reformat_list (char *buf, const ssize_t len)
{
  ssize_t pos = 0;
  ssize_t attrlen;

  while (pos < len)
  {
    attrlen = (unsigned char) buf[pos];
    memmove(buf + pos, buf + pos + 1, attrlen);
    buf[pos + attrlen] = '\0';
    pos += attrlen + 1;
  }
}

ssize_t
bsd_listxattr (const char *path,
	       char *buf,
	       const size_t buflen,
	       struct hv *flags)
{
  int attrnamespace = -1;
  ssize_t ret = 0;

  if (!valid_namespace(flags, &attrnamespace))
  {
    ret = -EOPNOTSUPP;
  }

  if (ret == 0)
  {
    ret = extattr_list_file(path,
			    attrnamespace,
			    /* To get the length on *BSD, pass NULL here. */
			    buflen ? buf : NULL,
			    buflen);

    if (buflen && (ret > 0))
      reformat_list(buf, ret);

    if (ret < 0) {
      ret = -errno;
    }
  }

  return ret;
}

ssize_t
bsd_flistxattr (const int fd,
		char *buf,
		const size_t buflen,
		struct hv *flags)
{
  int attrnamespace = -1;
  ssize_t ret = 0;

  if (!valid_namespace(flags, &attrnamespace))
  {
    ret = -EOPNOTSUPP;
  }

  if (ret == 0)
  {
    ret = extattr_list_fd(fd,
			  attrnamespace,
			  /* To get the length on *BSD, pass NULL here. */
			  buflen ? buf : NULL,
			  buflen);

    if (buflen && (ret > 0))
      reformat_list(buf, ret);

    if (ret < 0) {
      ret = -errno;
    }
  }

  return ret;
}

static ssize_t
listxattrns (char *buf, const size_t buflen,
	     const int iHasUser, const int iHasSystem)
{
  size_t len = 0;
  ssize_t ret = 0;

  if (iHasUser)
    len += sizeof(NAMESPACE_USER);
  if (iHasSystem)
    len += sizeof(NAMESPACE_SYSTEM);

  if (buflen >= len)
  {
    char *p = buf;

    if (iHasUser)
    {
      memcpy(p, NAMESPACE_USER, sizeof(NAMESPACE_USER));
      p += sizeof(NAMESPACE_USER);
    }
    if (iHasSystem)
    {
      memcpy(p, NAMESPACE_SYSTEM, sizeof(NAMESPACE_SYSTEM));
      p += sizeof(NAMESPACE_SYSTEM);
    }

    ret = len;
  }
  else if (buflen == 0)
  {
    ret = len;
  }
  else
  {
    ret = -ERANGE;
  }

  return ret;
}

ssize_t
bsd_listxattrns (const char *path,
		 char *buf,
		 const size_t buflen,
		 struct hv *flags)
{
  int iHasUser = 0;
  int iHasSystem = 0;
  ssize_t ret = 0;

  ret = extattr_list_file(path, EXTATTR_NAMESPACE_USER, NULL, 0);
  if (ret > 0)
    iHasUser = 1;

  if (ret >= 0)
  {
    ret = extattr_list_file(path, EXTATTR_NAMESPACE_SYSTEM, NULL, 0);
    if (ret > 0)
      iHasSystem = 1;

    /*
     * XXX: How do we cope with EPERM? Throw an exception.
     * For now ignore it, although this could cause problems.
     */
    if (ret == -1 && errno == EPERM)
      ret = 0;
  }

  if (ret >= 0)
    ret = listxattrns(buf, buflen, iHasUser, iHasSystem);

  return ret;
}

ssize_t
bsd_flistxattrns (const int fd,
		  char *buf,
		  const size_t buflen,
		  struct hv *flags)
{
  int iHasUser = 0;
  int iHasSystem = 0;
  ssize_t ret;

  ret = extattr_list_fd(fd, EXTATTR_NAMESPACE_USER, NULL, 0);
  if (ret > 0)
    iHasUser = 1;

  if (ret >= 0)
  {
    ret = extattr_list_fd(fd, EXTATTR_NAMESPACE_SYSTEM, NULL, 0);
    if (ret > 0)
      iHasSystem = 1;

    /*
     * XXX: How do we cope with EPERM? Throw an exception.
     * For now ignore it, although this could cause problems.
     */
    if (ret == -1 && errno == EPERM)
      ret = 0;
  }

  if (ret >= 0)
    ret = listxattrns(buf, buflen, iHasUser, iHasSystem);

  return ret;
}

#endif /* EXTATTR_BSD */
