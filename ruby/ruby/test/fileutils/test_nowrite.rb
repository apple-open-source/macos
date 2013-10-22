# $Id: test_nowrite.rb 40251 2013-04-11 17:00:49Z nagachika $

require 'fileutils'
require 'test/unit'
require_relative 'visibility_tests'

class TestFileUtilsNoWrite < Test::Unit::TestCase

  include FileUtils::NoWrite
  include TestFileUtils::Visibility

  def setup
    super
    @fu_module = FileUtils::NoWrite
  end

end
