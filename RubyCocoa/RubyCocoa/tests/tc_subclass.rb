#
#  $Id: tc_subclass.rb 1821 2007-06-08 07:16:33Z lrz $
#
#  Copyright (c) 2005-2006 kimura wataru
#  Copyright (c) 2001-2002 FUJIMOTO Hisakuni
#

require 'test/unit'
require 'osx/cocoa'

class SubClassA < OSX::NSObject

  DESCRIPTION = "Overrided 'description' Method !"

  ib_outlet :dummy_outlet

  def description(); DESCRIPTION end

end

###class ExceptionTest < OSX::NSObject
system 'make -s' || raise(RuntimeError, "'make' failed")
require 'objc_test.bundle'

OSX.ns_import :Override

class SubClassB < OSX::Override

  def self.foo() return 123 end
  def foo() return 123 end

  def giveRect
    OSX::NSRect.new(1,2,3,4)
  end

end

class OSX::NSObject
    def a_sample_method
    end
end

class CalledClass < OSX::NSObject
    def calledFoo(dummy)
        'foo'
    end
    def calledFoo_(dummy)
        'bar'
    end
end

class UndoSlave < OSX::NSObject
    def addFoo(o)
        o.addObject('foo')
    end
    addRubyMethod_withType('foo:', 'v@:@')
end

class SimpleClass; end

class TestActionClass < OSX::NSObject; end

