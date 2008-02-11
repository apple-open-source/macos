#
#  $Id: tc_wrapper.rb 2118 2007-11-10 22:58:03Z lrz $
#
#  Copyright (c) 2006 FUJIMOTO Hisakuni
#  Copyright (c) 2006 kimura wataru
#

require 'test/unit'
require 'osx/cocoa'

class TC_OCObjWrapper < Test::Unit::TestCase
  include OSX

  def setup
    @obj = NSObject.alloc.init
    @data = NSData.alloc.init
  end

  def teardown
  end

  def test_instance_methods
    assert_equal_array(@obj.objc_methods, NSObject.objc_instance_methods)
  end

  def test_instance_methods_nonrecursive
    assert_equal_array(NSObject.objc_instance_methods | NSData.objc_instance_methods(false), NSData.objc_instance_methods(true))
  end

  def test_class_methods
    assert_equal_array(NSObject.objc_methods, NSObject.objc_class_methods)
  end

  def test_class_methods_nonrecursive
    assert_equal_array(NSObject.objc_class_methods | NSData.objc_class_methods(false), NSData.objc_class_methods(true))
  end

  def assert_equal_array(ary1, ary2)
    assert_equal((ary1 | ary2).size, ary1.size)
  end

  def test_method_type
    sel = 'getBytes:'
    assert_equal(NSData.objc_instance_method_type(sel),
		 @data.objc_method_type(sel))

    sel = 'dataWithBytes:length:'
    assert_equal(NSData.objc_class_method_type(sel),
		 NSData.objc_method_type(sel))
  end

  def test_message_syntaxes
    old_relaxed_syntax = OSX.relaxed_syntax
    OSX.relaxed_syntax = true
    url1 = NSURL.alloc.initWithScheme_host_path_('http', 'localhost', '/foo') 
    url2 = NSURL.alloc.initWithScheme_host_path('http', 'localhost', '/foo') 
    assert_equal(true, url1.isEqual(url2)) 
    url3 = NSURL.alloc.objc_send(:initWithScheme, 'http', :host, 'localhost', :path, '/foo')
    assert_equal(true, url1.isEqual(url3))
    # No need to check for symbol/value/... and inline Hash syntaxes, as they are deprecated.
    # However we should check that an exception is raised (as if relaxed_syntax was false) for
    # the 1.0.0 release.
    OSX.relaxed_syntax = false 
    url5 = NSURL.alloc.initWithScheme_host_path_('http', 'localhost', '/foo') 
    assert_equal(true, url1.isEqual_(url5))
    assert_raises OSX::OCMessageSendException do
      # We cannot check the following method
      #   NSURL.alloc.initWithScheme_host_path('http', 'localhost', '/foo')
      # because it has already been registered, so let's check another method:
      NSString.stringWithCapacity(42)
    end 
    assert_raises OSX::OCMessageSendException do 
      NSURL.alloc.initWithScheme('http', :host, 'localhost', :path, '/foo')
    end
    assert_raises OSX::OCMessageSendException do
      NSURL.alloc.initWithScheme('http', :host => 'localhost', :path => '/foo')
    end
    OSX.relaxed_syntax = old_relaxed_syntax
  end

  def test_objc_send
    # Some additional tests for objc_send.
    assert_raises ArgumentError do 
      OSX::NSObject.alloc.objc_send
    end 
    assert_nothing_raised { OSX::NSObject.alloc.objc_send :init }
    assert_nothing_raised { OSX::NSArray.arrayWithObject(1).objc_send(:objectAtIndex, 0) }
  end

  def test_missing_args
    assert_raises ArgumentError do
      NSURL.URLWithString_
    end
    assert_raises ArgumentError do
      NSURL.URLWithString_relativeToURL_('http://localhost')
    end
  end

  def test_proxy_ancestors
    assert(!OSX::NSProxy.ancestors.include?(OSX::NSObject))
    assert(!OSX::NSProtocolChecker.ancestors.include?(OSX::NSObject))
    assert(!OSX::NSDistantObject.ancestors.include?(OSX::NSObject))
  end

  def test_alias
    # alias class method
    assert_raises OSX::OCMessageSendException do
      str = NSString.objc_send(:str, 'RubyCocoa')
    end
    OSX::NSString.objc_alias_class_method 'str:', 'stringWithString:'
    str = NSString.objc_send(:str, 'RubyCocoa')
    assert(str.isEqualToString?('RubyCocoa'), 'alias class method')
    # alias instance method
    assert_raises OSX::OCMessageSendException do
      substr = str.objc_send(:substr, [4..8])
    end
    OSX::NSString.objc_alias_method 'substr:', 'substringWithRange:'
    substr = str.objc_send(:substr, [4..8])
    assert(substr.isEqualToString?('Cocoa'), 'alias instace method')
    # RuntimeError should be raise when the selctor does not exist
    assert_raises RuntimeError do
      OSX::NSString.objc_alias_method 'foo', 'foobarbaz'
    end
  end

  def test_inline_functions
    rect = OSX::NSMakeRect(1,2,3,4)
    assert_kind_of(OSX::NSRect, rect)
    assert_equal([[1,2],[3,4]], rect.to_a)
    assert_equal(rect, OSX::NSRect.new(1,2,3,4))
    assert_equal(rect, OSX::NSMakeRect(1,2,3,4))
    assert_equal(rect.origin, OSX::NSPoint.new(1,2))
    assert_equal(rect.origin, OSX::NSMakePoint(1,2))
    assert_equal(rect.size, OSX::NSSize.new(3,4))
    assert_equal(rect.size, OSX::NSMakeSize(3,4))
  end

  def test_enumerator_to_a
    ary = OSX::NSMutableArray.alloc.init
    %w{One Two Three}.each { |o| ary.addObject(o) }
    enum = ary.objectEnumerator
    ary2 = enum.to_a
    assert_kind_of(Array, ary2)
    assert_equal(ary2, ary.to_a)
  end

  def test_convenience_setters
    button = OSX::NSButton.alloc.initWithFrame(NSZeroRect)
    assert_kind_of(OSX::NSControl, button)
    button.intValue = 1
    assert_equal(1, button.intValue)
    button.stringValue = '0'
    assert_equal('0', button.stringValue)
    button.objectValue = true
    assert_equal(true, button.objectValue.to_ruby)
  end

  def test_convenience_predicates
    ary = OSX::NSArray.arrayWithObject(1)
    assert(ary.equalToArray?([1]))
    assert(!ary.equalToArray?([2]))
  end

=begin
  # Test disabled, because #ocm_conforms? has also been disabled.
  def test_conforms_to_protocol
    assert(NSString.ocm_conforms?('NSMutableCopying'))
    assert(!NSObject.ocm_conforms?('NSMutableCopying'))
    assert(NSProxy.ocm_conforms?('NSObject'))
    assert_raises(ArgumentError) { NSObject.ocm_conforms?('Foo') }
  end
=end
end
