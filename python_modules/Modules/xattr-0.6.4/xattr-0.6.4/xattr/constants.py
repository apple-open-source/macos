
# Options for pathname based xattr calls
XATTR_NOFOLLOW = 0x0001 # Don't follow symbolic links

# Options for setxattr calls
XATTR_CREATE = 0x0002 # set the value, fail if attr already exists
XATTR_REPLACE = 0x0004 # set the value, fail if attr does not exist

# Set this to bypass authorization checking (eg. if doing auth-related work)
XATTR_NOSECURITY = 0x0008

XATTR_MAXNAMELEN = 127

XATTR_FINDERINFO_NAME = "com.apple.FinderInfo"

XATTR_RESOURCEFORK_NAME = "com.apple.ResourceFork"
