'''
Some simple tests to check that the framework is properly wrapped.
'''
import objc
import unittest
import SecurityInterface

class TestSecurityInterface (unittest.TestCase):
    def testClasses(self):
        pass
        # self.assert_( hasattr(SecurityInterface, 'CLASSNAME') )
        # self.assert_( isinstance(SecurityInterface.CLASSNAME, objc.objc_class) )
        # Tollfree CF-type:
        # self.assert_( hasattr(SecurityInterface, 'CLASSNAMERef') )
        # self.assert_( SecurityInterface.CLASSNAMERef is SecurityInterface.CLASSNAME )

        # Not-tollfree CF-type:
        # self.assert_( hasattr(SecurityInterface, 'CLASSNAMERef') )
        # self.assert_( issubclass(SecurityInterface.CLASSNAMERef, objc.lookUpClass('NSCFType')) )
        # self.assert_( SecurityInterface.CLASSNAMERef is not objc.lookUpClass('NSCFType') )

    def testValues(self):
        # Use this to test for a number of enum and #define values
        pass

        # Integer values:
        # self.assert_( hasattr(SecurityInterface, 'CONSTANT') )
        # self.assert_( isinstance(SecurityInterface.CONSTANT, (int, long)) )
        # self.assertEquals(SecurityInterface.CONSTANT, 7)

        # String values:
        # self.assert_( hasattr(SecurityInterface, 'CONSTANT') )
        # self.assert_( isinstance(SecurityInterface.CONSTANT, (str, unicode)) )
        # self.assertEquals(SecurityInterface.CONSTANT, 'value')

    def testVariables(self):
        # Use this to test for global variables, (NSString*'s and the like)
        pass

        # self.assert_( hasattr(SecurityInterface, 'CONSTANT') )
        # self.assert_( isinstance(SecurityInterface.CONSTANT, unicode) )

    def testFunctions(self):
        # Use this to test for functions
        pass

        # self.assert_( hasattr(SecurityInterface, 'FUNCTION') )

    def testOpaque(self):
        # Use this to test for opaque pointers
        pass

        # self.assert_( hasattr(SecurityInterface, 'OPAQUE') )

    def testProtocols(self):
        # Use this to test if informal protocols  are present
        pass

        # self.assert_( hasattr(SecurityInterface, 'protocols') )

        # self.assert_( hasattr(SecurityInterface.protocols, 'PROTOCOL') )
        # self.assert_( isinstance(SecurityInterface.protocols.PROTOCOL, objc.informal_protocol) )

    def test_structs(self):
        # Use this to test struct wrappers
        pass

        # self.assert_( hasattr(SecurityInterface, 'STRUCT') )
        # o = SecurityInterface.STRUCT()
        # self.assert_( hasattr(o, 'FIELD_NAME') )



if __name__ == "__main__":
    unittest.main()

