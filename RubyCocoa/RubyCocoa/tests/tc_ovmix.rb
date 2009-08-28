#
#  Copyright (c) 2006 Laurent Sansonetti, Apple Computer Inc.
#  This test is based on a bug report reproducer, written by Tim Burks.
#

require 'test/unit'
require 'osx/cocoa'
require 'rbconfig'

system 'make -s' || raise(RuntimeError, "'make' failed")
require 'objc_test.bundle'

class RigHelper < OSX::NSObject
  def name
    "helper"
  end

  addRubyMethod_withType("testChar:", "c@:c")
  def testChar(c)
    c
  end

  addRubyMethod_withType("testInt:", "i@:i")
  def testInt(i)
    i
  end

  addRubyMethod_withType("testShort:", "s@:s")
  def testShort(s)
    s
  end

  addRubyMethod_withType("testLong:", "l@:l")
  def testLong(l)
    l
  end

  addRubyMethod_withType("testFloat:", "f@:f")
  def testFloat(f)
    f
  end

  addRubyMethod_withType("testDouble:", "d@:d")
  def testDouble(d)
    d
  end

  addRubyMethod_withType("testLongLong:", "q@:q")
  def testLongLong(ll)
    ll
  end
end

class ObjcExportHelper < OSX::NSObject
  def foo1
    's'
  end
  objc_method :foo1, %w{id}

  def foo2(integer)
    integer + 2
  end
  objc_method :foo2, %w{int int}

  def foo3_obj(ary, obj)
    ary.addObject(obj)
  end
  objc_method :foo3, %w{void id id}

  def foo4_size(point, size)
    OSX::NSRect.new(point, size)
  end
  objc_method :foo4_size, [OSX::NSRect, OSX::NSPoint, OSX::NSSize]

  def foo5(rectPtr)
    rectPtr.origin.x *= 10
    rectPtr.origin.y *= 10
    rectPtr.size.width *= 10
    rectPtr.size.height *= 10
  end
  addRubyMethod_withType("foo5:", "v@:^#{OSX::NSRect.encoding}")

  def self.superFoo
    42
  end
  objc_class_method :superFoo, ['int']
end

class TestStret < OSX::ObjcTestStret
  def overrideMe(x)
    OSX::NSRect.new(x,x,x,x)
  end
end

class OSX::DirectOverride
  def self.classOverrideMe
    'bar'
  end
  def overrideMe
    'foo'
  end
end

class OSX::DirectOverrideChild
  def overrideMe
    'bar'
  end
end

class OSX::NSObject
  def self.mySuperClassMethod
    'bar'
  end
  objc_class_method :mySuperClassMethod, ['id']
  def mySuperMethod
    'foo'
  end
  objc_method :mySuperMethod, ['id']

  objc_method(:mySuperMethodWithBlock, [:id, :int]) { |x| "foo_#{x}" }
end

class OvmixArgRetainedController < OSX::NSObject
  def setObject(o)
    @o = o
  end
  objc_method :setObject, ['id', 'id']
  def getObject
    @o
  end
  objc_method :getObject, ['id']
end

class TC_OVMIX < Test::Unit::TestCase
  def test_rig
    testrig = OSX::TestRig.alloc.init
    testrig.run    
  end

  def test_objc_method
    OSX::TestObjcExport.runTests
  end

  def test_direct_override
    assert(OSX::DirectOverride.ancestors.include?(OSX::NSObject))
    o = OSX::DirectOverride.alloc.init
    assert_kind_of(OSX::NSString, o.performSelector('overrideMe'))
    assert_equal('foo', o.performSelector('overrideMe').to_s)
    assert_kind_of(OSX::NSString, OSX::DirectOverride.performSelector('classOverrideMe'))
    assert_equal('bar', OSX::DirectOverride.performSelector('classOverrideMe').to_s)
    OSX::DirectOverride.checkOverridenMethods
  end

  def test_direct_inheritance
    assert(OSX::DirectOverrideParent.ancestors.include?(OSX::NSObject))
    p = OSX::DirectOverrideParent.alloc.init
    assert_kind_of(OSX::NSString, p.performSelector('overrideMe'))
    assert_equal('foo', p.performSelector('overrideMe').to_s)
    p.checkOverride('foo')
    
    assert(OSX::DirectOverrideChild.ancestors.include?(OSX::NSObject))
    assert(OSX::DirectOverrideChild.ancestors.include?(
      OSX::DirectOverrideParent))
    c = OSX::DirectOverrideChild.alloc.init
    assert_kind_of(OSX::NSString, c.performSelector('overrideMe'))
    assert_equal('bar', c.performSelector('overrideMe').to_s)
    c.checkOverride('bar')
  end

  def test_super_method
    o = OSX::NSString.stringWithCString('blah')
    assert_equal('foo', o.mySuperMethod.to_s)
    assert_equal('foo', o.performSelector('mySuperMethod').to_s)
    assert_equal('bar', OSX::NSString.mySuperClassMethod.to_s)
    assert_equal('bar', OSX::NSString.performSelector('mySuperClassMethod').to_s)
  end

  def test_objc_method_with_block
    o = OSX::NSString.stringWithCString('blah')
    assert_equal('foo_123', o.mySuperMethodWithBlock(123).to_s)
    assert_equal('foo_456', o.objc_send(:mySuperMethodWithBlock, 456).to_s)
  end

  def test_arg_retained
    c = OvmixArgRetainedController.alloc.init
    OSX::OvmixArgRetained.test(c)
  end
end
