#
#  $Id: tc_objcptr.rb 2126 2007-11-12 21:42:05Z psychs $
#
#  Copyright (c) 2001-2003 FUJIMOTO Hisakuni
#

require 'test/unit'
require 'osx/cocoa'

system 'make -s' || raise(RuntimeError, "'make' failed")
require 'objc_test.bundle'
OSX.load_bridge_support_file 'ObjcPtrTest.bridgesupport'

class TC_ObjcPtr < Test::Unit::TestCase
  include OSX

  def test_s_new
    length = 123456
    cptr = ObjcPtr.new( length )
    assert_kind_of( ObjcPtr, cptr )
    assert_equal( length, cptr.allocated_size )
    assert( ! cptr.tainted? )
  end

  def test_s_new_with_type
    assert_nothing_raised       { ObjcPtr.new(:char) }
    assert_nothing_raised       { ObjcPtr.new(:uchar) }
    assert_nothing_raised       { ObjcPtr.new(:short) }
    assert_nothing_raised       { ObjcPtr.new(:ushort) }
    assert_nothing_raised       { ObjcPtr.new(:int) }
    assert_nothing_raised       { ObjcPtr.new(:uint) }
    assert_nothing_raised       { ObjcPtr.new(:long) }
    assert_nothing_raised       { ObjcPtr.new(:ulong) }
    assert_nothing_raised       { ObjcPtr.new(:longlong) }
    assert_nothing_raised       { ObjcPtr.new(:ulonglong) }
    assert_nothing_raised       { ObjcPtr.new(:float) }
    assert_nothing_raised       { ObjcPtr.new(:double) }
    assert_raises(RuntimeError) { ObjcPtr.new(:unknown_type) }

    assert_equal( 1, ObjcPtr.new(:char).allocated_size )
    assert_equal( 1, ObjcPtr.new(:uchar).allocated_size )
    assert_equal( 2, ObjcPtr.new(:short).allocated_size )
    assert_equal( 2, ObjcPtr.new(:ushort).allocated_size )
    # assert_equal( 4, ObjcPtr.new(:int).allocated_size )
    # assert_equal( 4, ObjcPtr.new(:uint).allocated_size )
    assert_equal( 4, ObjcPtr.new(:long).allocated_size )
    assert_equal( 4, ObjcPtr.new(:ulong).allocated_size )
    assert_equal( 8, ObjcPtr.new(:longlong).allocated_size )
    assert_equal( 8, ObjcPtr.new(:ulonglong).allocated_size )
    assert_equal( 4, ObjcPtr.new(:float).allocated_size )
    assert_equal( 8, ObjcPtr.new(:double).allocated_size )

    assert_equal( "c", ObjcPtr.new(:char).encoding )
    assert_equal( "C", ObjcPtr.new(:uchar).encoding )
    assert_equal( "s", ObjcPtr.new(:short).encoding )
    assert_equal( "S", ObjcPtr.new(:ushort).encoding )
    assert_equal( "i", ObjcPtr.new(:int).encoding )
    assert_equal( "I", ObjcPtr.new(:uint).encoding )
    assert_equal( "l", ObjcPtr.new(:long).encoding )
    assert_equal( "L", ObjcPtr.new(:ulong).encoding )
    assert_equal( "q", ObjcPtr.new(:longlong).encoding )
    assert_equal( "Q", ObjcPtr.new(:ulonglong).encoding )
    assert_equal( "f", ObjcPtr.new(:float).encoding )
    assert_equal( "d", ObjcPtr.new(:double).encoding )
  end

  def test_s_new_with_type_and_count
    assert_nothing_raised    { ObjcPtr.new(:char, 17) }
    assert_nothing_raised    { ObjcPtr.new(:uchar, 17) }
    assert_nothing_raised    { ObjcPtr.new(:short, 17) }
    assert_nothing_raised    { ObjcPtr.new(:ushort, 17) }
    assert_nothing_raised    { ObjcPtr.new(:int, 17) }
    assert_nothing_raised    { ObjcPtr.new(:uint, 17) }
    assert_nothing_raised    { ObjcPtr.new(:long, 17) }
    assert_nothing_raised    { ObjcPtr.new(:ulong, 17) }
    assert_nothing_raised    { ObjcPtr.new(:longlong, 17) }
    assert_nothing_raised    { ObjcPtr.new(:ulonglong, 17) }
    assert_nothing_raised    { ObjcPtr.new(:float, 17) }
    assert_nothing_raised    { ObjcPtr.new(:double, 17) }
    assert_raises(RuntimeError) { ObjcPtr.new(:unknown_type, 17) }

    assert_equal( 1 * 17, ObjcPtr.new(:char, 17).allocated_size )
    assert_equal( 1 * 17, ObjcPtr.new(:uchar, 17).allocated_size )
    assert_equal( 2 * 17, ObjcPtr.new(:short, 17).allocated_size )
    assert_equal( 2 * 17, ObjcPtr.new(:ushort, 17).allocated_size )
    # assert_equal( 4 * 17, ObjcPtr.new(:int).allocated_size )
    # assert_equal( 4 * 17, ObjcPtr.new(:uint).allocated_size )
    assert_equal( 4 * 17, ObjcPtr.new(:long, 17).allocated_size )
    assert_equal( 4 * 17, ObjcPtr.new(:ulong, 17).allocated_size )
    assert_equal( 8 * 17, ObjcPtr.new(:longlong, 17).allocated_size )
    assert_equal( 8 * 17, ObjcPtr.new(:ulonglong, 17).allocated_size )
    assert_equal( 4 * 17, ObjcPtr.new(:float, 17).allocated_size )
    assert_equal( 8 * 17, ObjcPtr.new(:double, 17).allocated_size )

    assert_equal( "c", ObjcPtr.new(:char, 17).encoding )
    assert_equal( "C", ObjcPtr.new(:uchar, 17).encoding )
    assert_equal( "s", ObjcPtr.new(:short, 17).encoding )
    assert_equal( "S", ObjcPtr.new(:ushort, 17).encoding )
    assert_equal( "i", ObjcPtr.new(:int, 17).encoding )
    assert_equal( "I", ObjcPtr.new(:uint, 17).encoding )
    assert_equal( "l", ObjcPtr.new(:long, 17).encoding )
    assert_equal( "L", ObjcPtr.new(:ulong, 17).encoding )
    assert_equal( "q", ObjcPtr.new(:longlong, 17).encoding )
    assert_equal( "Q", ObjcPtr.new(:ulonglong, 17).encoding )
    assert_equal( "f", ObjcPtr.new(:float, 17).encoding )
    assert_equal( "d", ObjcPtr.new(:double, 17).encoding )
  end

  def test_s_allocate_as_int8
    cptr = ObjcPtr.allocate_as_int8
    assert_kind_of( ObjcPtr, cptr )
    assert_equal( 1, cptr.allocated_size )
    assert( ! cptr.tainted? )
  end

  def test_s_allocate_as_int16
    cptr = ObjcPtr.allocate_as_int16
    assert_kind_of( ObjcPtr, cptr )
    assert_equal( 2, cptr.allocated_size )
    assert( ! cptr.tainted? )
  end

  def test_s_allocate_as_int32
    cptr = ObjcPtr.allocate_as_int32
    assert_kind_of( ObjcPtr, cptr )
    assert_equal( 4, cptr.allocated_size )
    assert( ! cptr.tainted? )
  end

  def test_s_allocate_as_bool
    cptr = ObjcPtr.allocate_as_bool
    assert_kind_of( ObjcPtr, cptr )
    assert_equal( 1, cptr.allocated_size )
    assert( ! cptr.tainted? )
  end

  def test_s_allocate_as_int
    cptr = ObjcPtr.allocate_as_int
    assert_kind_of( ObjcPtr, cptr )
    assert_equal( 4, cptr.allocated_size )
    assert( ! cptr.tainted? )
  end

  def test_cptr_as_returned_value_of_method_call
    cptr = NSData.dataWithContentsOfFile('/etc/passwd').bytes
    assert_kind_of( ObjcPtr, cptr )
    assert_equal( 0, cptr.allocated_size )
    assert( cptr.tainted? )
  end

  def test_cptr_as_param_of_method_call
    src = 'hello world'
    data = NSData.dataWithRubyString( src )
    cptr = ObjcPtr.new( src.size )
    data.getBytes_length( cptr )
    assert_equal( src.size, cptr.allocated_size )
  end

  def test_bytestr_at
    src = 'hello world'
    cptr = NSData.dataWithRubyString(src).bytes
    bstr = cptr.bytestr_at(3,4)
    assert_equal( src[3,4], bstr )
    assert( bstr.tainted? )
  end

  def test_bytestr
    src = 'hello world'
    cptr = NSData.dataWithRubyString(src).bytes
    bstr = cptr.bytestr(src.size)
    assert_equal( src, bstr )
    assert( bstr.tainted? )
  end

  def test_ocptr_ary_like
    components = [0.1, 0.5, 0.9, 0] 
    color = CGColorCreate(CGColorSpaceCreateDeviceRGB(), components)
    assert_kind_of(CGColorRef, color)
    components2 = CGColorGetComponents(color)
    assert((components2[0] >= 0.09 and components2[0] <= 0.11))
    assert((components2[1] >= 0.49 and components2[1] <= 0.51))
    assert((components2[2] >= 0.89 and components2[2] <= 0.91))
  end

  def test_ocptr_aref
    nearly_equal = proc do |v0,v1| 
      x = (v0 - v1).to_f
      (-0.01 <= x && x <= 0.01) ? true : false
    end

    components = [0.1, 0.5, 0.9, 0] 
    color = CGColorCreate(CGColorSpaceCreateDeviceRGB(), components)
    assert_kind_of(CGColorRef, color)
    components2 = CGColorGetComponents(color)
    ary = nil
    assert_nothing_raised { ary = components2[0, 4] }
    assert_equal( true, nearly_equal.call( ary[0], 0.1 ))
    assert_equal( true, nearly_equal.call( ary[1], 0.5 ))
    assert_equal( true, nearly_equal.call( ary[2], 0.9 ))
    assert_equal( true, nearly_equal.call( ary[3], 0.0 ))
  end

  def test_ocptr_int_assign
    obj = ObjcPtr.new(:int)
    obj.assign(123)
    assert_equal(123, obj.int)
    number = NSNumber.numberWithInt(42)
    obj.assign(number)
    assert_equal(42, obj.int)
    assert_raises(ArgumentError) { obj.assign('foo') }
  end

  def test_ocptr_ary_assign
    str = 'foobar'
    obj = ObjcPtr.new(:char, str.length)
    str.length.times { |i| obj[i] = str[i] }
    assert_equal('foobar', obj.bytestr)
    assert_raises(ArgumentError) { obj[0] = 'blah' }
  end
  
  def test_ocptr_as_id
    obj = ObjcPtrTest.new.returnVoidPtrForArrayOfString
    ary = obj.cast_as('@')
    assert_kind_of(OSX::NSArray, ary)
    assert_kind_of(OSX::NSString, ary.first)
    
    obj = ObjcPtrTest.new.returnVoidPtrForKCFBooleanTrue
    assert_equal(true, obj.cast_as('@').boolValue)
    
    obj = ObjcPtrTest.new.returnVoidPtrForKCFBooleanFalse
    assert_equal(false, obj.cast_as('@').boolValue)
    
    obj = ObjcPtrTest.new.returnVoidPtrForInt
    assert_equal(-2147483648, obj.cast_as('^i'))
    
    obj = ObjcPtrTest.new.returnVoidPtrForUInt
    assert_equal(4294967295, obj.cast_as('^I'))
    
    obj = ObjcPtrTest.new.returnVoidPtrForCStr
    assert_equal('foobar', obj.cast_as('*'))
  end

