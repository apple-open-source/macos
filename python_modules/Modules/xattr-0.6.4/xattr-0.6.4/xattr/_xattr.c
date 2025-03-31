#include "Python.h"
#ifdef __FreeBSD__
#include <sys/extattr.h>
#elif defined(__SUN__) || defined(__sun__)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#else
#include <sys/xattr.h>
#endif

#ifdef __FreeBSD__

/* FreeBSD compatibility API */
#define XATTR_XATTR_NOFOLLOW 0x0001
#define XATTR_XATTR_CREATE 0x0002
#define XATTR_XATTR_REPLACE 0x0004
#define XATTR_XATTR_NOSECURITY 0x0008


/* Converts a freebsd format attribute list into a NULL terminated list.
 * While the man page on extattr_list_file says it is NULL terminated, 
 * it is actually the first byte that is the length of the
 * following attribute.
 */
static void convert_bsd_list(char *namebuf, size_t size)
{
    size_t offset = 0;
    while(offset < size) {
        int length = (int) namebuf[offset];
        memmove(namebuf+offset, namebuf+offset+1, length);
        namebuf[offset+length] = '\0';
        offset += length+1;
    }
}

static ssize_t xattr_getxattr(const char *path, const char *name,
                              void *value, ssize_t size, u_int32_t position, 
                              int options)
{
    if (position != 0 || 
        !(options == 0 || options == XATTR_XATTR_NOFOLLOW)) {
        return -1;
    }

    if (options & XATTR_XATTR_NOFOLLOW) {
        return extattr_get_link(path, EXTATTR_NAMESPACE_USER,
                                name, value, size);
    }
    else {
        return extattr_get_file(path, EXTATTR_NAMESPACE_USER,
                                name, value, size);
    }
}

static ssize_t xattr_setxattr(const char *path, const char *name,
                              void *value, ssize_t size, u_int32_t position,
                              int options)
{
    int rv = 0;
    int nofollow;

    if (position != 0) {
        return -1;
    }

    nofollow = options & XATTR_XATTR_NOFOLLOW;
    options &= ~XATTR_XATTR_NOFOLLOW;

    if (options == XATTR_XATTR_CREATE ||
        options == XATTR_XATTR_REPLACE) {

        /* meh. FreeBSD doesn't really have this in it's
         * API... Oh well. 
         */
    }
    else if (options != 0) {
        return -1;
    }

    if (nofollow) {
        rv = extattr_set_link(path, EXTATTR_NAMESPACE_USER,
                                name, value, size);
    }
    else {
        rv = extattr_set_file(path, EXTATTR_NAMESPACE_USER,
                                name, value, size);
    }

    /* freebsd returns the written length on success, not zero. */
    if (rv >= 0) {
        return 0;
    }
    else {
        return rv;
    }
}

static ssize_t xattr_removexattr(const char *path, const char *name,
                                 int options)
{
    if (!(options == 0 ||
          options == XATTR_XATTR_NOFOLLOW)) {
        return -1;
    }

    if (options & XATTR_XATTR_NOFOLLOW) {
        return extattr_delete_link(path, EXTATTR_NAMESPACE_USER, name);
    }
    else {
        return extattr_delete_file(path, EXTATTR_NAMESPACE_USER, name);
    }
}


static ssize_t xattr_listxattr(const char *path, char *namebuf,
                               size_t size, int options)
{
    ssize_t rv = 0;
    if (!(options == 0 ||
          options == XATTR_XATTR_NOFOLLOW)) {
        return -1;
    }

    if (options & XATTR_XATTR_NOFOLLOW) {
        rv = extattr_list_link(path, EXTATTR_NAMESPACE_USER, namebuf, size);
    }
    else {
        rv = extattr_list_file(path, EXTATTR_NAMESPACE_USER, namebuf, size);
    }

    if (rv > 0 && namebuf) {
        convert_bsd_list(namebuf, rv);
    }

    return rv;
}

