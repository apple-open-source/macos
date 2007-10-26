#
#  CIMicroPaintView.rb
#  CIMicroPaint
#
#  Created by Laurent Sansonetti on 12/13/06.
#  Copyright (c) 2006 Apple Computer. All rights reserved.
#

require 'SampleCIView'

class CIMicroPaintView < SampleCIView
  attr_accessor :color, :brushSize

  def initWithFrame(frame)
    return nil if super_initWithFrame(frame).nil?
    
    @brushSize = 25.0
    @color = NSColor.colorWithDeviceRed_green_blue_alpha(0.0, 0.0, 0.0, 1.0)
    
    @brushFilter = CIFilter.filterWithName('CIRadialGradient')
    @brushFilter.setValue_forKey(CIColor.colorWithRed_green_blue_alpha(0.0, 0.0, 0.0, 0.0), 'inputColor1')
    @brushFilter.setValue_forKey(NSNumber.numberWithFloat(0.0), 'inputRadius0')
    @compositeFilter = CIFilter.filterWithName('CISourceOverCompositing')
    
    return self
  end

  def brushSize=(brushSize)
    if brushSize.respondsToSelector?('floatValue')
      @brushSize = brushSize.floatValue
    else
      @brushSize = brushSize
    end
  end

  def viewBoundsDidChange(bounds)
    cg_bounds = bounds.to_cgrect
    return if @imageAccumulator and CGRectEqualToRect(cg_bounds, @imageAccumulator.extent)
    
    # Create a new accumulator and composite the old one over the it.
    c = CIImageAccumulator.alloc.initWithExtent_format(cg_bounds, KCIFormatRGBA16)
    f = CIFilter.filterWithName('CIConstantColorGenerator')
    f.setValue_forKey(CIColor.colorWithRed_green_blue_alpha(1.0, 1.0, 1.0, 1.0), 'inputColor')
    c.setImage(f.valueForKey('outputImage'))
    
    if @imageAccumulator
      f = CIFilter.filterWithName('CISourceOverCompositing')
      f.setValue_forKey(@imageAccumulator.image, 'inputImage')
      f.setValue_forKey(c.image, 'inputBackgroundImage')
      c.setImage(f.valueForKey('outputImage'))
    end
    
    @imageAccumulator = c
    
    setImage(@imageAccumulator.image)
  end

  def mouseDragged(event)
    loc = convertPoint_fromView(event.locationInWindow, nil)
    rect = CGRect.new(CGPoint.new(loc.x - @brushSize, loc.y - @brushSize),
                      CGSize.new(2.0 * @brushSize, 2.0 * @brushSize))
    
    @brushFilter.setValue_forKey(NSNumber.numberWithFloat(@brushSize), 'inputRadius1')
    cicolor = CIColor.alloc.initWithColor(@color)
    @brushFilter.setValue_forKey(cicolor, 'inputColor0')
    @brushFilter.setValue_forKey(CIVector.vectorWithX_Y(loc.x, loc.y), 'inputCenter')
    
    @compositeFilter.setValue_forKey(@brushFilter.valueForKey('outputImage'), 'inputImage')    
    @compositeFilter.setValue_forKey(@imageAccumulator.image, 'inputBackgroundImage')
  
    @imageAccumulator.setImage_dirtyRect(@compositeFilter.valueForKey('outputImage'), rect)
    setImage_dirtyRect(@imageAccumulator.image, rect)
  end

  def mouseDown(event)
    mouseDragged(event)
  end

end
