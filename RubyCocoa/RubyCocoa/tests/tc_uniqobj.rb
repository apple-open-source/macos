#
#  Copyright (c) 2006 Laurent Sansonetti, Apple Computer Inc.
#

require 'test/unit'
require 'osx/cocoa'

system 'make -s' || raise(RuntimeError, "'make' failed")
require 'objc_test.bundle'

class PureUniqRubyObject
end

class UniqRubyObject < OSX::NSObject
end

class TC_UniqObject < Test::Unit::TestCase

    # Check that a NSObject-based Ruby object is not copied each time it crosses the bridge. 
    def test_rbobj_uniq
        rbobj = UniqRubyObject.alloc.init
        bridged = OSX::UniqObjC.alloc.init
        bridged.start(rbobj)
        2.times do
            assert_equal(bridged.pass(rbobj), rbobj)
        end
        bridged.end(rbobj)
    end

    # Check that a pure Ruby object is not copied each time it crosses the bridge. 
    def test_pure_rbobj_uniq
        rbobj = PureUniqRubyObject.new 
        bridged = OSX::UniqObjC.alloc.init
        bridged.start(rbobj)
        2.times do
            assert_equal(bridged.pass(rbobj), rbobj)
        end
        bridged.end(rbobj)
    end

    # Check that a pure frozen Ruby object is not copied each time it crosses the bridge.
    def test_pure_frozen_rbobj_uniq
        rbobj = PureUniqRubyObject.new.freeze
        bridged = OSX::UniqObjC.alloc.init
        bridged.start(rbobj)
        2.times do
            assert_equal(bridged.pass(rbobj), rbobj)
        end
        bridged.end(rbobj)
    end

    # Check that a pure Objective-C object is not copied each time it crosses the bridge. 
    def test_ocobj_uniq
        o = OSX::UniqObjC.alloc.init
        a = OSX::NSArray.arrayWithObject(o)
        2.times do
            o2 = a.objectAtIndex(0)
            assert_equal(o.__ocid__, o2.__ocid__)
            assert_equal(o, o2)
        end
    end

    # Check that a Ruby-based Objective-C object is not copied each time it crosses the bridge. 
    def test_pure_ocobj_uniq
        o = OSX::NSObject.alloc.init
        a = OSX::NSArray.arrayWithObject(o)
        2.times do
            o2 = a.objectAtIndex(0)
            assert_equal(o.__ocid__, o2.__ocid__)
            assert_equal(o, o2)
        end
    end
end

