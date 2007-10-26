'''
Some simple tests to check that the framework is properly wrapped.
'''
import objc
import unittest
import InterfaceBuilder

class TestInterfaceBuilder (unittest.TestCase):
    def testClasses(self):
        self.assert_( hasattr(InterfaceBuilder, 'IBInspector') )
        self.assert_( isinstance(InterfaceBuilder.IBInspector, objc.objc_class) )

    def testValues(self):
        self.assert_( hasattr(InterfaceBuilder, 'IB_BIG_SIZE') )
        self.assert_( isinstance(InterfaceBuilder.IB_BIG_SIZE, (int, long)) )
        self.assertEquals(InterfaceBuilder.IB_BIG_SIZE, 10000)

        self.assert_( hasattr(InterfaceBuilder, 'IBBottomLeftKnobPosition') )
        self.assert_( isinstance(InterfaceBuilder.IBBottomLeftKnobPosition, (int, long)) )
        self.assertEquals(InterfaceBuilder.IBBottomLeftKnobPosition, 0)

    def testVariables(self):
        self.assert_( hasattr(InterfaceBuilder, 'IBDidBeginTestingInterfaceNotification') )
        self.assert_( isinstance(InterfaceBuilder.IBDidBeginTestingInterfaceNotification, unicode) )

        self.assert_( hasattr(InterfaceBuilder, 'IBTabViewItemPboardType') )
        self.assert_( isinstance(InterfaceBuilder.IBTabViewItemPboardType, unicode) )

    def testStructs(self):
        self.assert_( hasattr(InterfaceBuilder, 'IBInset') )

        o = InterfaceBuilder.IBInset()
        self.assert_( hasattr(o, 'left') )
        self.assert_( hasattr(o, 'top') )
        self.assert_( hasattr(o, 'right') )
        self.assert_( hasattr(o, 'bottom') )



if __name__ == "__main__":
    unittest.main()

