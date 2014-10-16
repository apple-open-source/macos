#ifndef EXTATTR_SOLARIS_H
#define EXTATTR_SOLARIS_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/*
 * XXX: FIXME: Need to distinguish file non-existence and attribute
 * non-existence. Need to choose an unused error code somehow.
 */
#ifndef ENOATTR
#define ENOATTR ENOENT
#endif

struct hv;

int solaris_setxattr (const char *path,
		      const char *attrname,
		      const char *attrvalue,
		      const size_t slen,
		      struct hv *flags);

int solaris_fsetxattr (const int fd,
		       const char *attrname,
		       const char *attrvalue,
		       const size_t slen,
		       struct hv *flags);

int solaris_getxattr (const char *path,
		      const char *attrname,
		      void *attrvalue,
		      const size_t slen,
		      struct hv *flags);

int solaris_fgetxattr (const int fd,
		       const char *attrname,
		       void *attrvalue,
		       const size_t slen,
		      struct hv *flags);

int solaris_removexattr (const char *path,
			 const char *attrname,
			 struct hv *flags);

int solaris_fremovexattr (const int fd,
			  const char *attrname,
			  struct hv *flags);

ssize_t solaris_listxattr (const char *path,
			   char *buf,
			   const size_t buflen,
			   struct hv *flags);

ssize_t solaris_flistxattr (const int fd,
			    char *buf,
			    const size_t buflen,
			    struct hv *flags);

ssize_t solaris_listxattrns (const char *path,
			     char *buf,
			     const size_t buflen,
			     struct hv *flags);

ssize_t solaris_flistxattrns (const int fd,
			      char *buf,
			      const size_t buflen,
			      struct hv *flags);

#endif /* EXTATTR_SOLARIS_H */
