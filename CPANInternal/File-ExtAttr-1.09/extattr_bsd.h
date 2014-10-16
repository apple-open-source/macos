#ifndef EXTATTR_BSD_H
#define EXTATTR_BSD_H

#include <sys/types.h>
#include <sys/extattr.h>
#include <sys/uio.h>

struct hv;

int bsd_setxattr (const char *path,
		  const char *attrname,
		  const char *attrvalue,
		  const size_t slen,
		  struct hv *flags);

int bsd_fsetxattr (const int fd,
		   const char *attrname,
		   const char *attrvalue,
		   const size_t slen,
		   struct hv *flags);

int bsd_getxattr (const char *path,
		  const char *attrname,
		  void *attrvalue,
		  const size_t slen,
		  struct hv *flags);

int bsd_fgetxattr (const int fd,
		   const char *attrname,
		   void *attrvalue,
		   const size_t slen,
		   struct hv *flags);

int bsd_removexattr (const char *path,
		     const char *attrname,
		     struct hv *flags);

int bsd_fremovexattr (const int fd,
		      const char *attrname,
		      struct hv *flags);

ssize_t bsd_listxattr (const char *path,
		       char *buf,
		       const size_t buflen,
		       struct hv *flags);

ssize_t bsd_flistxattr (const int fd,
			char *buf,
			const size_t buflen,
			struct hv *flags);

ssize_t bsd_listxattrns (const char *path,
			 char *buf,
			 const size_t buflen,
			 struct hv *flags);

ssize_t bsd_flistxattrns (const int fd,
			  char *buf,
			  const size_t buflen,
			  struct hv *flags);

#endif /* EXTATTR_BSD_H */
