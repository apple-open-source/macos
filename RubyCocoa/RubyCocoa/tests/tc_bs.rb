#
#  Copyright (c) 2006 Laurent Sansonetti, Apple Computer Inc.
#

require 'test/unit'
require 'osx/cocoa'
require 'rbconfig'

system 'make -s' || raise(RuntimeError, "'make' failed")

class TC_BridgeSupport < Test::Unit::TestCase

  def setup
    @ruby_path = File.join(Config::CONFIG["bindir"], Config::CONFIG["RUBY_INSTALL_NAME"])
  end
 
  def test_framework_loaded
    %w{Foundation /System/Library/Frameworks/CoreFoundation.framework AppKit CoreGraphics}.each do |fname|
      assert_equal(true, OSX.framework_loaded?(fname), fname)
    end
    %w{AddressBook /System/Library/Frameworks/OpenGL.framework InstantMessage}.each do |fname|
      assert_equal(false, OSX.framework_loaded?(fname), fname)
    end
    %w{DoesNotExist /System/Library/Frameworks/DoesNotExist.framework}.each do |fname|
      assert_raises(ArgumentError, fname) do
        OSX.framework_loaded?(fname)
      end
    end
  end

  def test_foundation_only
    # AppKit/CoreGraphics aren't loaded if you use osx/foundation.
    assert_equal('false', __spawn_line("require 'osx/foundation'; p OSX.framework_loaded?('AppKit')"))
    assert_equal('false', __spawn_line("require 'osx/foundation'; p OSX.framework_loaded?('CoreGraphics')"))
  end

  def test_bs_files_but_already_loaded_framework
    assert_equal('true', __spawn_line("require 'osx/foundation'; OSX::NSBundle.bundleWithPath('/System/Library/Frameworks/AddressBook.framework').load; OSX.require_framework 'AddressBook'; p OSX.const_defined?(:ABMultipleValueSelection)"))
  end

  def test_include_osx_require_framework
    assert_equal('42', __spawn_line("require 'osx/foundation'; include OSX; require_framework 'AppKit'; p 42"))
  end

  def __spawn_line(line)
    res = `DYLD_FRAMEWORK_PATH=../framework/build/Default #{@ruby_path} -I../lib -I../ext/rubycocoa -e \"#{line}\"`
    raise "Can't spawn Ruby line: '#{line}'" unless $?.success?
    return res.strip
  end

  # reload_protocol() in BridgeSupport.m
  # this test should be failure on 10.4 (not supported)
  def test_bs_reload_protocol
    # NSCopying (known protocol)
    assert_not_nil(OSX.lookup_informal_protocol_method_type('copyWithZone:', false))
    # new protocol introduced by dynamic libraries
    assert_nil(OSX.lookup_informal_protocol_method_type('tc_instmWithArg1:arg2:arg3:', false))
    assert_nil(OSX.lookup_informal_protocol_method_type('tc_clmWithArg1:arg2:arg3:', true))
    require 'objc_proto.bundle'
    OSX.load_bridge_support_file('ObjcTest.bridgesupport')
    assert_not_nil(types = OSX.lookup_informal_protocol_method_type('tc_instmWithArg1:arg2:arg3:', false))
    assert_equal('@@:id@', types.gsub(/\d/, ''))
    assert_not_nil(types = OSX.lookup_informal_protocol_method_type('tc_clmWithArg1:arg2:arg3:', true))
    assert_equal('i@:@c*', types.gsub(/\d/, ''))
  end

end

