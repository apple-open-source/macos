'''
Python mapping for the IOBluetooth framework.

This module does not contain docstrings for the wrapped code, check Apple's
documentation for details on how to use these functions and classes. 
'''

import objc as _objc
from CoreFoundation import *
from Foundation import *
from IOKit import *

__bundle__ = _objc.initFrameworkWrapper("IOBluetooth",
    frameworkIdentifier="com.apple.Bluetooth",
    frameworkPath=_objc.pathForFramework(
        "/System/Library/Frameworks/IOBluetooth.framework"),
    globals=globals())
