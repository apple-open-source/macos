require 'test/unit'
require 'osx/cocoa'

### Tests below

module TestConstModule
   def self.const_missing(c)
     :module_success
   end
end

class TestConstClass
   def self.const_missing(c)
     :class_success
   end
end


class TC_Constants < Test::Unit::TestCase
   include OSX

   # Check that const lookup directly in OSX module
   # or via the include statement above works.
   def test_normal_constant
     assert_equal 10, OSX::NSKeyDown
     assert_equal 10, NSKeyDown
   end
   
   # Check that a function constant works when accessed as a real
   # constant using ::. Then check that it has now become
   # a real constant on OSX.
   def test_redirected_function_constant
     assert_equal "NSGlobalDomain", OSX::NSGlobalDomain.to_s
     assert OSX.const_defined?(:NSGlobalDomain)
     # This gets found in OSX directly since the previous
     # lookup defined it
     assert_equal "NSGlobalDomain", NSGlobalDomain.to_s
   end

   # Similar to above but without explicit reference to OSX module.
   def test_indirect_function_constant
     assert !Object.const_defined?(:NSFileTypeRegular)
     assert_equal "NSFileTypeRegular", NSFileTypeRegular.to_s
     assert !Object.const_defined?(:NSFileTypeRegular)
     assert OSX.const_defined?(:NSFileTypeRegular)
   end

   def test_real_function
     assert_raises NameError do OSX::NSClassFromString end
   end

   # Check that a normal bad constant fails properly
   def test_nonexistent_constant
     assert_raises NameError do OSX::NonexistentConstant end
     assert_raises NameError do NonexistentConstant end
   end

   # not affects behaviors of Ruby classes
   def test_nonexistent_method
     assert_raises NoMethodError do OSX.foo end
     def OSX.foo; bar; end
     assert_raises NameError do OSX.foo end
   end

   # Ensure that const_missing still works within a different module
   def test_module_const_missing
     assert_equal :module_success, TestConstModule::Foo
   end

   # Ensure that const_missing still works within a different class
   def test_class_const_missing
     assert_equal :class_success, TestConstClass::Foo
   end

   def test_string_constant
     val = OSX::KCGNotifyEventTapAdded
     assert_kind_of(String, val)
     assert_equal('com.apple.coregraphics.eventTapAdded', val)
   end

   def test_string_constant_nsstring
     val = OSX::KCGDisplayMode
     assert_kind_of(OSX::NSString, val)
     assert_equal('Mode', val.to_s)
   end
end