static ssize_t xattr_fgetxattr(int fd, const char *name, void *value,
                               ssize_t size, u_int32_t position, int options)
{
    if (position != 0 ||
        !(options == 0 || options == XATTR_XATTR_NOFOLLOW)) {
        return -1;
    }

    if (options & XATTR_XATTR_NOFOLLOW) {
        return -1;
    }
    else {
        return extattr_get_fd(fd, EXTATTR_NAMESPACE_USER, name, value, size);
    }
}

static ssize_t xattr_fsetxattr(int fd, const char *name, void *value,
                               ssize_t size, u_int32_t position, int options)
{
    int rv = 0;
    int nofollow;

    if (position != 0) {
        return -1;
    }

    nofollow = options & XATTR_XATTR_NOFOLLOW;
    options &= ~XATTR_XATTR_NOFOLLOW;

    if (options == XATTR_XATTR_CREATE ||
        options == XATTR_XATTR_REPLACE) {
        /* freebsd noop */
    }
    else if (options != 0) {
        return -1;
    }

    if (nofollow) {
        return -1;
    }
    else {
        rv = extattr_set_fd(fd, EXTATTR_NAMESPACE_USER, 
                            name, value, size);
    }

    /* freebsd returns the written length on success, not zero. */
    if (rv >= 0) {
        return 0;
    }
    else {
        return rv;
    }
}

static ssize_t xattr_fremovexattr(int fd, const char *name, int options)
{

    if (!(options == 0 ||
          options == XATTR_XATTR_NOFOLLOW)) {
        return -1;
    }

    if (options & XATTR_XATTR_NOFOLLOW) {
        return -1;
    }
    else {
        return extattr_delete_fd(fd, EXTATTR_NAMESPACE_USER, name);
    }
}


static ssize_t xattr_flistxattr(int fd, char *namebuf, size_t size, int options)
{
    ssize_t rv = 0;

    if (!(options == 0 ||
          options == XATTR_XATTR_NOFOLLOW)) {
        return -1;
    }

    if (options & XATTR_XATTR_NOFOLLOW) {
        return -1;
    }
    else {
        rv = extattr_list_fd(fd, EXTATTR_NAMESPACE_USER, namebuf, size);
    }

    if (rv > 0 && namebuf) {
        convert_bsd_list(namebuf, rv);
    }

    return rv;
}

#elif defined(__SUN__) || defined(__sun__)

/* Solaris 9 and later compatibility API */
#define XATTR_XATTR_NOFOLLOW 0x0001
#define XATTR_XATTR_CREATE 0x0002
#define XATTR_XATTR_REPLACE 0x0004
#define XATTR_XATTR_NOSECURITY 0x0008

#define u_int32_t unsigned int

static ssize_t xattr_fgetxattr(int fd, const char *name, void *value,
                               ssize_t size, u_int32_t position, int options)
{
    int xfd;
    ssize_t bytes;
    struct stat statbuf;

    /* XXX should check that name does not have / characters in it */
    xfd = openat(fd, name, O_RDONLY | O_XATTR);
    if (xfd == -1) {
	return -1;
    }
    if (lseek(xfd, position, SEEK_SET) == -1) {
	close(xfd);
	return -1;
    }
    if (value == NULL) {
        if (fstat(xfd, &statbuf) == -1) {
	    close(xfd);
	    return -1;
        }
	close(xfd);
	return statbuf.st_size;
    }
    /* XXX should keep reading until the buffer is exhausted or EOF */
    bytes = read(xfd, value, size);
    close(xfd);
    return bytes;
}

static ssize_t xattr_getxattr(const char *path, const char *name,
                              void *value, ssize_t size, u_int32_t position, 
                              int options)
{
    int fd;
    ssize_t bytes;

    if (position != 0 || 
        !(options == 0 || options == XATTR_XATTR_NOFOLLOW)) {
        return -1;
    }

    fd = open(path,
	      O_RDONLY |
	      ((options & XATTR_XATTR_NOFOLLOW) ? O_NOFOLLOW : 0));
    if (fd == -1) {
	return -1;
    }
    bytes = xattr_fgetxattr(fd, name, value, size, position, options);
    close(fd);
    return bytes;
}

