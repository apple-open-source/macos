#ifndef EXTATTR_PORTABLE_H
#define EXTATTR_PORTABLE_H

/* OS detection */
#include "extattr_os.h"

struct hv;

/*
 * Portable extattr functions. When these fail, they should return
 * -errno, i.e.: < 0 indicates failure.
 */

static inline int
portable_setxattr (const char *path,
                   const char *attrname,
                   const void *attrvalue,
                   const size_t slen,
                   struct hv *flags)
{
#ifdef EXTATTR_MACOSX
  return macosx_setxattr(path, attrname, attrvalue, slen, flags);
#elif defined(EXTATTR_BSD)
  return bsd_setxattr(path, attrname, attrvalue, slen, flags);
#elif defined(EXTATTR_SOLARIS)
  return solaris_setxattr(path, attrname, attrvalue, slen, flags);
#else
  return linux_setxattr(path, attrname, attrvalue, slen, flags);
#endif
}

static inline int
portable_fsetxattr (const int fd,
                    const char *attrname,
                    const void *attrvalue,
                    const size_t slen,
                    struct hv *flags)
{
#ifdef EXTATTR_MACOSX
  return macosx_fsetxattr(fd, attrname, attrvalue, slen, flags);
#elif defined(EXTATTR_BSD)
  return bsd_fsetxattr(fd, attrname, attrvalue, slen, flags);
#elif defined(EXTATTR_SOLARIS)
  return solaris_fsetxattr(fd, attrname, attrvalue, slen, flags);
#else
  return linux_fsetxattr(fd, attrname, attrvalue, slen, flags);
#endif
}

static inline int
portable_getxattr (const char *path,
                   const char *attrname,
                   void *attrvalue,
                   const size_t slen,
                   struct hv *flags)
{
#ifdef EXTATTR_MACOSX
  return macosx_getxattr(path, attrname, attrvalue, slen, flags);
#elif defined(EXTATTR_BSD)
  return bsd_getxattr(path, attrname, attrvalue, slen, flags);
#elif defined(EXTATTR_SOLARIS)
  return solaris_getxattr(path, attrname, attrvalue, slen, flags);
#else
  return linux_getxattr(path, attrname, attrvalue, slen, flags);
#endif
}

static inline int
portable_fgetxattr (const int fd,
                    const char *attrname,
                    void *attrvalue,
                    const size_t slen,
                    struct hv *flags)
{
#ifdef EXTATTR_MACOSX
  return macosx_fgetxattr(fd, attrname, attrvalue, slen, flags);
#elif defined(EXTATTR_BSD)
  return bsd_fgetxattr(fd, attrname, attrvalue, slen, flags);
#elif defined(EXTATTR_SOLARIS)
  return solaris_fgetxattr(fd, attrname, attrvalue, slen, flags);
#else
  return linux_fgetxattr(fd, attrname, attrvalue, slen, flags);
#endif
}

static inline ssize_t
portable_lenxattr (const char *path, const char *attrname, struct hv *flags)
{
#ifdef EXTATTR_BSD
  /* XXX: flags? Namespace? */
  return extattr_get_file(path, EXTATTR_NAMESPACE_USER, attrname, NULL, 0);
#else
  /* XXX: Can BSD use this too? Maybe once namespacing sorted. */
  return portable_getxattr(path, attrname, NULL, 0, flags);
#endif
}

static inline int
portable_flenxattr (int fd, const char *attrname, struct hv *flags)
{
#ifdef EXTATTR_BSD
  /* XXX: flags? Namespace? */
  return extattr_get_fd(fd, EXTATTR_NAMESPACE_USER, attrname, NULL, 0);
#else
  /* XXX: Can BSD use this too? Maybe once namespacing sorted. */
  return portable_fgetxattr(fd, attrname, NULL, 0, flags);
#endif
}

static inline int
portable_removexattr (const char *path, const char *name, struct hv *flags)
{
#ifdef EXTATTR_MACOSX
  return macosx_removexattr(path, name, flags);
#elif defined(EXTATTR_BSD)
  return bsd_removexattr(path, name, flags);
#elif defined(EXTATTR_SOLARIS)
  return solaris_removexattr(path, name, flags);
#else
  return linux_removexattr(path, name, flags);
#endif
}

static inline int
portable_fremovexattr (const int fd, const char *name, struct hv *flags)
{
#ifdef EXTATTR_MACOSX
  return macosx_fremovexattr(fd, name, flags);
#elif defined(EXTATTR_BSD)
  return bsd_fremovexattr(fd, name, flags);
#elif defined(EXTATTR_SOLARIS)
  return solaris_fremovexattr(fd, name, flags);
#else
  return linux_fremovexattr(fd, name, flags);
#endif
}

static inline int
portable_listxattr(const char *path,
                   char *buf,
                   const size_t slen,
                   struct hv *flags)
{
#ifdef EXTATTR_MACOSX
  return macosx_listxattr(path, buf, slen, flags);
#elif defined(EXTATTR_BSD)
  return bsd_listxattr(path, buf, slen, flags);
#elif defined(EXTATTR_SOLARIS)
  return solaris_listxattr(path, buf, slen, flags);
#else
  return linux_listxattr(path, buf, slen, flags);
#endif
}

static inline int
portable_flistxattr(const int fd,
                    char *buf,
                    const size_t slen,
                    struct hv *flags)
{
#ifdef EXTATTR_MACOSX
  return macosx_flistxattr(fd, buf, slen, flags);
#elif defined(EXTATTR_BSD)
  return bsd_flistxattr(fd, buf, slen, flags);
#elif defined(EXTATTR_SOLARIS)
  return solaris_flistxattr(fd, buf, slen, flags);
#else
  return linux_flistxattr(fd, buf, slen, flags);
#endif
}

static inline int
portable_listxattrns(const char *path,
		     char *buf,
		     const size_t slen,
		     struct hv *flags)
{
#ifdef EXTATTR_MACOSX
  return macosx_listxattrns(path, buf, slen, flags);
#elif defined(EXTATTR_BSD)
  return bsd_listxattrns(path, buf, slen, flags);
#elif defined(EXTATTR_SOLARIS)
  return solaris_listxattrns(path, buf, slen, flags);
#else
  return linux_listxattrns(path, buf, slen, flags);
#endif
}

static inline int
portable_flistxattrns(const int fd,
		      char *buf,
		      const size_t slen,
		      struct hv *flags)
{
#ifdef EXTATTR_MACOSX
  return macosx_flistxattrns(fd, buf, slen, flags);
#elif defined(EXTATTR_BSD)
  return bsd_flistxattrns(fd, buf, slen, flags);
#elif defined(EXTATTR_SOLARIS)
  return solaris_flistxattrns(fd, buf, slen, flags);
#else
  return linux_flistxattrns(fd, buf, slen, flags);
#endif
}

#endif /* EXTATTR_PORTABLE_H */
