require 'osx/cocoa'

class DotView < OSX::NSView

  ib_outlets   :colorWell, :sizeSlider

  def awakeFromNib
    @color = OSX::NSColor.redColor
    @radius = 10.0
    @center = OSX::NSPoint.new(bounds.size.width / 2,
			       bounds.size.height / 2)
    @colorWell.setColor(@color)
    @sizeSlider.setFloatValue(@radius)
  end

  def drawRect (rect)
    OSX::NSColor.whiteColor.set
    OSX::NSRectFill(bounds)
    dot_rect = OSX::NSRect.new(@center.x - @radius, @center.y - @radius,
			       2 * @radius, 2 * @radius)
    @color.set
    OSX::NSBezierPath.bezierPathWithOvalInRect(dot_rect).fill
  end

  def isOpaque
    true
  end

  def mouseUp (event)
    @center = convertPoint(event.locationInWindow, :fromView, nil)
    setNeedsDisplay true
  end

  def setColor (sender)
    @color = sender.color
    setNeedsDisplay true
  end
  ib_action :setColor

  def setRadius (sender)
    @radius = sender.floatValue
    setNeedsDisplay true
  end
  ib_action :setRadius

end
