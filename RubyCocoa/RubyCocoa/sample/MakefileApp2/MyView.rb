require 'osx/cocoa'

class MyView < OSX::NSView

  def awakeFromNib
    @color_name = 'blue'
  end

  def drawRect(frame)
    color_set = "OSX::NSColor.#{@color_name}Color.set"
    eval(color_set)
    OSX.NSRectFill(bounds)
  end

  def set_color(color_name)
    @color_name = color_name.to_s.downcase
    setNeedsDisplay true
  end

end
