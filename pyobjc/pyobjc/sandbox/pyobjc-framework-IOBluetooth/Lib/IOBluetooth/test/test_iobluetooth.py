'''
Some simple tests to check that the framework is properly wrapped.
'''
import objc
import unittest
import IOBluetooth

class TestIOBluetooth (unittest.TestCase):
    def testClasses(self):
        pass
        # self.assert_( hasattr(IOBluetooth, 'CLASSNAME') )
        # self.assert_( isinstance(IOBluetooth.CLASSNAME, objc.objc_class) )
        # Tollfree CF-type:
        # self.assert_( hasattr(IOBluetooth, 'CLASSNAMERef') )
        # self.assert_( IOBluetooth.CLASSNAMERef is IOBluetooth.CLASSNAME )

        # Not-tollfree CF-type:
        # self.assert_( hasattr(IOBluetooth, 'CLASSNAMERef') )
        # self.assert_( issubclass(IOBluetooth.CLASSNAMERef, objc.lookUpClass('NSCFType')) )
        # self.assert_( IOBluetooth.CLASSNAMERef is not objc.lookUpClass('NSCFType') )

    def testValues(self):
        # Use this to test for a number of enum and #define values
        pass

        # Integer values:
        # self.assert_( hasattr(IOBluetooth, 'CONSTANT') )
        # self.assert_( isinstance(IOBluetooth.CONSTANT, (int, long)) )
        # self.assertEquals(IOBluetooth.CONSTANT, 7)

        # String values:
        # self.assert_( hasattr(IOBluetooth, 'CONSTANT') )
        # self.assert_( isinstance(IOBluetooth.CONSTANT, (str, unicode)) )
        # self.assertEquals(IOBluetooth.CONSTANT, 'value')

    def testVariables(self):
        # Use this to test for global variables, (NSString*'s and the like)
        pass

        # self.assert_( hasattr(IOBluetooth, 'CONSTANT') )
        # self.assert_( isinstance(IOBluetooth.CONSTANT, unicode) )

    def testFunctions(self):
        # Use this to test for functions
        pass

        # self.assert_( hasattr(IOBluetooth, 'FUNCTION') )

    def testOpaque(self):
        # Use this to test for opaque pointers
        pass

        # self.assert_( hasattr(IOBluetooth, 'OPAQUE') )

    def testProtocols(self):
        # Use this to test if informal protocols  are present
        pass

        # self.assert_( hasattr(IOBluetooth, 'protocols') )

        # self.assert_( hasattr(IOBluetooth.protocols, 'PROTOCOL') )
        # self.assert_( isinstance(IOBluetooth.protocols.PROTOCOL, objc.informal_protocol) )

    def test_structs(self):
        # Use this to test struct wrappers
        pass

        # self.assert_( hasattr(IOBluetooth, 'STRUCT') )
        # o = IOBluetooth.STRUCT()
        # self.assert_( hasattr(o, 'FIELD_NAME') )



if __name__ == "__main__":
    unittest.main()