static ssize_t xattr_fsetxattr(int fd, const char *name, void *value,
                               ssize_t size, u_int32_t position, int options)
{
    int xfd;
    ssize_t bytes = 0;

    /* XXX should check that name does not have / characters in it */
    xfd = openat(fd, name, O_XATTR | O_TRUNC |
		 ((options & XATTR_XATTR_CREATE) ? O_EXCL : 0) |
		 ((options & XATTR_XATTR_NOFOLLOW) ? O_NOFOLLOW : 0) |
		 ((options & XATTR_XATTR_REPLACE) ? O_RDWR : O_WRONLY|O_CREAT),
		 0644);
    if (xfd == -1) {
	return -1;
    }
    while (size > 0) {
	bytes = write(xfd, value, size);
	if (bytes == -1) {
	    close(xfd);
	    return -1;
	}
	size -= bytes;
	value += bytes;
    }
    close(xfd);
    return 0;
}

static ssize_t xattr_setxattr(const char *path, const char *name,
                              void *value, ssize_t size, u_int32_t position,
                              int options)
{
    int fd;
    ssize_t bytes;

    if (position != 0) {
        return -1;
    }

    fd = open(path,
	      O_RDONLY | (options & XATTR_XATTR_NOFOLLOW) ? O_NOFOLLOW : 0);
    if (fd == -1) {
	return -1;
    }
    bytes = xattr_fsetxattr(fd, name, value, size, position, options);
    close(fd);
    return bytes;
}

static ssize_t xattr_fremovexattr(int fd, const char *name, int options)
{
  int xfd, status;
    /* XXX should check that name does not have / characters in it */
    if (!(options == 0 || options == XATTR_XATTR_NOFOLLOW)) {
        return -1;
    }
    if (options & XATTR_XATTR_NOFOLLOW) {
        return -1;
    }
    xfd = openat(fd, ".", O_XATTR, 0644);
    if (xfd == -1) {
	return -1;
    }
    status = unlinkat(xfd, name, 0);
    close(xfd);
    return status;
}

static ssize_t xattr_removexattr(const char *path, const char *name,
                                 int options)
{
    int fd;
    ssize_t status;

    fd = open(path,
	      O_RDONLY | ((options & XATTR_XATTR_NOFOLLOW) ? O_NOFOLLOW : 0));
    if (fd == -1) {
	return -1;
    }
    status =  xattr_fremovexattr(fd, name, options);
    close(fd);
    return status;
}

static ssize_t xattr_xflistxattr(int xfd, char *namebuf, size_t size, int options)
{
    int esize;
    DIR *dirp;
    struct dirent *entry;
    ssize_t nsize = 0;

    dirp = fdopendir(xfd);
    if (dirp == NULL) {
        return (-1);
    }
    while (entry = readdir(dirp)) {
        if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0)
            continue;
	    esize = strlen(entry->d_name);
	    if (nsize + esize + 1 <= size) {
            snprintf((char *)(namebuf + nsize), esize + 1,
                    entry->d_name);
    	}
	    nsize += esize + 1; /* +1 for \0 */
    }
    closedir(dirp);
    return nsize;
}    
static ssize_t xattr_flistxattr(int fd, char *namebuf, size_t size, int options)
{
    int xfd;
    
    xfd = openat(fd, ".", O_RDONLY);
    return xattr_xflistxattr(xfd, namebuf, size, options);
}

static ssize_t xattr_listxattr(const char *path, char *namebuf,
                               size_t size, int options)
{
    int xfd;

    xfd = attropen(path, ".", O_RDONLY);
    return xattr_xflistxattr(xfd, namebuf, size, options);
}

#elif !defined(XATTR_NOFOLLOW)
/* Linux compatibility API */
#define XATTR_XATTR_NOFOLLOW 0x0001
#define XATTR_XATTR_CREATE 0x0002
#define XATTR_XATTR_REPLACE 0x0004
#define XATTR_XATTR_NOSECURITY 0x0008
static ssize_t xattr_getxattr(const char *path, const char *name, void *value, ssize_t size, u_int32_t position, int options) {
    if (position != 0 || !(options == 0 || options == XATTR_XATTR_NOFOLLOW)) {
        return -1;
    }
    if (options & XATTR_XATTR_NOFOLLOW) {
        return lgetxattr(path, name, value, size);
    } else {
        return getxattr(path, name, value, size);
    }
}