class TC_SubClass < Test::Unit::TestCase

  def test_s_new
    err = nil
    begin
      SubClassA.new
    rescue => err
    end
    assert_kind_of( RuntimeError, err )
    assert_equal( OSX::NSBehaviorAttachment::ERRMSG_FOR_RESTRICT_NEW, err.to_s )
  end

  def test_ocid
    obj = SubClassA.alloc.init
    assert_not_nil( obj.__ocid__ )
    assert_kind_of( Integer, obj.__ocid__ )
  end

  def test_override
    obj = SubClassA.alloc.init
    assert_equal( SubClassA::DESCRIPTION, obj.description )
    assert_equal( SubClassA::DESCRIPTION, obj.objc_send(:description).to_s )
    assert_equal( SubClassA.objc_instance_method_type('description'), 
		  SubClassA.objc_instance_method_type('super:description') )
    obj_b = SubClassB.alloc.init
    assert_equal( 123, obj_b.foo )
    assert_equal( 123, obj_b.oc_foo )
    assert_equal( SubClassB.objc_instance_method_type('foo'), 
		  SubClassB.objc_instance_method_type('super:foo') )
    assert_equal( 321, obj_b.super_foo )
    assert_equal( 123, obj_b.class.foo )
    assert_equal( 123, obj_b.class.oc_foo )
    assert_equal( OSX::NSRect.new(1,2,3,4), obj_b.giveRect )
    assert_equal( OSX::NSRect.new(1,2,3,4), obj_b.oc_giveRect )
    OSX::Override.testFooOn(obj_b)
    OSX::Override.testFooOn(SubClassB)
    OSX::Override.testFooOnClassName('SubClassB')
  end

  def test_outlet
    obj = SubClassA.alloc.init
    assert_nothing_thrown { obj.dummy_outlet = 12345 }
    assert_equal( 12345, obj.instance_eval{ @dummy_outlet } )
    assert_nothing_thrown { obj.dummy_outlet = 12345.to_s }
    assert_equal( 12345.to_s, obj.instance_eval{ @dummy_outlet } )
  end

  def test_action
    kls = TestActionClass
    kls.class_eval do
      attr_reader :foo, :bar, :baz
      def initialize
        @prefix = 'hello,'
      end
      def setFoo(sender)
        @foo = @prefix + sender.to_s
      end
    end
    assert_nothing_thrown {
      kls.class_eval do 
        ib_action :setFoo
      end
    }
    assert_nothing_thrown {
      kls.class_eval do
        ib_action(:setBar) { |sender|
          @bar = @prefix + sender.to_s
        }
      end
    }
    assert_nothing_thrown {
      kls.class_eval do
        ib_action :setBaz do |sender|
          @baz = @prefix + sender.to_s
        end
      end
    }
    assert_nothing_thrown {
      kls.class_eval do
        ib_action(:hoge) { @foo = @bar = @baz = 'hoge' }
      end
    }
    obj = kls.alloc.init
    assert_nothing_thrown { obj.objc_send(:setFoo, "world") }
    assert_equal( "hello,world", obj.foo )
    assert_nothing_thrown { obj.objc_send(:setBar, "world") }
    assert_equal( "hello,world", obj.bar )
    assert_nothing_thrown { obj.objc_send(:setBaz, "world") }
    assert_equal( "hello,world", obj.baz )
    assert_nothing_thrown { obj.objc_send(:hoge) }
    assert_equal( 'hoge', obj.foo )
  end

  def test_addmethod
    obj = SubClassA.alloc.init
    assert_raise( OSX::OCMessageSendException ) { obj.unknownMethod('dummy') }
    SubClassA.module_eval <<-EOS
      addRubyMethod_withType('unknownMethod:', 'i4@8@12' )
      def unknownMethod(text) return 123 end
    EOS
    assert_equal( 123, obj.unknownMethod('dummy') )
  end

  def test_objc_ruby_call
    caller = OSX::CallerClass.alloc.init # <- ObjC
    called = CalledClass.alloc.init # <- Ruby
    saved_relaxed_syntax = OSX.relaxed_syntax
    OSX.relaxed_syntax = true
    assert_equal('foo', caller.callFoo(called).to_s)
    OSX.relaxed_syntax = false
    assert_equal('bar', caller.callFoo_(called).to_s)
    OSX.relaxed_syntax = saved_relaxed_syntax
  end

  def test_ancestors
    # Basic ancestors.
    assert(OSX::NSString.ancestors.include?(OSX::NSObject))
    
    # CoreFoundation-bridged ancestors.
    assert(OSX::NSCFString.ancestors.include?(OSX::NSString))
    assert(OSX::NSCFDictionary.ancestors.include?(OSX::NSDictionary))
    assert_kind_of(OSX::NSCFArray, OSX::NSArray.array)  
 
    # Method manually defined in an ancestor. 
    s = OSX::NSString.stringWithString('foo')
    assert(s.is_a?(OSX::NSObject))
    assert(s.respond_to?(:a_sample_method))
  end

  OSX.ns_import :SkipInternalClass
  def test_skip_internal_class
    assert_equal(OSX::NSData, OSX::SkipInternalClass.superclass)
    obj = OSX::SkipInternalClass.alloc.init
    assert_kind_of(OSX::NSData, obj)
    assert_kind_of(OSX::NSObject, obj)
  end

  # testunit-0.1.8 has "assert_raises" not "assert_raise"
  unless method_defined? :assert_raise
    alias :assert_raise :assert_raises
  end

  def test_undo1
    ary = OSX::NSMutableArray.arrayWithObject('foo')
    undo = OSX::NSUndoManager.alloc.init
    undo.prepareWithInvocationTarget(ary)
    undo.removeLastObject
    assert_equal(1, ary.count)
    undo.undo
    assert_equal(0, ary.count)
  end

  def test_undo2
    slave = UndoSlave.alloc.init 
    ary = OSX::NSMutableArray.alloc.init
    undo = OSX::NSUndoManager.alloc.init
    undo.prepareWithInvocationTarget(slave)
    undo.addFoo(ary)
    assert_equal(0, ary.count)
    undo.undo
    assert_equal(1, ary.count)
  end

  def test_rbobject
    test = OSX::TestRBObject.alloc.init
    o = SimpleClass.new    
    n = test.addressOfObject(o)
    assert_equal(n, test.addressOfObject(o))
    GC.start
    assert_equal(n, test.addressOfObject(o))
    o = SimpleClass.new
    assert(n != test.addressOfObject(o))
  end

end
