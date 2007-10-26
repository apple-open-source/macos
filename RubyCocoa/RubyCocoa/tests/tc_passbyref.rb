#
#  Copyright (c) 2006 Laurent Sansonetti, Apple Computer Inc.
#

require 'test/unit'
require 'osx/cocoa'

system 'make -s' || raise(RuntimeError, "'make' failed")
require 'objc_test.bundle'

class PassByRefSubclass1 < OSX::PassByRef

  def passByRefObject(objcptr)
    unless objcptr.nil?
      raise "#{objcptr} not an ObjCPtr" unless objcptr.is_a?(OSX::ObjcPtr)
      objcptr.assign(self)
      1
    else
      0
    end
  end

  def passByRefInteger(objcptr)
    unless objcptr.nil?
      raise "#{objcptr} not an ObjCPtr" unless objcptr.is_a?(OSX::ObjcPtr)
      objcptr.assign(6666)
      1
    else
      0
    end
  end

  def passByRefFloat(objcptr)
    unless objcptr.nil?
      raise "#{objcptr} not an ObjCPtr" unless objcptr.is_a?(OSX::ObjcPtr)
      objcptr.assign(6666.0)
      1
    else
      0
    end
  end

  def passByRefVarious_integer_floating(objcptr1, objcptr2, objcptr3)
    unless objcptr1.nil?
      raise "#{objcptr1} not an ObjCPtr" unless objcptr1.is_a?(OSX::ObjcPtr)
      objcptr1.assign(self)
    end
    unless objcptr2.nil?
      raise "#{objcptr2} not an ObjCPtr" unless objcptr2.is_a?(OSX::ObjcPtr)
      objcptr2.assign(6666)
    end
    unless objcptr3.nil?
      raise "#{objcptr3} not an ObjCPtr" unless objcptr3.is_a?(OSX::ObjcPtr)
      objcptr3.assign(6666.0)
    end
  end

end

