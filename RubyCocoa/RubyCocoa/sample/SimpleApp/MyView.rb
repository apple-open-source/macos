#
#  MyView.rb
#  SimpleApp
#
#  Created by FUJIMOTO Hisakuni on Sat Sep 07 2002.
#  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
#

require 'osx/cocoa'

class MyView <  OSX::NSView
  include OSX

  attr_reader :color, :alpha

  def initWithFrame (frame)
    super_initWithFrame(frame)
    @alpha = 0.5
    @color = NSColor.blueColor.colorWithAlphaComponent(@alpha)
    return self
  end

  def drawRect (rect)
    @color.set
    NSRectFill(rect)
  end

  def set_color (color)
    @color = color.colorWithAlphaComponent(@alpha)
    setNeedsDisplay(true)
  end

  def set_alpha (alpha)
    @alpha = alpha
    @color = @color.colorWithAlphaComponent(@alpha)
    setNeedsDisplay(true)
  end

end
