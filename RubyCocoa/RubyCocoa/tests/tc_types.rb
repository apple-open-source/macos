#
#  Copyright (c) 2006 Laurent Sansonetti, Apple Computer Inc.
#

require 'test/unit'
require 'osx/cocoa'

system 'make -s' || raise(RuntimeError, "'make' failed")
require 'objc_test.bundle'

class TC_Types < Test::Unit::TestCase

  def test_auto_boolean_conversion_objc
    s1 = OSX::NSString.alloc.initWithString("foo")
    s2 = s1.copy
    s3 = OSX::NSString.alloc.initWithString("bar")
    assert_equal(true, s1.isEqualToString(s2))
    assert_equal(true, s1.isEqualToString?(s2))
    assert_equal(false, s1.isEqualToString(s3))
    assert_equal(false, s1.isEqualToString?(s3))
  end

  def test_auto_boolean_conversion_c
    str = OSX::CFStringCreateWithCString(OSX::KCFAllocatorDefault, 'foobar', OSX::KCFStringEncodingASCII)
    assert_equal(true, OSX::CFStringHasPrefix(str, 'foo'))
    assert_equal(false, OSX::CFStringHasPrefix(str, 'bar'))
  end

  def test_char_conversion
    v = OSX::NSNumber.numberWithChar(?v)
    assert_equal(?v, v.charValue)
  end

  def test_uchar_conversion
    v = OSX::NSNumber.numberWithUnsignedChar(?v)
    assert_equal(?v, v.unsignedCharValue)
  end

  def test_short_conversion
    v = OSX::NSNumber.numberWithShort(42)
    assert_equal(42, v.shortValue)
  end

  def test_ushort_conversion
    v = OSX::NSNumber.numberWithUnsignedShort(42)
    assert_equal(42, v.unsignedShortValue)
  end

  def test_int_conversion
    v = OSX::NSNumber.numberWithInt(42)
    assert_equal(42, v.intValue)
  end

  def test_float_conversion
    v = OSX::NSNumber.numberWithFloat(42.42)
    assert((42.42 - v.floatValue).abs < 0.01)
  end

  def test_nsrect
    rect = OSX::NSRect.new
    assert_equal(0, rect.origin.x)
    assert_equal(0, rect.origin.y)
    assert_equal(0, rect.size.width)
    assert_equal(0, rect.size.height)
    assert_equal(OSX::NSZeroRect.to_a.flatten.map { |x| x.to_i }, rect.to_a.flatten.map { |x| x.to_i })
    rect = OSX::NSRect.new(OSX::NSPoint.new(1, 2), OSX::NSSize.new(3, 4))
    assert_equal(1, rect.origin.x)
    assert_equal(2, rect.origin.y)
    assert_equal(3, rect.size.width)
    assert_equal(4, rect.size.height)
    rect = OSX::NSRect.new(1, 2, 3, 4)
    assert_equal(1, rect.origin.x)
    assert_equal(2, rect.origin.y)
    assert_equal(3, rect.size.width)
    assert_equal(4, rect.size.height)
    assert_equal([[1, 2], [3, 4]], rect.to_a)
    rect.origin.x = 42
    rect.origin.y = 43
    assert_equal(42, rect.origin.x)
    assert_equal(43, rect.origin.y)
    assert_equal(rect, OSX::NSRectFromString('{{42, 43}, {3, 4}'))
    assert_equal('{{42, 43}, {3, 4}}', OSX::NSStringFromRect(rect).to_s)
  end

  def test_nssize
    size = OSX::NSSize.new
    assert_equal(0, size.width)
    assert_equal(0, size.height)
    assert_equal(OSX::NSZeroSize, size)
    size.width = 42
    size.height = 43
    assert_equal([42, 43], size.to_a)
    assert_equal(size, OSX::NSSize.new(42, 43))
    assert_equal(size, OSX::NSSizeFromString('{42, 43}'))
    assert_equal('{42, 43}', OSX::NSStringFromSize(size).to_s)
  end

  def test_nsrange
    range = OSX::NSRange.new
    assert_equal(0, range.location)
    assert_equal(0, range.length)
    assert_equal(OSX::NSRange.new(0, 0), range)
    range = OSX::NSRange.new(2..6)
    assert_equal(2, range.location)
    assert_equal(5, range.length)
    assert_equal(2...7, range.to_range)
    range = OSX::NSRange.new(2, 10)
    assert_equal(2, range.location)
    assert_equal(10, range.length)
    assert_equal([2, 10], range.to_a)
    range.location = 42
    range.length = 43
    assert_equal(42, range.location)
    assert_equal(43, range.length)
  end

  def test_cg_affine_transform
    ary = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0]
    assert_equal(OSX::CGAffineTransformMake(*ary).to_a, ary)
  end

  def test_bool_nsnumber
    d = OSX::NSMutableDictionary.alloc.init
    d.setValue_forKey(true, 'true')
    d.setValue_forKey(false, 'false')
    d.setValue_forKey(123, '123')
    assert_kind_of(OSX::NSCFBoolean, d.objectForKey('true'))
    assert_kind_of(OSX::NSCFBoolean, d.objectForKey('false'))
    assert_kind_of(OSX::NSNumber, d.objectForKey('123'))
  end

  def test_float_nsnumber
    assert(!OSX::CFNumberIsFloatType(42))
    assert(OSX::CFNumberIsFloatType(42.42))
    assert(!OSX::NSNumber.numberWithInt(42).float?)
    assert(OSX::NSNumber.numberWithFloat(42.42).float?)
  end

  def test_cftypes
    str = OSX::NSString.alloc.initWithCString_encoding('foo', OSX::NSASCIIStringEncoding)
    assert_equal(3, str.length)
    assert_equal(3, OSX::CFStringGetLength(str))
    str2 = OSX::CFStringCreateWithCString(OSX::KCFAllocatorDefault, 'foo', OSX::KCFStringEncodingASCII)
    assert_kind_of(OSX::NSString, str2)
    assert_equal(3, str2.length)
    assert_equal(3, OSX::CFStringGetLength(str2))
    assert(str.isEqualToString(str2))
    assert(OSX::CFEqual(str, str2))
    url = OSX::CFURLCreateWithString(OSX::KCFAllocatorDefault, 'http://www.google.com', nil)
    assert_kind_of(OSX::NSURL, url)
    assert_equal(url.path, OSX::CFURLCopyPath(url))
  end

  def test_cftype_proxies
    assert_kind_of(OSX::CFRunLoopRef, OSX::CFRunLoopGetCurrent())
  end

  def test_opaque_boxed
    z = OSX::NSDefaultMallocZone()
    assert_kind_of(OSX::NSZone, z)
    assert_kind_of(OSX::Boxed, z)
    assert_kind_of(OSX::NSString, OSX::NSZoneName(z))
  end

  def test_four_char_code_enums
    OSX.require_framework('AddressBook')
    assert_equal(1633841264, OSX::KEventClassABPeoplePicker)
    assert_equal(OSX::TestFourCharCode.kEventClassABPeoplePickerValue, OSX::KEventClassABPeoplePicker)
  end

  def test_nsdecimal
    zero = OSX::NSDecimalNumber.zero
    assert_equal('0', OSX::NSDecimalString(zero.decimalValue, nil).to_s)
    one = OSX::NSDecimalNumber.one
    assert_equal('1', OSX::NSDecimalString(one.decimalValue, nil).to_s)
    one2 = OSX::NSDecimalNumber.decimalNumberWithDecimal(one.decimalValue)
    assert_equal(OSX::NSOrderedSame, one.compare(one2))
    two = one.decimalNumberByAdding(one)
    assert_equal(2, two.doubleValue)
    four = two.decimalNumberByMultiplyingBy(two)
    assert_equal(4, four.doubleValue)
    still_four = four.decimalNumberByDividingBy(one)
    assert_equal(4, still_four.doubleValue)
    four_pow_four = four.decimalNumberByRaisingToPower(4)
    assert_equal(256, four_pow_four.doubleValue)
    fifty_six = OSX::NSDecimalNumber.decimalNumberWithString('56')
    two_hundred = four_pow_four.decimalNumberBySubtracting(fifty_six)
    assert_equal(200, two_hundred.doubleValue)
  end

  def test_boxed_fields
    ary = OSX::NSRect.fields
    assert_kind_of(Array, ary)
    assert_equal(2, ary.size)
    assert(ary.include?(:origin))
    assert(ary.include?(:size))
    ary = OSX::NSZone.fields
    assert_kind_of(Array, ary)
    assert_equal(0, ary.size) 
  end

  def test_boxed_opaque
    assert(!OSX::NSRect.opaque?)
    assert(!OSX::NSSize.opaque?)
    assert(OSX::NSZone.opaque?)
    assert(OSX::NSDecimal.opaque?)
  end

  def test_const_magic_cookie
    assert_kind_of(OSX::CFAllocatorRef, OSX::KCFAllocatorUseContext)
    # FIMXE test calling CFAllocatorCreate with KCFAllocatorUseContext once we support function pointers 
    assert_equal(1, OSX::TestMagicCookie.isKCFAllocatorUseContext(OSX::KCFAllocatorUseContext))
  end

  def test_ignored_enum
    assert_raise(RuntimeError) { OSX::KCGDirectMainDisplay }
    assert_nothing_raised { OSX.CGMainDisplayID() }
  end

  def test_method_func_ptr_arg
    ary = OSX::NSMutableArray.alloc.init
    [5, 3, 2, 4, 1].each { |i| ary.addObject(i) }
    p = proc do |x, y, ctx| 
      assert_equal(nil, ctx)
      x.intValue <=> y.intValue
    end 
    ary.sortUsingFunction_context(p, nil)
    assert_equal(ary.to_a.map { |i| i.to_i }, [1, 2, 3, 4, 5])
    assert_raise(ArgumentError) { ary.sortUsingFunction_context(proc { || }, nil) }
    assert_raise(ArgumentError) { ary.sortUsingFunction_context(proc { |a| }, nil) }
    assert_raise(ArgumentError) { ary.sortUsingFunction_context(proc { |a, b| }, nil) }
    assert_raise(ArgumentError) { ary.sortUsingFunction_context(proc { |a, b, c, d| }, nil) }
  end

  def test_func_func_ptr_arg
    ary = OSX::NSMutableArray.alloc.init
    [5, 3, 2, 4, 1].each { |i| ary.addObject(i) }
    i = 0
    provided_ctx = nil
    p = proc do |value, ctx|
      if provided_ctx.nil?
        assert_nil(ctx)
      else
        assert_equal(provided_ctx, ctx.bytestr(provided_ctx.length))
      end
      assert_kind_of(OSX::ObjcPtr, value)
      i += 1
    end
    provided_ctx = nil
    OSX::CFArrayApplyFunction(ary, OSX::NSMakeRange(0, ary.count), p, provided_ctx)
    assert_equal(i, 5)
    provided_ctx = "foobar"
    OSX::CFArrayApplyFunction(ary, OSX::NSMakeRange(0, ary.count), p, provided_ctx)
    assert_equal(i, 10)
  end

  def test_boxed_struct_dup
    rect = OSX::NSRect.new(1, 2, 3, 4)
    rect2 = rect.dup
    assert_equal(rect, rect2)
    size2 = rect2.size.dup
    assert_equal(rect.size, size2)
    size3 = rect2.size.clone
    assert_equal(rect.size, size3)
    assert(!OSX::NSZone.instance_methods(false).include?('dup'))
  end
end
