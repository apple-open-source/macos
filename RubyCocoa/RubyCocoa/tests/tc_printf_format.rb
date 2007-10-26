require 'test/unit'
require 'osx/cocoa'

class TC_PrintfFormat < Test::Unit::TestCase

  def test_int
    verify('%d', 42)
  end

  def test_float
    verify('%f', 42.42)
    verify('%0.2f', 42.42)
  end

  def test_char
    verify('%c', ?a)
    verify('%c', ?A)
  end

  def test_str
    verify('foo %s bar %s', 'hoge', 42)
  end

  def test_complex
    verify('blah %f/%d %%%s', 123.123, 42, 'gruik')
  end

  def verify(fmt, *args)
    to_match = fmt % args
    s = OSX::NSString.stringWithFormat(fmt, *args)
    assert_kind_of(OSX::NSString, s)
    assert_equal(to_match, s.to_s)
    s2 = OSX::CFStringCreateWithFormat(nil, nil, fmt, *args)
    assert_kind_of(OSX::NSString, s2)
    assert_equal(s.to_s, s2.to_s)
    assert(s2.isEqual(s))
  end

  def test_invalid
    assert_raises(ArgumentError) { verify('%d') }
    assert_raises(ArgumentError) { verify('%d %d', 1) }
  end

end
