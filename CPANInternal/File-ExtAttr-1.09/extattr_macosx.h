#ifndef EXTATTR_MACOSX_H
#define EXTATTR_MACOSX_H

#include <sys/types.h>
#include <sys/xattr.h>

struct hv;

int macosx_setxattr (const char *path,
		     const char *attrname,
		     const char *attrvalue,
		     const size_t slen,
		     struct hv *flags);

int macosx_fsetxattr (const int fd,
		      const char *attrname,
		      const char *attrvalue,
		      const size_t slen,
		      struct hv *flags);

int macosx_getxattr (const char *path,
		     const char *attrname,
		     void *attrvalue,
		     const size_t slen,
		     struct hv *flags);

int macosx_fgetxattr (const int fd,
		      const char *attrname,
		      void *attrvalue,
		      const size_t slen,
		      struct hv *flags);

int macosx_removexattr (const char *path,
			const char *attrname,
			struct hv *flags);

int macosx_fremovexattr (const int fd,
			 const char *attrname,
			 struct hv *flags);

ssize_t macosx_listxattr (const char *path,
			  char *buf,
			  const size_t buflen,
			  struct hv *flags);

ssize_t macosx_flistxattr (const int fd,
			   char *buf,
			   const size_t buflen,
			   struct hv *flags);

ssize_t macosx_listxattrns (const char *path,
                            char *buf,
                            const size_t buflen,
                            struct hv *flags);

ssize_t macosx_flistxattrns (const int fd,
                             char *buf,
                             const size_t buflen,
                             struct hv *flags);

#endif /* EXTATTR_MACOSX_H */
