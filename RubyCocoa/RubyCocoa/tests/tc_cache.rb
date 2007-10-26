require 'osx/cocoa'
require 'test/unit'

system 'make -s' || raise(RuntimeError, "'make' failed")
require 'objc_test.bundle'

class ObjcToRubyCacheCallbackTarget < OSX::NSObject
  def callback(obj)
  end
end

class TCObjcToRubyCache < Test::Unit::TestCase
  def test_nsstring
    do_test_cache(OSX::NSString)
  end
  
  def test_nsdata
    do_test_cache(OSX::NSData)
  end
  
  def test_nsarray
    do_test_cache(OSX::NSArray)
  end
  
  def test_nsdictionary
    do_test_cache(OSX::NSDictionary)
  end
  
  def test_nsindexset
    do_test_cache(OSX::NSIndexSet)
  end
  
  private
  
  def do_test_cache(klass)
    t = ObjcToRubyCacheCallbackTarget.alloc.init
    res = OSX::ObjcToRubyCacheTest.testObjcToRubyCacheFor_with(klass, t)
    assert_equal(0, res)
  end
end
