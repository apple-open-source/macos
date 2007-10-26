'''
Some simple tests to check that the framework is properly wrapped.
'''
import objc
import unittest
import IOBluetoothUI

class TestIOBluetoothUI (unittest.TestCase):
    def testClasses(self):
        pass
        # self.assert_( hasattr(IOBluetoothUI, 'CLASSNAME') )
        # self.assert_( isinstance(IOBluetoothUI.CLASSNAME, objc.objc_class) )
        # Tollfree CF-type:
        # self.assert_( hasattr(IOBluetoothUI, 'CLASSNAMERef') )
        # self.assert_( IOBluetoothUI.CLASSNAMERef is IOBluetoothUI.CLASSNAME )

        # Not-tollfree CF-type:
        # self.assert_( hasattr(IOBluetoothUI, 'CLASSNAMERef') )
        # self.assert_( issubclass(IOBluetoothUI.CLASSNAMERef, objc.lookUpClass('NSCFType')) )
        # self.assert_( IOBluetoothUI.CLASSNAMERef is not objc.lookUpClass('NSCFType') )

    def testValues(self):
        # Use this to test for a number of enum and #define values
        pass

        # Integer values:
        # self.assert_( hasattr(IOBluetoothUI, 'CONSTANT') )
        # self.assert_( isinstance(IOBluetoothUI.CONSTANT, (int, long)) )
        # self.assertEquals(IOBluetoothUI.CONSTANT, 7)

        # String values:
        # self.assert_( hasattr(IOBluetoothUI, 'CONSTANT') )
        # self.assert_( isinstance(IOBluetoothUI.CONSTANT, (str, unicode)) )
        # self.assertEquals(IOBluetoothUI.CONSTANT, 'value')

    def testVariables(self):
        # Use this to test for global variables, (NSString*'s and the like)
        pass

        # self.assert_( hasattr(IOBluetoothUI, 'CONSTANT') )
        # self.assert_( isinstance(IOBluetoothUI.CONSTANT, unicode) )

    def testFunctions(self):
        # Use this to test for functions
        pass

        # self.assert_( hasattr(IOBluetoothUI, 'FUNCTION') )

    def testOpaque(self):
        # Use this to test for opaque pointers
        pass

        # self.assert_( hasattr(IOBluetoothUI, 'OPAQUE') )

    def testProtocols(self):
        # Use this to test if informal protocols  are present
        pass

        # self.assert_( hasattr(IOBluetoothUI, 'protocols') )

        # self.assert_( hasattr(IOBluetoothUI.protocols, 'PROTOCOL') )
        # self.assert_( isinstance(IOBluetoothUI.protocols.PROTOCOL, objc.informal_protocol) )

    def test_structs(self):
        # Use this to test struct wrappers
        pass

        # self.assert_( hasattr(IOBluetoothUI, 'STRUCT') )
        # o = IOBluetoothUI.STRUCT()
        # self.assert_( hasattr(o, 'FIELD_NAME') )



if __name__ == "__main__":
    unittest.main()

