#
#  $Id: tc_attachments.rb 1740 2007-05-14 16:41:08Z lrz $
#
#  Copyright (c) 2006 Jonathan Paisley
#

require 'test/unit'
require 'osx/cocoa'

class TC_Attachments < Test::Unit::TestCase
  include OSX

  def setup
    @a = NSMutableArray.arrayWithArray(["a","b","c"])
    @d = NSMutableDictionary.dictionaryWithDictionary({"a"=>1,"b"=>2})

    # Initialise the connection to the window server so images are happy
    NSApplication.sharedApplication

    @i = NSImage.alloc.initWithSize([100,100])
  end
  
  def test_array_size
    assert_equal 3, @a.size
  end
  
  def test_dictionary_size
    assert_equal 2, @d.size
  end

  def test_array_index
    assert_equal "a", @a[0].to_s
    assert_equal "c", @a[-1].to_s
  end
  
  def test_dictionary_index
    assert_nil @d["x"]
    assert_equal 2, @d["b"].to_i
  end
  
  def test_array_push
    @a.push "d"
    assert_equal 4, @a.size
    assert_equal "d", @a[3].to_s
  end
  
  def test_array_assign
    assert_equal "BB", (@a[1] = "BB")
    assert_equal "BB", @a[1].to_s
  end
  
  def test_array_enum
    assert_equal ["a","b","c"], @a.map { |e| e.to_s }
  end
  
  def test_dictionary_keys
    assert_kind_of Array, @d.keys
    assert_equal ["a", "b"], @d.keys.map { |e| e.to_s }.sort
  end
  
  def test_dictionary_values
    assert_kind_of Array, @d.values
    assert_equal [1,2], @d.values.map { |e| e.to_i }.sort
  end
  
  def test_dictionary_assign
    assert_equal 3, (@d["c"] = 3)
    assert_equal 3, @d["c"].to_i
  end
  
  def test_data
    data = NSData.dataWithBytes_length("somedata")
    assert_equal "somedata", data.rubyString
  end
  
  def test_image_focus
    assert_respond_to @i, :focus
    success = false
    @i.focus do
      success = true
    end
    assert success, "Focus block did not run"
    
    # Doesn't seem to be a way to test that lockFocus/unlockFocus have been called,
    # so it's not possible to test the begin/ensure block in 'focus'.
  end

  def test_indexset_to_a
    assert_respond_to( OSX::NSIndexSet.alloc.init, :to_a )
    assert_kind_of( Array, OSX::NSIndexSet.alloc.init.to_a )
    assert_equal( [1,2,3], 
                  OSX::NSIndexSet.indexSetWithIndexesInRange(1..3).to_a )
  end

  def test_userdefault
    ud = OSX::NSUserDefaults.standardUserDefaults
    ud['Foo'] = 42
    assert_equal(ud['Foo'], ud.objectForKey('Foo'))
    assert_equal(42, ud['Foo'].intValue)
    assert_equal(42, ud.integerForKey('Foo'))
    ud.delete('Foo')
    assert_equal(nil, ud['Foo'])
    assert_equal(0, ud.integerForKey('Foo'))
  end

  def test_nsdate
    now = OSX::NSDate.date
    now2 = Time.now
    assert_kind_of(Time, now.to_time)
    assert_equal(now2.to_i, now.to_time.to_i)
  end

end
