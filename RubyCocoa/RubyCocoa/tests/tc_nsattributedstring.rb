#
#  $Id: tc_nsattributedstring.rb 1111 2006-09-24 13:15:53Z lrz $
#
#  Copyright (c) 2001-2002 FUJIMOTO Hisakuni
#

require 'test/unit'
require 'osx/cocoa'

class TC_NSAttributedString < Test::Unit::TestCase

  STR = 'Hello World'
  HTML_A = "<h1>#{STR}</h1>".freeze # 
  HTML_B = "<a href='hello'>#{STR}</a>".freeze #

  def test_s_alloc
    obj = OSX::NSAttributedString.alloc.init
    assert_equal( true, obj.isKindOfClass?( OSX::NSAttributedString ))
  end

  def test_initWithHTML
    # 2nd arg nil
    data = OSX::NSData.dataWithBytes_length(HTML_A)
    obj = OSX::NSAttributedString.alloc.initWithHTML_documentAttributes(data, nil)
    assert_match(/^#{STR}/, obj.to_s) #

    # 2nd arg assign
    data = OSX::NSData.dataWithBytes_length(HTML_B)
    arg1 = OSX::OCObject.new
    obj = OSX::NSAttributedString.alloc.initWithHTML_documentAttributes(data, arg1)
    assert_match(/^#{STR}/, obj.to_s) #
    assert_equal(true, arg1.isKindOfClass?(OSX::NSDictionary))

    # illegal 2nd arg
    data = OSX::NSData.dataWithBytes_length(HTML_B)
    arg1 = Object.new
    assert_raises(OSX::OCDataConvException) do
      obj = OSX::NSAttributedString.alloc.initWithHTML_documentAttributes(data, arg1)
    end
  end

end
