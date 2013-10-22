# $Id: test_verbose.rb 40251 2013-04-11 17:00:49Z nagachika $

require 'test/unit'
require 'fileutils'
require_relative 'visibility_tests'

class TestFileUtilsVerbose < Test::Unit::TestCase

  include FileUtils::Verbose
  include TestFileUtils::Visibility

  def setup
    super
    @fu_module = FileUtils::Verbose
  end

end
