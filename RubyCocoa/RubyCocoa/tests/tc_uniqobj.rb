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

class TableViewNibOwner < OSX::NSObject
  ib_outlet :tableView
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
    
    def test_equality_of_a_objc_object_that_has_been_instantiated_from_a_nib
      # A naive fix for equality.
      # OSX::NSObject.class_eval do
      #   def ==(other)
      #     __ocid__ == other.__ocid__
      #   end
      # end
      
      url = OSX::NSURL.fileURLWithPath(File.expand_path('../TableView.nib', __FILE__))
      nib = OSX::NSNib.alloc.initWithContentsOfURL(url)
      owner = OSX::TableViewNibOwner.alloc.init
      res, outlets = nib.instantiateNibWithOwner_topLevelObjects(owner)
      
      # They are both the same objc objects but referenced in another way,
      # which seems to return different ruby proxy instances.
      tableView1 = outlets.select {|obj| obj.is_a? OSX::NSScrollView }.first
      tableView2 = owner.instance_variable_get(:@tableView)
      
      assert tableView1 == tableView2
      assert tableView1 === tableView2
      assert OSX::NSScrollView === tableView2
    end
end

