'''
Python mapping for the SenTestingKit framework.

This module does not contain docstrings for the wrapped code, check Apple's
documentation for details on how to use these functions and classes. 
'''

import objc as _objc
from Foundation import *

__bundle__ = _objc.initFrameworkWrapper("SenTestingKit",
    frameworkIdentifier="ch.sente.SenTestingKit",
    frameworkPath=_objc.pathForFramework(
        "/System/Library/Frameworks/SenTestingKit.framework"),
    globals=globals())

# Emulation for some of the assertion macros
from _macros import *