class TC_PassByRef < Test::Unit::TestCase

  def test_passbyref_methods
    bridged = OSX::PassByRef.alloc.init

    # Object.
    assert_equal(0, bridged.passByRefObject(nil))
    assert_equal([1, bridged], bridged.passByRefObject_)
  
    # Integer.
    assert_equal(0, bridged.passByRefInteger(nil))
    assert_equal([1, 666], bridged.passByRefInteger_)
    
    # Float.
    assert_equal(0, bridged.passByRefFloat(nil))
    assert_equal([1, 666.0], bridged.passByRefFloat_)

    # Various.
    assert_nil(bridged.passByRefVarious_integer_floating(nil, nil, nil))
    assert_equal([bridged, 666, 666.0], bridged.passByRefVarious_integer_floating_)
    assert_equal([666, 666.0], bridged.passByRefVarious_integer_floating_(nil))
    assert_equal(666.0, bridged.passByRefVarious_integer_floating_(nil, nil))
  end

  def test_passbyref_subclass_methods
    bridged = OSX::PassByRefSubclass1.alloc.init
    
    # Object.
    assert_equal(0, bridged.passByRefObject(nil))
    assert_equal([1, bridged], bridged.passByRefObject_)
  
    # Integer.
    assert_equal(0, bridged.passByRefInteger(nil))
    assert_equal([1, 6666], bridged.passByRefInteger_)
    
    # Float.
    assert_equal(0, bridged.passByRefFloat(nil))
    assert_equal([1, 6666.0], bridged.passByRefFloat_)

    # Various.
    assert_nil(bridged.passByRefVarious_integer_floating(nil, nil, nil))
    assert_equal([bridged, 6666, 6666.0], bridged.passByRefVarious_integer_floating_)
    assert_equal([6666, 6666.0], bridged.passByRefVarious_integer_floating_(nil))
    assert_equal(6666.0, bridged.passByRefVarious_integer_floating_(nil, nil))
  end

  def test_passbyref_foundation
    invalid_path = '/does/not/exist'

    # Passing nil for error should not return it. 
    val = OSX::NSString.stringWithContentsOfFile_encoding_error(invalid_path, OSX::NSASCIIStringEncoding, nil)
    assert_nil(val)

    # Not specifying error should return it.  
    val = OSX::NSString.stringWithContentsOfFile_encoding_error(invalid_path, OSX::NSASCIIStringEncoding)
    assert_kind_of(Array, val)
    assert_equal(2, val.size)
    assert_nil(val.first)
    assert_kind_of(OSX::NSError, val.last)

    o = OSX::NSString.alloc.initWithString('foobar')
    val = o.writeToFile_atomically_encoding_error(invalid_path, false, OSX::NSASCIIStringEncoding, nil)
    assert_equal(false, val)

    val = o.writeToFile_atomically_encoding_error(invalid_path, false, OSX::NSASCIIStringEncoding)
    assert_kind_of(Array, val)
    assert_equal(2, val.size)
    assert_equal(false, val.first)
    assert_kind_of(OSX::NSError, val.last)
  end
  
  def test_in_c_array_id
    o1, o2 = OSX::NSObject.alloc.init, OSX::NSObject.alloc.init
    ary = OSX::NSArray.arrayWithObjects_count([o1, o2])
    assert_equal(o1, ary.objectAtIndex(0))
    assert_equal(o2, ary.objectAtIndex(1))
  end

  def test_multiple_in_c_array_id
    o1, o2 = OSX::NSObject.alloc.init, OSX::NSObject.alloc.init
    o1, o2 = OSX::NSString.alloc.initWithString('o1'), OSX::NSString.alloc.initWithString('o2')
    k1, k2 = OSX::NSString.alloc.initWithString('k1'), OSX::NSString.alloc.initWithString('k2')
    dict = OSX::NSDictionary.dictionaryWithObjects_forKeys_count([o1, o2], [k1, k2])
    assert_equal(o1, dict.objectForKey(k1))
    assert_equal(o2, dict.objectForKey(k2))
    # Both 'array' arguments must have the same length.
    assert_raises(ArgumentError) { OSX::NSDictionary.dictionaryWithObjects_forKeys_count([o1, o2], [k1]) }
  end

  def test_in_c_array_byte
    s = OSX::NSString.alloc.initWithBytes_length_encoding('foobar', OSX::NSASCIIStringEncoding)
    assert_equal(6, s.length)
    assert_equal('foobar', s.to_s)
    s = OSX::NSString.alloc.initWithBytes_length_encoding('foobar', 6, OSX::NSASCIIStringEncoding)
    assert_equal(6, s.length)
    assert_equal('foobar', s.to_s)
    s = OSX::NSString.alloc.initWithBytes_length_encoding('foobar', 3, OSX::NSASCIIStringEncoding)
    assert_equal(3, s.length)
    assert_equal('foo', s.to_s)
    assert_raises(ArgumentError) { OSX::NSString.alloc.initWithBytes_length_encoding('foobar', 100, OSX::NSASCIIStringEncoding) }
    assert_raises(ArgumentError) { OSX::NSString.alloc.initWithBytes_length_encoding('foobar', -1, OSX::NSASCIIStringEncoding) }
    assert_raises(ArgumentError) { OSX::NSApplicationMain(1, []) }
    assert_raises(ArgumentError) { OSX::NSApplicationMain(-1, []) }
  end

  def test_out_c_array_byte
    d = OSX::NSData.alloc.initWithBytes_length('foobar')
    data = '      ' 
    d.getBytes_length(data)
    assert_equal(data, 'foobar')
    data = '   ' 
    d.getBytes_length(data)
    assert_equal(data, 'foo')
  end

  def test_ignored
    d = OSX::NSData.alloc.initWithBytes_length('foobar')
    assert_raises(RuntimeError) { d.getBytes(nil) }
    assert_raises(RuntimeError) { d.getBytes_range(nil, OSX::NSRange.new(0, 1)) }
  end

  def test_null_accepted
    assert_raises(ArgumentError) { OSX::NSCountWindows(nil) }
    r = OSX::NSCountWindows()
    assert_kind_of(Fixnum, r)
  end

  def test_in_c_array_fixed_length
    font = OSX::NSFont.fontWithName_matrix('Helvetica', [1, 0, 0, 1, 0, 0].pack('f*'))
    assert_kind_of(OSX::NSFont, font)
    assert_raises(ArgumentError) { OSX::NSFont.fontWithName_matrix('Helvetica', nil) } 
    assert_raises(ArgumentError) { OSX::NSFont.fontWithName_matrix('Helvetica', [].pack('f*')) } 
    assert_raises(ArgumentError) { OSX::NSFont.fontWithName_matrix('Helvetica', [1, 2, 3, 4, 5].pack('f*')) } 
    assert_raises(ArgumentError) { OSX::NSFont.fontWithName_matrix('Helvetica', [1, 2, 3, 4, 5, 6, 7].pack('f*')) } 
    # TODO: should support direct Array of Float.
  end

  def test_out_c_array_length_pointer
    ary = OSX::NSBitmapImageRep.getTIFFCompressionTypes_count_()
    assert_kind_of(Array, ary)
    assert_equal(2, ary.length)
    assert_kind_of(String, ary.first)
    assert_kind_of(Fixnum, ary.last)
    types = ary.first.unpack('i*')
    assert_equal(ary.last, types.length) 
  end

  def test_out_c_array_length_pointer2
    v = OSX::NSView.alloc.initWithFrame(OSX::NSZeroRect)
    ary = v.getRectsExposedDuringLiveResize_count_()
    assert_kind_of(Array, ary)
    assert_equal(2, ary.length)
    assert_equal(1, ary.last)
    assert_kind_of(Array, ary.first)
    assert_equal(1, ary.first.size)
    assert_equal(OSX::NSZeroRect, ary.first.first)
  end

  def test_out_pointer_in_middle
    ary = OSX::CFURLCreateDataAndPropertiesFromResource(nil, 
      OSX::NSURL.URLWithString('file:///doesNotExist'), 
      OSX::NSArray.array)
    assert_kind_of(Array, ary)
    assert_equal(4, ary.length)
    assert_equal(false, ary.first)
  end

  def test_get_c_ari_and_pstring
    path = '/System/Library/Frameworks/ApplicationServices.framework/Frameworks/SpeechSynthesis.framework'
    return unless File.exist?(File.join(path, 'Resources/BridgeSupport/SpeechSynthesis.bridgesupport'))
    OSX.require_framework(path)
    voiceDescSize = OSX::VoiceDescription.size
    assert_equal(362, voiceDescSize)
    error, numVoices = OSX.CountVoices
    assert(numVoices > 1)
    globalVoiceSpec = OSX::VoiceSpec.new
    globalVoiceDesc = OSX::VoiceDescription.new
    (1..numVoices).each do |i|
      # Get the spec by passing a reference to an existing spec structure, and also
      # by omitting the parameter, and verify that they are the same.
      voiceSpec = OSX::VoiceSpec.new
      error = OSX.GetIndVoice(i, voiceSpec)
      assert_equal(0, error)
      error, voiceSpec2 = OSX.GetIndVoice(i)
      assert_equal(0, error)
      assert_equal(voiceSpec, voiceSpec2)
      error = OSX.GetIndVoice(i, globalVoiceSpec)
      assert_equal(0, error)
      assert_equal(voiceSpec, globalVoiceSpec)

      # Same for the desc.
      voiceDesc = OSX::VoiceDescription.new
      error = OSX.GetVoiceDescription(voiceSpec, voiceDesc, voiceDescSize)
      assert_equal(0, error)
      error, voiceDesc2 = OSX.GetVoiceDescription(voiceSpec, voiceDescSize)
      assert_equal(0, error)
      assert_equal(voiceDesc, voiceDesc2)
      error = OSX.GetVoiceDescription(voiceSpec, globalVoiceDesc, voiceDescSize)
      assert_equal(0, error)
      assert_equal(voiceDesc, globalVoiceDesc)

      # Test the pstring stuff. 
      assert_kind_of(Array, voiceDesc.name)
      assert_equal(64, voiceDesc.name.length)
      str = voiceDesc.name.pack_as_pstring
      assert_kind_of(String, str)
      assert_equal(voiceDesc.name[0..str.length], str.unpack_as_pstring)
      assert_kind_of(Array, voiceDesc.comment)
      assert_equal(256, voiceDesc.comment.length)
      str = voiceDesc.comment.pack_as_pstring
      assert_kind_of(String, str)
      assert_equal(voiceDesc.comment[0..str.length], str.unpack_as_pstring)
    end

    # Test C_ARY setters.
    globalVoiceDesc.name = 'foo'.unpack_as_pstring
    assert_equal('foo',globalVoiceDesc.name.pack_as_pstring)
    assert_raises(TypeError) { globalVoiceDesc.name = 'str' }
    assert_raises(ArgumentError) { globalVoiceDesc.name = (0..65).to_a }
  end

  def test_nsscanner_untouch_pointer
    # [NSScanner scanUpToString_intoString] doesn't touch the 
    # passed-by-reference NSString if the given token could not be scanned.
    # RubyCocoa sets the pointer as NULL by default. 
    string = "world\nis\nmine\n"
    scanner = OSX::NSScanner.scannerWithString(string)
    3.times do
      go, line = scanner.scanUpToString_intoString("\n")
      assert(go)
      assert_kind_of(OSX::NSString, line)
    end
    go, line = scanner.scanUpToString_intoString("\n")
    assert(!go)
    assert_nil(line)
  end

  # TODO:
  #def test_null_terminated
  #end

  # TODO:
  #def test_retval_array
  #end

  # TODO: test NSCoder encode/decode methods
end