static ssize_t xattr_setxattr(const char *path, const char *name, void *value, ssize_t size, u_int32_t position, int options) {
    int nofollow;
    if (position != 0) {
        return -1;
    }
    nofollow = options & XATTR_XATTR_NOFOLLOW;
    options &= ~XATTR_XATTR_NOFOLLOW;
    if (options == XATTR_XATTR_CREATE) {
        options = XATTR_CREATE;
    } else if (options == XATTR_XATTR_REPLACE) {
        options = XATTR_REPLACE;
    } else if (options != 0) {
        return -1;
    }
    if (nofollow) {
        return lsetxattr(path, name, value, size, options);
    } else {
        return setxattr(path, name, value, size, options);
    }
}

static ssize_t xattr_removexattr(const char *path, const char *name, int options) {
    if (!(options == 0 || options == XATTR_XATTR_NOFOLLOW)) {
        return -1;
    }
    if (options & XATTR_XATTR_NOFOLLOW) {
        return lremovexattr(path, name);
    } else {
        return removexattr(path, name);
    }
}


static ssize_t xattr_listxattr(const char *path, char *namebuf, size_t size, int options) {
    if (!(options == 0 || options == XATTR_XATTR_NOFOLLOW)) {
        return -1;
    }
    if (options & XATTR_XATTR_NOFOLLOW) {
        return llistxattr(path, namebuf, size);
    } else {
        return listxattr(path, namebuf, size);
    }
}

static ssize_t xattr_fgetxattr(int fd, const char *name, void *value, ssize_t size, u_int32_t position, int options) {
    if (position != 0 || !(options == 0 || options == XATTR_XATTR_NOFOLLOW)) {
        return -1;
    }
    if (options & XATTR_XATTR_NOFOLLOW) {
        return -1;
    } else {
        return fgetxattr(fd, name, value, size);
    }
}

static ssize_t xattr_fsetxattr(int fd, const char *name, void *value, ssize_t size, u_int32_t position, int options) {
    int nofollow;
    if (position != 0) {
        return -1;
    }
    nofollow = options & XATTR_XATTR_NOFOLLOW;
    options &= ~XATTR_XATTR_NOFOLLOW;
    if (options == XATTR_XATTR_CREATE) {
        options = XATTR_CREATE;
    } else if (options == XATTR_XATTR_REPLACE) {
        options = XATTR_REPLACE;
    } else if (options != 0) {
        return -1;
    }
    if (options & XATTR_XATTR_NOFOLLOW) {
        return -1;
    } else {
        return fsetxattr(fd, name, value, size, options);
    }
}

static ssize_t xattr_fremovexattr(int fd, const char *name, int options) {
    if (!(options == 0 || options == XATTR_XATTR_NOFOLLOW)) {
        return -1;
    }
    if (options & XATTR_XATTR_NOFOLLOW) {
        return -1;
    } else {
        return fremovexattr(fd, name);
    }
}


static ssize_t xattr_flistxattr(int fd, char *namebuf, size_t size, int options) {
    if (!(options == 0 || options == XATTR_XATTR_NOFOLLOW)) {
        return -1;
    }
    if (options & XATTR_XATTR_NOFOLLOW) {
        return -1;
    } else {
        return flistxattr(fd, namebuf, size);
    }
}

#else /* Mac OS X assumed */
#define xattr_getxattr getxattr
#define xattr_fgetxattr fgetxattr
#define xattr_removexattr removexattr
#define xattr_fremovexattr fremovexattr
#define xattr_setxattr setxattr
#define xattr_fsetxattr fsetxattr
#define xattr_listxattr listxattr
#define xattr_flistxattr flistxattr
#endif

static PyObject *xattr_error(void);
static PyObject *xattr_error_with_filename(char *name);

static PyObject *
xattr_error(void)
{
    return PyErr_SetFromErrno(PyExc_IOError);
}

static PyObject *
xattr_error_with_filename(char *name)
{
    return PyErr_SetFromErrnoWithFilename(PyExc_IOError, name);
}

