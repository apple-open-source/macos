require 'osx/cocoa'
require 'test/unit'

system 'make -s' || raise(RuntimeError, "'make' failed")
require 'objc_test.bundle'

class MyClass < OSX::NSObject
 attr_accessor :bool
 def validateMenuItem(menuItem)
   return @bool
 end
end

class TCBool < Test::Unit::TestCase
 def test_informal_protocol
   rcv = MyClass.alloc.init
   [[false, false], [true, true], [1, true], [0, true],
    [42, true], [nil, false]].each do |val, expected|
    rcv.bool = val
    obj = rcv.objc_send(:validateMenuItem, nil)
    assert_equal(expected, obj, "testing #{val} <=> #{expected}")
   end
 end
end


class RubyDataClass < OSX::NSObject
  attr_accessor :v
  def testBool; @v; end
  def testChar; @v; end
  def testInt; @v; end
  def testFloat; @v; end
  def testDouble; @v; end
end

class AnnotatedRubyDataClass < RubyDataClass
  objc_method :testBool, 'B@:'
  objc_method :testChar, 'c@:'
  objc_method :testInt, 'i@:'
  objc_method :testFloat, 'f@:'
  objc_method :testDouble, 'd@:'
end

class TC_BoolTypeConversion < Test::Unit::TestCase
  def test_bool_type_conversions
    do_objc_to_ruby_conversion_tests
    do_ruby_to_objc_conversion_tests
    OSX.load_bridge_support_file('ObjcTest.bridgesupport')    
    do_objc_to_ruby_conversion_tests_with_bridgesupport
    do_ruby_to_objc_conversion_tests_with_bridgesupport
  end
  
  private
  
  def do_objc_to_ruby_conversion_tests
    objcdata = OSX::ObjcDataClass.alloc.init
    [[:boolNo, 0], [:boolYes, 1],
     [:cppBoolFalse, false], [:cppBoolTrue, true],
     [:intZero, 0], [:intOne, 1], [:int42, 42], [:intMinusOne, -1], [:biguint, 2147483650],
     [:charZero, 0], [:charOne, 1],
     [:floatTwo, 2.0], [:doubleTwo, 2.0]].each do |method, expected|
      res = objcdata.__send__(method)
      assert_equal(expected, res, "testing objc to ruby conversion: #{method} <=> #{expected}")
    end
  end
  
  def do_objc_to_ruby_conversion_tests_with_bridgesupport
    objcdata = OSX::ObjcDataClass.alloc.init
    [[:boolNo, false], [:boolYes, true],
     [:cppBoolFalse, false], [:cppBoolTrue, true],
     [:intZero, 0], [:intOne, 1], [:int42, 42], [:intMinusOne, -1], [:biguint, 2147483650],
     [:charZero, 0], [:charOne, 1],
     [:floatTwo, 2.0], [:doubleTwo, 2.0]].each do |method, expected|
      res = objcdata.__send__(method)
      assert_equal(expected, res, "testing objc to ruby conversion (informal protocol): #{method} <=> #{expected}")
    end
  end
  
  def do_ruby_to_objc_conversion_tests
    rcv = OSX::ObjcConversionTest.alloc.init
    rubydata = AnnotatedRubyDataClass.alloc.init
    do_conversion_tests(rcv, rubydata, '(objc_method)')
  end
  
  def do_ruby_to_objc_conversion_tests_with_bridgesupport
    rcv = OSX::ObjcConversionTest.alloc.init
    rubydata = AnnotatedRubyDataClass.alloc.init
    do_conversion_tests(rcv, rubydata, '(objc_method)')
    rubydata = RubyDataClass.alloc.init
    do_conversion_tests(rcv, rubydata, '(informal protocol)')
  end
  
  def do_conversion_tests(rcv, rubydata, msg)
    [[nil, '0'], [false, '0'], [true, '1'], [0, '1'], [1, '1'], [42, '1'], [2.0, '1']].each do |v, expected|
      rubydata.v = v
      s = rcv.callbackTestBool(rubydata).to_s
      assert_equal(expected, s, "testing ruby to objc BOOL conversion #{msg}: #{v} <=> #{expected}")
    end
=begin
    [[nil, '0'], [false, '0'], [true, '1'], [0, '0'], [1, '1'], [42, '42'], [2.0, '2']].each do |v, expected|
      rubydata.v = v
      s = rcv.callbackTestChar(rubydata).to_s
      assert_equal(expected, s, "testing ruby to objc char conversion #{msg}: #{v} <=> #{expected}")
    end
=end
    [[nil, '0'], [false, '0'], [true, '1'], [0, '0'], [1, '1'], [42, '42'], [2.0, '2']].each do |v, expected|
      rubydata.v = v
      s = rcv.callbackTestInt(rubydata).to_s
      assert_equal(expected, s, "testing ruby to objc int conversion #{msg}: #{v} <=> #{expected}")
    end
    [[0, '0.0'], [1, '1.0'], [42, '42.0'], [2.0, '2.0']].each do |v, expected|
      rubydata.v = v
      s = rcv.callbackTestFloat(rubydata).to_s
      assert_equal(expected, s, "testing ruby to objc float conversion #{msg}: #{v} <=> #{expected}")
    end
    [[0, '0.0'], [1, '1.0'], [42, '42.0'], [2.0, '2.0']].each do |v, expected|
      rubydata.v = v
      s = rcv.callbackTestDouble(rubydata).to_s
      assert_equal(expected, s, "testing ruby to objc double conversion #{msg}: #{v} <=> #{expected}")
    end
  end
end
