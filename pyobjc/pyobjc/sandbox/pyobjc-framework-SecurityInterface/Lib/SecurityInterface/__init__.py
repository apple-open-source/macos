'''
Python mapping for the SecurityInterface framework.

This module does not contain docstrings for the wrapped code, check Apple's
documentation for details on how to use these functions and classes. 
'''

import objc as _objc
from AppKit import *
#import Security
#import SecurityFoundation

__bundle__ = _objc.initFrameworkWrapper("SecurityInterface",
    frameworkIdentifier="com.apple.securityinterface",
    frameworkPath=_objc.pathForFramework(
        "/System/Library/Frameworks/SecurityInterface.framework"),
    globals=globals())
