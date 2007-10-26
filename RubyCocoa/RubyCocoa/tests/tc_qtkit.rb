#
#  $Id: /remote/trunk/src/tests/tc_attachments.rb 1224 2006-11-03T17:23:39.319167Z paisleyj  $
#
#  Copyright (c) 2006 Jonathan Paisley
# 
# Eloy Duran 08-01-2007: Updated for the apple-unstable branch

require 'test/unit'
require 'osx/cocoa'
OSX.require_framework 'QTKit'

class TC_QTKit < Test::Unit::TestCase
  include OSX

  def test_String_to_qttime
    # days:hours:minutes:seconds.frames/timescale
    qtt = QTTimeFromString("01:02:03:05.007/1000")
    millis = (((1 * 24 + 2) * 60 + 3) * 60 + 5) * 1000 + 7
    
    assert qtt.is_a?(QTTime)
    assert_equal millis, qtt.timeValue
    assert_equal 1000, qtt.timeScale    
  end
  
  def test_String_to_qttime_r
    qtt1 = "01:02:03:05.007/1000"
    millis1 = (((1 * 24 + 2) * 60 + 3) * 60 + 5) * 1000 + 7
    qtt2 = "02:02:03:05.007/1000"
    millis2 = (((2 * 24 + 2) * 60 + 3) * 60 + 5) * 1000 + 7
    qttr = QTTimeRangeFromString("#{qtt1}~#{qtt2}")
    
    assert qttr.is_a?(QTTimeRange)
    assert_equal millis1, qttr.time.timeValue
    assert_equal millis2, qttr.duration.timeValue
  end
  
  def test_qtkit_loaded
    # Just check that we don't get any exceptions here
    movie = QTMovie.movie
  end
  
  def test_qtkit_qttime_passing
    movie = QTMovie.movie
    qtt = QTTimeFromString("01:02:03:05.007/1000")
    movie.setCurrentTime(qtt)
  end

end
