#ifndef EXTATTR_LINUX_H
#define EXTATTR_LINUX_H

#include <sys/types.h>
#include <attr/attributes.h>
#include <attr/xattr.h>

struct hv;

int linux_setxattr (const char *path,
                    const char *attrname,
                    const char *attrvalue,
                    const size_t slen,
                    struct hv *flags);

int linux_fsetxattr (const int fd,
                     const char *attrname,
                     const char *attrvalue,
                     const size_t slen,
                     struct hv *flags);

int linux_getxattr (const char *path,
                    const char *attrname,
                    void *attrvalue,
                    const size_t slen,
                    struct hv *flags);

int linux_fgetxattr (const int fd,
                     const char *attrname,
                     void *attrvalue,
                     const size_t slen,
                     struct hv *flags);

int linux_removexattr (const char *path,
                       const char *attrname,
                       struct hv *flags);

int linux_fremovexattr (const int fd,
                        const char *attrname,
                        struct hv *flags);

ssize_t linux_listxattr (const char *path,
                         char *buf,
                         const size_t buflen,
                         struct hv *flags);

ssize_t linux_flistxattr (const int fd,
                          char *buf,
                          const size_t buflen,
                          struct hv *flags);

ssize_t linux_listxattrns (const char *path,
			   char *buf,
			   const size_t buflen,
			   struct hv *flags);

ssize_t linux_flistxattrns (const int fd,
			    char *buf,
			    const size_t buflen,
			    struct hv *flags);

#endif /* EXTATTR_LINUX_H */