PyDoc_STRVAR(pydoc_getxattr,
    "getxattr(path, name, size=0, position=0, options=0) -> str\n"
    "\n"
    "..."
);
static PyObject*
py_getxattr(PyObject* self __attribute__((__unused__)), PyObject *args) /* , PyObject *kwds) */
{
    /* static char *keywords[] = { "path", "name", "size", "position", "options", NULL }; */
    char *path;
    char *name;
    PyObject *buffer;
    int options = 0;
    size_t size = 0;
    u_int32_t position = 0;
    ssize_t res;
    if (!PyArg_ParseTuple(args, /* AndKeywords(args, kwds, */
            "etet|IIi:getxattr", /* keywords, */
            Py_FileSystemDefaultEncoding, &path,
            Py_FileSystemDefaultEncoding, &name,
            &size,
            &position,
            &options)) {
        return NULL;
    }
    if (size == 0) {
        Py_BEGIN_ALLOW_THREADS
        res = xattr_getxattr((const char *)path, (const char *)name, NULL, 0, position, options);
        Py_END_ALLOW_THREADS
        if (res == -1) {    
            PyObject *tmp = xattr_error_with_filename(path);
            PyMem_Free(path);
            PyMem_Free(name);
            return tmp;
        }
        size = res;
    }
    buffer = PyString_FromStringAndSize((char *)NULL, size);
    if (buffer == NULL) {
        PyMem_Free(path);
        PyMem_Free(name);
        return NULL;
    }
    Py_BEGIN_ALLOW_THREADS
    res = xattr_getxattr((const char *)path, (const char *)name, (void *)PyString_AS_STRING(buffer), size, position, options);
    Py_END_ALLOW_THREADS
    if (res == -1) {
        PyObject *tmp = xattr_error_with_filename(path);
        Py_DECREF(buffer);
        PyMem_Free(path);
        PyMem_Free(name);
        return tmp;
    }
    PyMem_Free(path);
    PyMem_Free(name);
    if (res != size) {
        _PyString_Resize(&buffer, (int)res);
    }
    return buffer;
}

PyDoc_STRVAR(pydoc_fgetxattr,
    "fgetxattr(fd, name, size=0, position=0, options=0) -> str\n"
    "\n"
    "..."
);
static PyObject*
py_fgetxattr(PyObject* self __attribute__((__unused__)), PyObject *args) /* , PyObject *kwds) */
{
    /* static char *keywords[] = { "fd", "name", "size", "position", "options", NULL }; */
    int fd;
    char *name;
    PyObject *buffer;
    int options = 0;
    size_t size = 0;
    u_int32_t position = 0;
    ssize_t res;
    if (!PyArg_ParseTuple(args, /* AndKeywords(args, kwds, */
            "iet|IIi:fgetxattr", /* keywords, */
            &fd,
            Py_FileSystemDefaultEncoding, &name,
            &size,
            &position,
            &options)) {
        return NULL;
    }
    if (size == 0) {
        Py_BEGIN_ALLOW_THREADS
        res = xattr_fgetxattr(fd, (const char *)name, NULL, 0, position, options);
        Py_END_ALLOW_THREADS
        if (res == -1) {    
            PyMem_Free(name);
            return xattr_error();
        }
        size = res;
    }
    buffer = PyString_FromStringAndSize((char *)NULL, size);
    if (buffer == NULL) {
        PyMem_Free(name);
        return NULL;
    }
    Py_BEGIN_ALLOW_THREADS
    res = xattr_fgetxattr(fd, (const char *)name, (void *)PyString_AS_STRING(buffer), size, position, options);
    Py_END_ALLOW_THREADS
    PyMem_Free(name);
    if (res == -1) {
        Py_DECREF(buffer);
        return xattr_error();
    }
    if (res != size) {
        _PyString_Resize(&buffer, (int)res);
    }
    return buffer;
}

