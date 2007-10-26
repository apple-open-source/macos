#
#  StickyWindow.rb
#  Stickies
#
#  Created by Laurent Sansonetti on 1/4/07.
#  Copyright (c) 2007 Apple Computer. All rights reserved.
#

class StickyWindow < NSWindow

  def initWithContentRect_styleMask_backing_defer(contentRect, styleMask, backingType, flag)
    if super_initWithContentRect_styleMask_backing_defer(contentRect, NSBorderlessWindowMask, backingType, flag)
      setBackgroundColor(NSColor.yellowColor)
      setHasShadow(true)
      return self
    end
  end
  
  def canBecomeKeyWindow; true; end

end
