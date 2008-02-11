require 'osx/cocoa'
require 'test/unit'


class TCBoxed < Test::Unit::TestCase
  include OSX
  
  def test_rect_assign
    a = NSRect.new(13,42,49,52)
    assert_nothing_raised {
      a.x = 0
      a.y = 1
      a.width = 2
      a.height = 3
    }
  end
  
  def test_rect_center
    a = NSRect.new(100,100,200,200)
    b = NSPoint.new(200,200)
    assert_equal(true, a.center == b)
  end
  
  def test_rect_contain
    a = NSRect.new(100,100,200,200)
    b = NSRect.new(100,100,100,100)
    assert_equal(true, a.contain?(b))
    c = NSPoint.new(250,250)
    assert_equal(true, a.contain?(c))
    assert_equal(false, b.contain?(c))
  end
  
  def test_rect_inset
    a = NSRect.new(100,100,200,200)
    b = a.inset(3, 11)
    c = NSRect.new(103,111,194,178)
    assert_equal(true, b == c)
  end

  def test_rect_intersect
    a = NSRect.new(100,100,200,200)
    b = NSRect.new(150,150,100,100)
    c = NSRect.new(300,300,100,100)
    assert_equal(true, a.intersect?(b))
    assert_equal(true, b.intersect?(a))
    assert_equal(false, a.intersect?(c))
  end

  def test_rect_offset
    a = NSRect.new(100,100,200,200).offset(30,30)
    b = NSRect.new(130,130,200,200)
    assert_equal(true, a == b)
  end

  def test_rect_union
    a = NSRect.new(100,100,200,200)
    b = NSRect.new(300,300,100,100)
    c = a.union(b)
    d = NSRect.new(100,100,300,300)
    assert_equal(true, c == d)
  end

  def test_rect_empty
    a = NSRect.new(100,100,0,0)
    assert_equal(true, a.empty?)
  end

  def test_point_inrect
    a = NSRect.new(100,100,100,100)
    b = NSPoint.new(120,120)
    c = NSPoint.new(250,200)
    assert_equal(true, b.in?(a))
    assert_equal(false, c.in?(a))
  end
  
  def test_range_contain
    a = NSRange.new(10,20)
    b = NSRange.new(10,20)
    c = NSRange.new(15,16)
    assert_equal(true, a.contain?(b))
    assert_equal(false, a.contain?(c))
    assert_equal(true, a.contain?(15))
    assert_equal(false, a.contain?(33))
  end
  
  def test_range_intersect
    a = NSRange.new(10,20)
    b = NSRange.new(15,20)
    c = NSRange.new(30,20)
    assert_equal(true, a.intersect?(b))
    assert_equal(false, a.intersect?(c))
  end
  
  def test_range_intersection
    a = NSRange.new(10,20)
    b = NSRange.new(15,20)
    c = NSRange.new(15,15)
    assert_equal(true, a.intersection(b) == c)
  end
  
  def test_range_union
    a = NSRange.new(10,10)
    b = NSRange.new(30,20)
    c = NSRange.new(10,40)
    assert_equal(true, a.union(b) == c)
  end
  
  def test_range_empty
    a = NSRange.new(10,0)
    assert_equal(true, a.empty?)
    s = 'abc'.to_ns
    r = s.rangeOfString('z')
    assert(r.not_found?)
    assert(r.empty?)
    r = s.rangeOfString('b')
    assert(!r.not_found?)
    assert(!r.empty?)
  end

  def test_range_max
    [ NSRange.new(10,10),
      NSRange.new(30,20),
      NSRange.new(10,40)
    ].each do |r|
      assert_equal(r.max, OSX::NSMaxRange(r))
    end
  end
  
  def test_range_size
    a = NSRange.new(0,10)
    assert_equal(10, a.size)
    a.size = 42
    assert_equal(10, a.length)
  end
  
  def test_range_not_found
    assert(NSRange.new(OSX::NSNotFound, 0).not_found?)
    assert(!NSRange.new(0, 0).not_found?)
    assert(!NSRange.new(1, 0).not_found?)
    assert(!NSRange.new(0, 1).not_found?)
    cs = OSX::NSCharacterSet.characterSetWithCharactersInString("abc")
    r = OSX::NSString.stringWithString('xyz').rangeOfCharacterFromSet(cs)
    assert(r.not_found?)
  end
end
