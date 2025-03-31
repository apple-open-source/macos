"""
Extended attributes extend the basic attributes of files and directories
in the file system.  They are stored as name:data pairs associated with
file system objects (files, directories, symlinks, etc).

The xattr type wraps a path or file descriptor with a dict-like interface
that exposes these extended attributes.
"""

__version__ = '0.6.4'
from constants import XATTR_NOFOLLOW, XATTR_CREATE, XATTR_REPLACE, \
    XATTR_NOSECURITY, XATTR_MAXNAMELEN, XATTR_FINDERINFO_NAME, \
    XATTR_RESOURCEFORK_NAME

import _xattr

def _pyflakes_api():
    # trick pyflakes into thinking these are used.
    return [
        XATTR_NOFOLLOW, XATTR_CREATE, XATTR_REPLACE,
        XATTR_NOSECURITY, XATTR_MAXNAMELEN, XATTR_FINDERINFO_NAME,
        XATTR_RESOURCEFORK_NAME,
    ]


def _boundfunc(func, first):
    def _func(*args):
        return func(first, *args)
    return _func

class xattr(object):
    """
    A wrapper for paths or file descriptors to access
    their extended attributes with a dict-like interface
    """

    def __init__(self, obj, options=0):
        """
        obj should be a path, a file descriptor, or an
        object that implements fileno() and returns a file
        descriptor.

        options should be 0 or XATTR_NOFOLLOW.  If set, it will
        be OR'ed with the options passed to getxattr, setxattr, etc.
        """
        self.obj = obj
        self.options = options
        self.flavor = None
        fileno = getattr(obj, 'fileno', None)
        if fileno is not None:
            obj = fileno()
        if isinstance(obj, int):
            self.flavor = 'fd'
            self._bind_any('f%sxattr', obj, options)
        else:
            self.flavor = 'file'
            self._bind_any('%sxattr', obj, options)

    def __repr__(self):
        if self.flavor:
            return '<%s %s=%r>' % (type(self).__name__, self.flavor, self.obj)
        else:
            return object.__repr__(self)

    def _bind_any(self, fmt, obj, options):
        options = self.options
        for method in ("get", "set", "remove", "list"):
            name = '_' + method
            func = getattr(_xattr, fmt % (method,))
            meth = _boundfunc(func, obj)
            try:
                meth.__name__ = name
            except TypeError:
                pass
            setattr(self, name, meth)

    def get(self, name, options=0):
        """
        Retrieve the extended attribute ``name`` as a ``str``.
        Raises ``IOError`` on failure.

        See x-man-page://2/getxattr for options and possible errors.
        """
        return self._get(name, 0, 0, options | self.options)

    def set(self, name, value, options=0):
        """
        Set the extended attribute ``name`` to ``value``
        Raises ``IOError`` on failure.

        See x-man-page://2/setxattr for options and possible errors.
        """
        self._set(name, value, 0, options | self.options)

    def remove(self, name, options=0):
        """
        Remove the extended attribute ``name``
        Raises ``IOError`` on failure.

        See x-man-page://2/removexattr for options and possible errors.
        """
        self._remove(name, options | self.options)

    def list(self, options=0):
        """
        Retrieves the extended attributes currently set as a list
        of unicode strings.  Raises ``IOError`` on failure.

        See x-man-page://2/listxattr for options and possible errors.
        """
        res = self._list(options | self.options).split('\x00')
        res.pop()
        return [unicode(s, 'utf-8') for s in res]

    # dict-like methods

    def __len__(self):
        return len(self.list())

    def __delitem__(self, item):
        try:
            self.remove(item)
        except IOError:
            raise KeyError(item)

    def __setitem__(self, item, value):
        self.set(item, value)

    def __getitem__(self, item):
        try:
            return self.get(item)
        except IOError:
            raise KeyError(item)

    def iterkeys(self):
        return iter(self.list())

    __iter__ = iterkeys

    def has_key(self, item):
        try:
            self.get(item)
        except IOError:
            return False
        else:
            return True

    __contains__ = has_key

    def clear(self):
        for k in self.keys():
            del self[k]

    def update(self, seq):
        if not hasattr(seq, 'iteritems'):
            seq = dict(seq)
        for k, v in seq.iteritems():
            self[k] = v

    def copy(self):
        return dict(self.iteritems())

    def setdefault(self, k, d=''):
        try:
            d = self.get(k)
        except IOError:
            self[k] = d
        return d

    def keys(self):
        return self.list()

    def itervalues(self):
        for k, v in self.iteritems():
            yield v

    def values(self):
        return list(self.itervalues())

    def iteritems(self):
        for k in self.list():
            yield k, self.get(k)

    def items(self):
        return list(self.iteritems())


def listxattr(f, symlink=False):
    __doc__ = xattr.list.__doc__
    return tuple(xattr(f).list(options=symlink and XATTR_NOFOLLOW or 0))

def getxattr(f, attr, symlink=False):
    __doc__ = xattr.get.__doc__
    return xattr(f).get(attr, options=symlink and XATTR_NOFOLLOW or 0)

def setxattr(f, attr, value, options=0, symlink=False):
    __doc__ = xattr.set.__doc__
    if symlink:
        options |= XATTR_NOFOLLOW
    return xattr(f).set(attr, value, options=options)

def removexattr(f, attr, symlink=False):
    __doc__ = xattr.remove.__doc__
    options = symlink and XATTR_NOFOLLOW or 0
    return xattr(f).remove(attr, options=options)
