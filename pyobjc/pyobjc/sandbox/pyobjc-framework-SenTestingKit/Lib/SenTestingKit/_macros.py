"""
Emulation for the assertion macros in SenTestingKit
"""
import sys
from Foundation import NSException
from SenTestingKit import STComposeString

# Some stack-inspecting helper functions, tuned for being called from other
# functions and returning info about the caller of those functions.
def _getSelf():
    return sys._getframe(2).f_locals['self']


def __LINE__():
    return sys._getframe(2).f_lineno

def __FILE__():
    return sys._getframe(2).f_code.co_filename


def STAssertEqualObjects(a1, a2, description, *args):
    try:
        if a1 != a2:
            _getSelf().failWithException_(
                    NSException.failureInEqualityBetweenObject_andObject_inFile_atLine_withDescription_(
                        a1, a2, __FILE__(), __LINE__(), STComposeString(description, *args)))
    except Exception, e:
        e = NSException.exceptionWithName_reason_userInfo_(
                e._pyobjc_info_['name'],
                e._pyobjc_info_['reason'],
                e._pyobjc_info_['userInfo'])
        _getSelf().failWithException_(
                NSException.failureInRaise_exception_inFile_atLine_withDescription_(
                    "(%s) == (%s)"%(a1, a2),
                    e,
                    __FILE__(),
                    __LINE__(),
                    STComposeString(description, *args)))

def STAssertEqual(a1, a2, description, *args):
    try:
        if a1 != a2:
            _getSelf().failWithException_(
                    NSException.failureInEqualityBetweenValue_andValue_inFile_atLine_withDescription_(
                        a1, a2, __FILE__(), __LINE__(), STComposeString(description, *args)))
    except Exception, e:
        e = NSException.exceptionWithName_reason_userInfo_(
                e._pyobjc_info_['name'],
                e._pyobjc_info_['reason'],
                e._pyobjc_info_['userInfo'])
        _getSelf().failWithException_(
                NSException.failureInRaise_exception_inFile_atLine_withDescription_(
                    "(%s) == (%s)"%(a1, a2),
                    e,
                    __FILE__(),
                    __LINE__(),
                    STComposeString(description, *args)))


def STAbsoluteDifference(left, right):
    return max(left, right) - min(lef, right)

def STAssertEqualsWithAccuracy(a1, a2, accuracy, description, *args):
    try:
        if STAbsoluteDifference(a1, a2) > accuracy:
            _getSelf().failWithException_(
                    NSException.failureInEqualityBetweenValue_andValue_withAccuracy_inFile_atLine_withDescription_(
                        a1, a2, accuracy__FILE__(), __LINE__(), STComposeString(description, *args)))
    except Exception, e:
        e = NSException.exceptionWithName_reason_userInfo_(
                e._pyobjc_info_['name'],
                e._pyobjc_info_['reason'],
                e._pyobjc_info_['userInfo'])
        _getSelf().failWithException_(
                NSException.failureInRaise_exception_inFile_atLine_withDescription_(
                    "(%s) == (%s) with accuracy %s"%(a1, a2, accuracy),
                    e,
                    __FILE__(),
                    __LINE__(),
                    STComposeString(description, *args)))

def STFail(description, *args):
    _getSelf().failWithException_(
            NSException.failureInFile_atLine_withDescription_(
                __FILE__(),
                __LINE__(),
                STComposeString(description, *args)))

def STAssertNil(a1, description, *args):
    if a1 != None:
        _getSelf().failWithException_(
                NSException.failureInCondition_isTrue_inFile_atLine_withDescrpiton_(
                    '(%s) is None'%(a1,), False, __FILE__(), __LINE__(), STComposeString(description, *args)))
                    

def STAssertNotNil(a1, description, *args):
    if a1 == None:
        _getSelf().failWithException_(
                NSException.failureInCondition_isTrue_inFile_atLine_withDescrpiton_(
                    '(%s) is not None'%(a1,), False, __FILE__(), __LINE__(), STComposeString(description, *args)))

def STAssertTrue(a1, description, *args):
    if not bool(a1):
        _getSelf().failWithException_(
                NSException.failureInCondition_isTrue_inFile_atLine_withDescrpiton_(
                    'bool(%s)'%(a1,), False, __FILE__(), __LINE__(), STComposeString(description, *args)))

def STAssertFalse(a1, description, *args):
    if bool(a1):
        _getSelf().failWithException_(
                NSException.failureInCondition_isTrue_inFile_atLine_withDescrpiton_(
                    'not bool(%s)'%(a1,), False, __FILE__(), __LINE__(), STComposeString(description, *args)))


def STAssertTrueNoThrow(*args):
    raise RuntimeError("STAsssertTrueNoThrow not supported")

def STAssertFalseNoThrow(*args):
    raise RuntimeError("STAsssertFalseNoThrow not supported")

def STAssertThrow(*args):
    raise RuntimeError("STAsssertThrow not supported")

def STAssertThrowsSpecific(*args):
    raise RuntimeError("STAssertThrowsSpecific not supported")

def STAssertThrowsSpecificNamed(*args):
    raise RuntimeError("STAssertThrowsSpecificNamed not supported")

def STAssertNoThrow(*args):
    raise RuntimeError("STAssertNoThrow not supported")

def STAssertNoThrowSpecific(*args):
    raise RuntimeError("STAssertNoThrowSpecific not supported")

def STAssertNoThrowSpecificNamed(*args):
    raise RuntimeError("STAssertNoThrowSpecificNamed not supported")
