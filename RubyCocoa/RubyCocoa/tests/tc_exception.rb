#
#  $Id: tc_exception.rb 1616 2007-03-02 07:56:39Z hisa $
#
#  Copyright (c) 2005 kimura wataru
#

require 'test/unit'
require 'osx/cocoa'

###class ExceptionTest < OSX::NSObject
system 'make -s' || raise(RuntimeError, "'make' failed")
require 'objc_test.bundle'

OSX.ns_import "RBExceptionTestBase" # at end of RBObject.m

class ExceptionTest < OSX::RBExceptionTestBase

  attr_reader :exception

  def rb_raise
    raise "rb_raise_message"
  end

  def ns_raise
    @exception = OSX::NSException.exceptionWithName_reason_userInfo("ns_raise_name", "ns_raise_reason", nil)
    @exception.raise
  end

  # Called from [RBExceptionTestBase testExceptionRoundTrip]
  def testExceptionRaise
    ns_raise
  end
end


class TC_Exceptions < Test::Unit::TestCase

  def setup
    @tester = ExceptionTest.alloc.init
  end

  def test_rb_raise
    with_suppress_backtrace_log { @tester.performSelector(:rb_raise) }
  rescue => err
  ensure
      assert_kind_of RuntimeError,err
      assert_equal "rb_raise_message",err.message
      assert_match /#{__FILE__}:/, err.backtrace.first
  end

  def check_nsexception(err)
    assert_kind_of OSX::OCException,err
    assert_match /ns_raise_name - ns_raise_reason/, err.message
    assert_equal "ns_raise_name", err.nsexception.name.to_s
    assert_equal "ns_raise_reason", err.nsexception.reason.to_s
    assert_match /#{__FILE__}:/, err.backtrace[2]
  end

  def test_direct_ns_raise
    with_suppress_backtrace_log { @tester.ns_raise }
  rescue => err
  ensure
    check_nsexception(err)
  end

  def test_indirect_ns_raise
    with_suppress_backtrace_log { @tester.performSelector(:ns_raise) }
  rescue => err
  ensure
    check_nsexception(err)
  end

  # This tests the condition at the beginning of 
  #   [RBObject rbobjRaiseRubyException]
  # where the original NSException is returned.
  #
  # We are checking that NSExceptions raised from
  # ObjC code are correctly wrapped and unwrapped
  # to the original exception
  def test_ns_rethrow
    with_suppress_backtrace_log do
      # This method is defined in the base class
      exc = @tester.testExceptionRoundTrip
      assert_equal @tester.exception.__ocid__, exc.__ocid__
    end
  end

  private

  def with_suppress_backtrace_log
    $RUBYCOCOA_SUPPRESS_EXCEPTION_LOGGING = true
    yield
  ensure
    $RUBYCOCOA_SUPPRESS_EXCEPTION_LOGGING = false
  end

end
