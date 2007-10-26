#
#  SampleCIView.rb
#  CIMicroPaint
#
#  Created by Laurent Sansonetti on 12/13/06.
#  Copyright (c) 2006 Apple Computer. All rights reserved.
#

if __FILE__ == $0
  puts "This script is just a class that cannot be used directly. Please use the RubyCIMicroPaint or RubyCIBevelSample examples that use it."
  exit 1
end

class NSRect
  def to_cgrect
    OSX::CGRect.new(
      OSX::CGPoint.new(origin.x, origin.y), 
      OSX::CGSize.new(size.width, size.height))
  end
end

class CGRect
  def to_nsrect
    OSX::NSRect.new(
      OSX::NSPoint.new(origin.x, origin.y), 
      OSX::NSSize.new(size.width, size.height))
  end
end

# Simple OpenGL based CoreImage view 
class SampleCIView < NSOpenGLView
  attr_reader :image

  def self.defaultPixelFormat
    attributes = [NSOpenGLPFAAccelerated, NSOpenGLPFANoRecovery, NSOpenGLPFAColorSize, 32, 0]
    @pf ||= NSOpenGLPixelFormat.alloc.initWithAttributes(attributes.pack('i*'))
  end

  def setImage_dirtyRect(image, r)
    if @image != image
      @image = image
      if r
        setNeedsDisplayInRect(r.to_nsrect)
      else
        setNeedsDisplay(true)
      end
    end
  end

  def setImage(image)
    setImage_dirtyRect(image, nil)
  end

  def prepareOpenGL
    # Enable beam-synced updates
    openGLContext.setValues_forParameter([1].pack('l'), NSOpenGLCPSwapInterval)
    
    # Make sure that everything we don't need is disabled. Some of these
    # are enabled by default and can slow down rendering. 
    glDisable(GL_ALPHA_TEST)
    glDisable(GL_DEPTH_TEST)
    glDisable(GL_SCISSOR_TEST)
    glDisable(GL_BLEND)
    glDisable(GL_DITHER)
    glDisable(GL_CULL_FACE)
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE)
    glDepthMask(GL_FALSE)
    glStencilMask(0)
    glClearColor(0.0, 0.0, 0.0, 0.0)
    glHint(GL_TRANSFORM_HINT_APPLE, GL_FASTEST)
  end

  def viewBoundsDidChange(bounds)
    # For subclasses.
  end
  addRubyMethod_withType('viewBoundsDidChange:', '@:@')

  def updateMatrices
    r = self.bounds
    
    if @lastBounds.nil? or !NSEqualRects(r, @lastBounds)
      openGLContext.update
      
      # Install an orthographic projection matrix (no perspective)
      # with the origin in the bottom left and one unit equal to one
      # device pixel.
      glViewport(0, 0, r.size.width, r.size.height)

      glMatrixMode(GL_PROJECTION)
      glLoadIdentity()
      glOrtho(0, r.size.width, 0, r.size.height, -1, 1)

      glMatrixMode(GL_MODELVIEW)
      glLoadIdentity()

      @lastBounds = r;
      viewBoundsDidChange(r)
    end
  end

  def drawRect(r)
    openGLContext.makeCurrentContext

    # Allocate a CoreImage rendering context using the view's OpenGL
    # context as its destination if none already exists.
    if @context.nil?
      pf = pixelFormat
      pf = SampleCIView.defaultPixelFormat if pf.nil?

      @context = CIContext.contextWithCGLContext_pixelFormat_options(
        CGLGetCurrentContext(), pf.CGLPixelFormatObj, nil)
    end
    
    ir = CGRectIntegral(r.to_cgrect)
    
    if NSGraphicsContext.currentContextDrawingToScreen
      updateMatrices

      # Clear the specified subrect of the OpenGL surface then
      # render the image into the view. Use the GL scissor test to
      # clip to * the subrect. Ask CoreImage to generate an extra
      # pixel in case * it has to interpolate (allow for hardware
      # inaccuracies).
      rr = CGRectIntersection(CGRectInset(ir, -1.0, -1.0), @lastBounds.to_cgrect)

      glScissor(ir.origin.x, ir.origin.y, ir.size.width, ir.size.height)
      glEnable(GL_SCISSOR_TEST)

      glClear(GL_COLOR_BUFFER_BIT)

      if respondsToSelector('drawRect:inCIContext:')
        drawRect_inCIContext(rr.to_nsrect, @context)
      elsif @image 
        @context.drawImage_atPoint_fromRect(@image, rr.origin, rr)
      end

      glDisable(GL_SCISSOR_TEST)

      # Flush the OpenGL command stream. If the view is double
      # buffered this should be replaced by [[self openGLContext]
      # flushBuffer]. 
      glFlush
    else
      # Printing the view contents. Render using CG, not OpenGL.
      if respondsToSelector('drawRect:inCIContext:')
        drawRect_inCIContext(ir.to_nsrect, @context)
      elsif @image
        cgImage = @context.createCGImage_fromRect(@image, ir)
        CGContextDrawImage(
          NSGraphicsContext.currentContext.graphicsPort,
          ir, cgImage) if cgImage
      end
    end
  end
end
