#
#  MyOpenGLView.rb
#  CocoaGL
#
#  Created by Laurent Sansonetti on 12/18/06.
#  Copyright (c) 2006 Apple Computer. All rights reserved.
#

class MyOpenGLView <  NSOpenGLView

	# Override NSView's initWithFrame: to specify our pixel format:
	#
	# Note: initWithFrame is called only if a "Custom View" is used in Interface BBuilder 
	# and the custom class is a subclass of NSView. For more information on resource loading
	# see: developer.apple.com (ADC Home > Documentation > Cocoa > Resource Management > Loading Resources)
  def initWithFrame(frame)
    attribs = [
      NSOpenGLPFANoRecovery,
      NSOpenGLPFAWindow,
      NSOpenGLPFAAccelerated,
      NSOpenGLPFADoubleBuffer,
      NSOpenGLPFAColorSize, 24,
      NSOpenGLPFAAlphaSize, 8,
      NSOpenGLPFADepthSize, 24,
      NSOpenGLPFAStencilSize, 8,
      NSOpenGLPFAAccumSize, 0,
      0
    ]
    fmt = NSOpenGLPixelFormat.alloc.initWithAttributes(attribs.pack('i*'))
    puts "No OpenGL pixel format" if fmt.nil?
    
    return self if initWithFrame_pixelFormat(frame, fmt)
  end

  RED_INDEX, GREEN_INDEX, BLUE_INDEX, ALPHA_INDEX = (0..3).to_a

  def awakeFromNib
    @colorIndex = ALPHA_INDEX
  end

  # Override the view's drawRect: to draw our GL content.
  def drawRect(rect)
	  glViewport(0, 0, rect.size.width, rect.size.height)
    
    clear_color = [0.0, 0.0, 0.0, 0.0]
    clear_color[@colorIndex] = 1.0
    
	  glClearColor(*clear_color)
  	glClear(GL_COLOR_BUFFER_BIT + GL_DEPTH_BUFFER_BIT + GL_STENCIL_BUFFER_BIT)

    openGLContext.flushBuffer
  end
  
  # The UI buttons are targetted to call this action method:
  ib_action :setClearColor do |sender|
    @colorIndex = sender.tag
    setNeedsDisplay(true)
  end
end