PyDoc_STRVAR(pydoc_setxattr,
    "setxattr(path, name, value, position=0, options=0) -> None\n"
    "\n"
    "..."
);
static PyObject*
py_setxattr(PyObject* self __attribute__((__unused__)), PyObject *args) /* , PyObject *kwds) */
{
    /* static char *keywords[] = { "path", "name", "value", "position", "options", NULL }; */
    PyObject *result;
    char *path;
    char *name;
    int options = 0;
    char *value;
    int size;
    u_int32_t position = 0;
    int res;
    if (!PyArg_ParseTuple(args, /* AndKeywords(args, kwds, */
            "etets#|Ii:setxattr", /* keywords, */
            Py_FileSystemDefaultEncoding, &path,
            Py_FileSystemDefaultEncoding, &name,
            &value, &size,
            &position,
            &options)) {
        return NULL;
    }
    Py_BEGIN_ALLOW_THREADS
    res = xattr_setxattr((const char *)path, (const char *)name, (void *)value, size, position, options);
    Py_END_ALLOW_THREADS
    if (res) {
        result = xattr_error_with_filename(path);
    } else {
        Py_INCREF(Py_None);
        result = Py_None;
    }
    PyMem_Free(path);
    PyMem_Free(name);
    return result;
}

PyDoc_STRVAR(pydoc_fsetxattr,
    "fsetxattr(fd, name, value, position=0, options=0) -> None\n"
    "\n"
    "..."
);
static PyObject*
py_fsetxattr(PyObject* self __attribute__((__unused__)), PyObject *args) /* , PyObject *kwds) */
{
    /* static char *keywords[] = { "fd", "name", "value", "position", "options", NULL }; */
    int fd;
    char *name;
    int options = 0;
    char *value;
    int size;
    u_int32_t position = 0;
    int res;
    if (!PyArg_ParseTuple(args, /* AndKeywords(args, kwds, */
            "iets#|Ii:fsetxattr", /* keywords, */
            &fd,
            Py_FileSystemDefaultEncoding, &name,
            &value, &size,
            &position,
            &options)) {
        return NULL;
    }
    Py_BEGIN_ALLOW_THREADS
    res = xattr_fsetxattr(fd, (const char *)name, (void *)value, size, position, options);
    Py_END_ALLOW_THREADS
    PyMem_Free(name);
    if (res) {
        return xattr_error();
    }
    Py_INCREF(Py_None);
    return Py_None;
}

PyDoc_STRVAR(pydoc_removexattr,
    "removexattr(path, name, options=0) -> None\n"
    "\n"
    "..."
);
static PyObject*
py_removexattr(PyObject* self __attribute__((__unused__)), PyObject *args) /* , PyObject *kwds) */
{
    /* static char *keywords[] = { "path", "name", "options", NULL }; */
    char *path;
    char *name;
    int options = 0;
    int res;
    PyObject *result;
    if (!PyArg_ParseTuple(args, /* AndKeywords(args, kwds, */
            "etet|i:removexattr", /* keywords, */
            Py_FileSystemDefaultEncoding, &path,
            Py_FileSystemDefaultEncoding, &name,
            &options)) {
        return NULL;
    }
    Py_BEGIN_ALLOW_THREADS
    res = xattr_removexattr((const char *)path, (const char *)name, options);
    Py_END_ALLOW_THREADS
    if (res) {
        result = xattr_error_with_filename(path);
    } else {
        Py_INCREF(Py_None);
        result = Py_None;
    }
    PyMem_Free(path);
    PyMem_Free(name);
    return result;
}

PyDoc_STRVAR(pydoc_fremovexattr,
    "fremovexattr(fd, name, options=0) -> None\n"
    "\n"
    "..."
);
static PyObject*
py_fremovexattr(PyObject* self __attribute__((__unused__)), PyObject *args) /* , PyObject *kwds) */
{
    /* static char *keywords[] = { "fd", "name", "options", NULL }; */
    int fd;
    char *name;
    int options = 0;
    int res;
    if (!PyArg_ParseTuple(args, /* AndKeywords(args, kwds, */
            "iet|i:fremovexattr", /* keywords, */
            &fd,
            Py_FileSystemDefaultEncoding, &name,
            &options)) {
        return NULL;
    }
    Py_BEGIN_ALLOW_THREADS
    res = xattr_fremovexattr(fd, (const char *)name, options);
    Py_END_ALLOW_THREADS
    PyMem_Free(name);
    if (res) {
        return xattr_error();
    }
    Py_INCREF(Py_None);
    return Py_None;
}

