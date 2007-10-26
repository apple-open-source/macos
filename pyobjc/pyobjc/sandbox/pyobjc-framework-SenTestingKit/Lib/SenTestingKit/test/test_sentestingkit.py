'''
Some simple tests to check that the framework is properly wrapped.
'''
import objc
import unittest
import SenTestingKit

class TestSenTestingKit (unittest.TestCase):
    def testClasses(self):
        self.assert_( hasattr(SenTestingKit, 'SenTestSuite') )
        self.assert_( isinstance(SenTestingKit.SenTestSuite, objc.objc_class) )
        self.assert_( hasattr(SenTestingKit, 'SenTestCase') )
        self.assert_( isinstance(SenTestingKit.SenTestCase, objc.objc_class) )

    def testVariables(self):
        self.assert_( hasattr(SenTestingKit, 'SenTestFailureException') )
        self.assert_( isinstance(SenTestingKit.SenTestFailureException, unicode) )
        self.assert_( hasattr(SenTestingKit, 'SenTestEqualityLeftKey') )
        self.assert_( isinstance(SenTestingKit.SenTestEqualityLeftKey, unicode) )

        self.assert_( hasattr(SenTestingKit, 'SenTestCaseDidStartNotification') )
        self.assert_( isinstance(SenTestingKit.SenTestCaseDidStartNotification, unicode) )
        self.assert_( hasattr(SenTestingKit, 'SenTestScopeNone') )
        self.assert_( isinstance(SenTestingKit.SenTestScopeNone, unicode) )

    def testFunctions(self):
        self.assert_( hasattr(SenTestingKit, 'STComposeString') )

        # For some reason this function isn't actually present in the framework:
        #self.assert_( hasattr(SenTestingKit, 'getScalarDescription') )

        self.assertEquals(SenTestingKit.STComposeString("%s %d", "hello", 39), "hello 39")


    def testSTComposeString(self):
        v = SenTestingKit.STComposeString("hello %d.", 42)
        self.assertEquals(v, "hello 42.")

class TestTestCase (unittest.TestCase):

    def testSimpleTestCase(self):
        class OCSenTest (SenTestingKit.SenTestCase):
            def testObjectsSamePass(self):
                SenTestingKit.STAssertEqualObjects("a", "a", "foo %s", "bar")

            def testObjectsSameFail(self):
                SenTestingKit.STAssertEqualObjects("a", "b", "foo %s", "bar")


        o = OCSenTest.alloc().init()
        o.raiseAfterFailure()
        o.testObjectsSamePass()

        try:
            o.testObjectsSameFail()

        except Exception, e:
            self.assertEquals(e._pyobjc_info_['name'], 'SenTestFailureException')

        # FIXME: add tests for the other macros as well.

if __name__ == "__main__":
    unittest.main()

