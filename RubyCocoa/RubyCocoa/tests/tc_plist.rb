#
#  Copyright (c) 2007 Laurent Sansonetti, Apple Computer Inc.
#

require 'test/unit'
require 'osx/cocoa'

class Time
  def ==(o)
    o.is_a?(Time) ? self.to_i == o.to_i : false
  end
end

class TC_Plist < Test::Unit::TestCase

  def setup
    @tiger_or_lower = `sw_vers -productVersion`.to_f <= 10.4
  end

  def test_array
    obj = ['ichi', 2, 3, 'quatre', 5]
    verify(obj)
  end

  def test_dict
    obj = {'un' => 1, 'deux' => 'ni', 'trois' => 3}
    verify(obj)
  end

  def test_fixnum
    obj = 42
    verify(obj)
  end

  def test_bignum
    obj = 100_000_000_000
    verify(obj, !@tiger_or_lower)
  end

  def test_float
    obj = 42.5
    verify(obj)
  end

  def test_boolean
    verify(true)
    verify(false)
  end

  def test_time
    verify(Time.now)
  end

  def test_complex
    hash = {
      'foo' => [ 1, 2, 3 ],
      'bar' => [ 4, 5, 6 ],
      'edited' => true,
      'last_modification' => Time.now 
    }
    verify(hash)
  end

  def verify(rbobj, test_binary=true)
    formats = [ nil, OSX::NSPropertyListXMLFormat_v1_0 ]
    formats << OSX::NSPropertyListBinaryFormat_v1_0 if test_binary
    formats.each do |format|
      data = rbobj.to_plist(format)
      assert_kind_of(String, data)
      obj = OSX.load_plist(data)
      assert_kind_of(rbobj.class, obj)
      assert_equal(obj, rbobj)
    end
  end

  def test_invalid_object
    assert_raise(RuntimeError) do
      obj = { 1 => 1 }
      obj.to_plist
    end
  end

  def test_invalid_plist_data
    assert_raise(RuntimeError) do
      OSX.load_plist(nil)
    end
    assert_raise(RuntimeError) do
      OSX.load_plist('some invalid data')
    end
  end

end