PyDoc_STRVAR(pydoc_listxattr,
    "listxattr(path, options=0) -> str\n"
    "\n"
    "..."
);
static PyObject*
py_listxattr(PyObject* self __attribute__((__unused__)), PyObject *args) /* , PyObject *kwds) */
{
    /* static char *keywords[] = { "path", "options", NULL }; */
    PyObject *buffer;
    char *path;
    int options = 0;
    ssize_t res;
    if (!PyArg_ParseTuple(args, /* AndKeywords(args, kwds, */
            "et|i:listxattr", /* keywords, */
            Py_FileSystemDefaultEncoding, &path,
            &options)) {
        return NULL;
    }
    Py_BEGIN_ALLOW_THREADS
    res = xattr_listxattr((const char *)path, NULL, 0, options);
    Py_END_ALLOW_THREADS
    if (res == -1) {    
        PyObject *tmp = xattr_error_with_filename(path);
        PyMem_Free(path);
        return tmp;
    }
    buffer = PyString_FromStringAndSize((char *)NULL, (int)res);
    if (buffer == NULL) {
        PyMem_Free(path);
        return NULL;
    }
    /* avoid 2nd listxattr call if the first one returns 0 */
    if (res == 0) {
        PyMem_Free(path);
        return buffer;
    }
    Py_BEGIN_ALLOW_THREADS
    res = xattr_listxattr((const char *)path, (void *)PyString_AS_STRING(buffer), (size_t)PyString_GET_SIZE(buffer), options);
    Py_END_ALLOW_THREADS
    if (res == -1) {
        PyObject *tmp = xattr_error_with_filename(path);
        Py_DECREF(buffer);
        PyMem_Free(path);
        return tmp;
    }
    PyMem_Free(path);
    if (res != (ssize_t)PyString_GET_SIZE(buffer)) {
        _PyString_Resize(&buffer, (int)res);
    }
    return buffer;
}

PyDoc_STRVAR(pydoc_flistxattr,
    "flistxattr(fd, options=0) -> str\n"
    "\n"
    "..."
);
static PyObject*
py_flistxattr(PyObject* self __attribute__((__unused__)), PyObject *args) /* , PyObject *kwds) */
{
    /* static char *keywords[] = { "fd", "options", NULL }; */
    PyObject *buffer;
    int fd;
    int options = 0;
    ssize_t res;
    if (!PyArg_ParseTuple(args, /* AndKeywords(args, kwds, */
            "i|i:flistxattr", /* keywords, */
            &fd,
            &options)) {
        return NULL;
    }
    Py_BEGIN_ALLOW_THREADS
    res = xattr_flistxattr(fd, NULL, 0, options);
    Py_END_ALLOW_THREADS
    if (res == -1) {    
        return xattr_error();
    }
    buffer = PyString_FromStringAndSize((char *)NULL, (int)res);
    if (buffer == NULL) {
        return NULL;
    }
    Py_BEGIN_ALLOW_THREADS
    res = xattr_flistxattr(fd, (void *)PyString_AS_STRING(buffer), (size_t)PyString_GET_SIZE(buffer), options);
    Py_END_ALLOW_THREADS
    if (res == -1) {
        Py_DECREF(buffer);
        return xattr_error();
    }
    if (res != (ssize_t)PyString_GET_SIZE(buffer)) {
        _PyString_Resize(&buffer, (int)res);
    }
    return buffer;
}


#define DEFN(n) \
    {  \
        #n, \
        (PyCFunction)py_ ##n, \
        METH_VARARGS /* | METH_KEYWORDS */, \
        pydoc_ ##n \
    }
static PyMethodDef xattr_methods[] = {
    DEFN(getxattr),
    DEFN(fgetxattr),
    DEFN(setxattr),
    DEFN(fsetxattr),
    DEFN(removexattr),
    DEFN(fremovexattr),
    DEFN(listxattr),
    DEFN(flistxattr),
    {}
};
#undef DEFN

void init_xattr(void);

void
init_xattr(void)
{
    PyObject *m;
    m = Py_InitModule4("_xattr", xattr_methods, NULL, NULL, PYTHON_API_VERSION);
}
