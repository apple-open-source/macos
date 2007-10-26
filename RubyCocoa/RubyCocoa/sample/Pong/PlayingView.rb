require 'osx/cocoa'

class PlayingView < OSX::NSView

  attr_reader :width, :height, :balls
  attr_writer :paddle

  ib_outlets :appCtrl

  def initialize
    @balls = Array.new
  end

  def awakeFromNib
    @width = bounds.size.width
    @height = bounds.size.height
  end

  def mouseDragged(evt)
    @appCtrl.mouseDragged(evt)
  end

  def drawRect(frame)
    draw_bg
    draw_paddle
    draw_balls
  end

  def draw_bg
    OSX::NSColor.whiteColor.set
    OSX::NSRectFill(bounds)
  end

  def draw_balls
    @balls.each do |ball|
      ball.color.set
      OSX::NSBezierPath.bezierPathWithOvalInRect(ball.rect).fill
    end
  end

  def draw_paddle
    if @paddle then
      @paddle.color.set
      OSX::NSRectFill(@paddle.rect)
    end
  end

end