#   rb_define_method (_kObjcPtr, "int8_at", rb_objcptr_int8_at, 1);
#   rb_define_method (_kObjcPtr, "uint8_at", rb_objcptr_uint8_at, 1);
#   rb_define_method (_kObjcPtr, "int16_at", rb_objcptr_int16_at, 1);
#   rb_define_method (_kObjcPtr, "uint16_at", rb_objcptr_uint16_at, 1);
#   rb_define_method (_kObjcPtr, "int32_at", rb_objcptr_int32_at, 1);
#   rb_define_method (_kObjcPtr, "uint32_at", rb_objcptr_uint32_at, 1);
#   rb_define_alias (_kObjcPtr, "int_at", "int32_at");
#   rb_define_alias (_kObjcPtr, "uint_at", "uint32_at");
#   rb_define_alias (_kObjcPtr, "bool_at", "uint8_at");

#   rb_define_method (_kObjcPtr, "int8", rb_objcptr_int8, 0);
#   rb_define_method (_kObjcPtr, "uint8", rb_objcptr_uint8, 0);
#   rb_define_method (_kObjcPtr, "int16", rb_objcptr_int16, 0);
#   rb_define_method (_kObjcPtr, "uint16", rb_objcptr_uint16, 0);
#   rb_define_method (_kObjcPtr, "int32", rb_objcptr_int32, 0);
#   rb_define_method (_kObjcPtr, "uint32", rb_objcptr_uint32, 0);
#   rb_define_alias (_kObjcPtr, "int", "int32");
#   rb_define_alias (_kObjcPtr, "uint", "uint32");
#   rb_define_alias (_kObjcPtr, "bool", "uint8");

end
