#  RTW.rb
#  Apple's RoundTransparentWindow in Ruby
#
#  Created by Tim Burks on 2/28/06.
#  Copyright (c) 2006 Tim Burks, Neon Design Technology, Inc. 
#
#  The contents of this file are released under the terms of the MIT License.
#  Please see the associated README file for details.

class Controller < OSX::NSObject
    ib_outlet :itsWindow

    def changeTransparency(sender)
       @itsWindow.setAlphaValue(sender.floatValue)
       # Ruby objects have their own display method that prevents the Cocoa method from being called!
       # Here we use the oc_ prefix to forcibly call the Cocoa (Objective-C) method.
       # Watch out for this: the bridge gives you NO WARNING when it happens.
       @itsWindow.oc_display
    end
    ib_action :changeTransparency 
end

class CustomWindow < OSX::NSWindow
    attr_accessor :initialLocation

    def initWithContentRect_styleMask_backing_defer(contentRect, aStyle, bufferingType, flag)
	result = super_initWithContentRect_styleMask_backing_defer_(contentRect, OSX::NSBorderlessWindowMask, OSX::NSBackingStoreBuffered, false)
	result.setBackgroundColor(OSX::NSColor.clearColor)
	result.setLevel(OSX::NSStatusWindowLevel)
	result.setAlphaValue(1.0)
	result.setOpaque(false)
	result.setHasShadow(true)
	result
    end

    def canBecomeKeyWindow
	true
    end
    
    def mouseDragged(theEvent)
	screenFrame = OSX::NSScreen.mainScreen.frame
	windowFrame = self.frame
	currentLocation = self.convertBaseToScreen(self.mouseLocationOutsideOfEventStream)
	newOrigin = OSX::NSPoint.new(currentLocation.x - @initialLocation.x, currentLocation.y - @initialLocation.y)
	# Don't let the window get dragged up under the menu bar
	if((newOrigin.y + windowFrame.size.height) > (screenFrame.origin.y + screenFrame.size.height))
	    newOrigin.y = screenFrame.origin.y + (screenFrame.size.height - windowFrame.size.height)
	end
	self.setFrameOrigin(newOrigin)
    end
    
    def mouseDown(theEvent)
	windowFrame = frame
	@initialLocation = convertBaseToScreen(theEvent.locationInWindow);
	@initialLocation.x -= windowFrame.origin.x;
	@initialLocation.y -= windowFrame.origin.y;
    end
end

class CustomView < OSX::NSView
    def awakeFromNib
	@circleImage = OSX::NSImage.imageNamed "circle"
	@pentaImage = OSX::NSImage.imageNamed "pentagon"
	setNeedsDisplay(true)
    end
        
    def acceptsFirstMouse(event)
	true
    end
    
    def drawRect(rect)
	OSX::NSColor.clearColor.set
	OSX::NSRectFill(frame) 
	if window.alphaValue > 0.7
	    @circleImage.compositeToPoint_operation_([0,0], OSX::NSCompositeSourceOver)
	else
	    @pentaImage.compositeToPoint_operation_([0,0], OSX::NSCompositeSourceOver)
	end
	window.invalidateShadow
    end
end
