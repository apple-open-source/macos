#
#  StickyTitleBarView.rb
#  Stickies
#
#  Created by Laurent Sansonetti on 1/4/07.
#  Copyright (c) 2007 Apple Computer. All rights reserved.
#

class StickyTitleBarView <  NSView

  def initWithFrame(frame)
    if super_initWithFrame(frame)
      @mouseInCloseBox = @trackingCloseBoxHit = false
      return self
    end
  end
  
  # This should be calculated, not hard-wired, but this simplifies the example code
  CLOSE_BOX = NSRect.new(3, 2, 8, 8)
  
  def drawRect(rect)
    border = bounds
    NSColor.yellowColor.set
    NSRectFill(border)
    NSColor.brownColor.set
    NSFrameRect(border)
    @mouseInCloseBox ? NSRectFill(CLOSE_BOX) : NSFrameRect(CLOSE_BOX)
  end
  
  def acceptsFirstMouse; true; end
  def mouseDownCanMoveWindow; false; end

  def mouseDown(theEvent)
    point = convertPoint_fromView(theEvent.locationInWindow, nil)
    if @mouseInCloseBox = NSPointInRect(point, CLOSE_BOX)
      @trackingCloseBoxHit = true
      setNeedsDisplayInRect(CLOSE_BOX)
    elsif theEvent.clickCount > 1
      window.miniaturize(self)
    end
  end
  
  def mouseDragged(theEvent)
    if @trackingCloseBoxHit
      point = convertPoint_fromView(theEvent.locationInWindow, nil)
      @mouseInCloseBox = NSPointInRect(point, CLOSE_BOX)
      setNeedsDisplayInRect(CLOSE_BOX)
    else
      windowOrigin = window.frame.origin
      window.setFrameOrigin(
          NSPoint.new(windowOrigin.x + theEvent.deltaX, windowOrigin.y - theEvent.deltaY))
    end
  end

  def mouseUp(theEvent)
    point = convertPoint_fromView(theEvent.locationInWindow, nil)
    if NSPointInRect(point, CLOSE_BOX)
      tryToCloseWindow
    else
      @trackingCloseBoxHit = false
      setNeedsDisplayInRect(CLOSE_BOX)
    end
  end

  def tryToCloseWindow
    w = window
    if w
      d = w.delegate
      if d and d.respondsToSelector('windowShouldClose:') and !d.windowShouldClose(window)
        # Delegate exists, and it vetoed closing the window.
        return
      end
    end
    
    # Otherwise, close the window
    w.close
  end

end
