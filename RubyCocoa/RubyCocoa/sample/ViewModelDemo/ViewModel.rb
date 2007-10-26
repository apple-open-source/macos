# This is an example of an combination object: model and view. A number of these are
# created and added to the superview (see Controller).

require 'osx/cocoa'

include OSX

SQUARE_SIZE = 50
STROKE_WIDTH = 10
SHADOW_OFFSET = 5
SHADOW_BLUR = 3

class ViewModel <  NSView
	
	attr_reader :speed

  def initWithFrame(frame)
    super_initWithFrame(frame)
		
		@stroke = STROKE_WIDTH
		@color = getRandomColor
		@speed = 0
		setupShadows
		setupDrawRect
		
    return self
  end
	
	def setNum(n)
		@num = n
	end
	
	def setupShadows
		# The shadow will automagically "mask" any white part in a drawn image.
		# Shadows also work with drawn paths, but leave room for the shadow in the frame
		# (if object drawing fills frame, then shadow won't be visible).
		
		@onShadow = NSShadow.alloc.init
		offset = NSSize.new(SHADOW_OFFSET, -SHADOW_OFFSET)
		@onShadow.setShadowOffset(offset) # required to get a shadow.
		@onShadow.setShadowBlurRadius(SHADOW_BLUR) # default is 0.
		# The following is not needed since it matches the default. Change if desired.
		@onShadow.setShadowColor(NSColor.blackColor.colorWithAlphaComponent(0.33)) 
		
		# For the off shadow, all that is needed is to create it. It defaults to "no shadow" settings.
		@offShadow = NSShadow.alloc.init
	end
	
	def setupDrawRect
		# The draw rect is smaller than the frame, to leave room for the shadow and the stroke width.
		@drawRect = NSRect.new(@stroke/2, SHADOW_OFFSET + @stroke/2, bounds.width - SHADOW_OFFSET - @stroke, bounds.height - SHADOW_OFFSET - @stroke)
	end
	
	def getRandomColor
		red = rand(256)/256.0 #forces float
		green = rand(256)/256.0
		blue = rand(256)/256.0
		NSColor.colorWithCalibratedRed_green_blue_alpha(red, green, blue, 1.0)
	end
	
  def drawRect(rect)
		if superview.shadowSwitch.state == 1
			@onShadow.set
		else
			@offShadow.set
		end

		@color.set
		NSBezierPath.setDefaultLineWidth(@stroke)
    NSBezierPath.strokeRect(@drawRect)
  end
	
	def mouseDown(event)
		puts "mouseDown for obj #{@num}"
		
		# It can be useful to keep this point so that the relationship (distance) between
		# the object and the pointer can be maintained during a mouseDragged event.
		# Object-local coordinates.
		@mouseDownPoint = convertPoint_fromView(event.locationInWindow, nil)
				
		# Put this subview on top, if control is set.
		if superview.moveToTopSwitch.state == 1
			superview.moveSubviewToTop(self)
			# or, to move it behind the others:
			#superview.moveSubviewToIndex(self, 0)
		end
	end
	
	def mouseDragged(event)
		myPoint = NSPoint.new(event.locationInWindow.x - superview.frame.origin.x, 
													event.locationInWindow.y - superview.frame.origin.y)
		myPoint.x -= @mouseDownPoint.x
		myPoint.y -= @mouseDownPoint.y
		
		if @speed > 0 # was in the process of animating to a new location
			@speed = 0 # stop the movement
		end
		
		# There is no need to redraw all the objects during a drag.
		# This method keeps dragging very smooth, even with lots of objects on the screen.
		# Mark the old rect as needing an update...
		superview.setNeedsDisplayInRect(frame)
		# ...update the position...
		setFrameOrigin(myPoint)
		# ...and mark the new one too.
		superview.setNeedsDisplayInRect(frame)
	end
	
	def setMoveDestination(destPt)
		if destPt.x != frame.x or destPt.y != frame.y
			@destPt = destPt
		end
	end
	
	def setSpeed(speed)
		@speed = speed
		@angle = getAngleInRadians(frame.origin, @destPt)
		@xDelta = Math.sin(@angle) * @speed
		@yDelta = Math.cos(@angle) * @speed
	end
	
	def startMovement
		# Set the global so the app knows that an object is moving
		$objMoving = true
		# start the timer (superview checks to see if it is already going)
		superview.startTimer
	end
	
	def moveToDestination
		return 0 if speed == 0
		currPt = frame.origin
		if currPt == @destPt  # reached destination
			@speed = 0
			return 0
		end
		
		if (currPt.x - @destPt.x).abs < @speed
			currPt.x = @destPt.x
		else
			currPt.x += @xDelta
		end
		
		if (currPt.y - @destPt.y).abs < @speed
			currPt.y = @destPt.y
		else
			currPt.y += @yDelta
		end
		
		# Unlike with dragging, during animation it is inefficient for each object
		# to call "setNeedsDisplay" as it moves. It is much better to just update the
		# object's position and let the superview call setNeedsDisplay once. 
		setFrameOrigin(currPt)
		
		return 1 # yes, this object is still moving
	end
	
	def getAngleInRadians(p1, p2) # points
		x = p2.x - p1.x
		y = p2.y - p1.y
		Math.atan2(x,y)
	end
end

