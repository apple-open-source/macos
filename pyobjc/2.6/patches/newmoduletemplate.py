'''
Python mapping for the @XXX@ framework.

This module does not contain docstrings for the wrapped code, check Apple's
documentation for details on how to use these functions and classes. 
'''

import objc as _objc

__bundle__ = _objc.initFrameworkWrapper("@XXX@",
    frameworkIdentifier="com.apple.@XXX@",
    frameworkPath=_objc.pathForFramework(
        "/System/Library/Frameworks/@XXX@.framework"),
    globals=globals())
