#
#  CIExposureView.rb
#  CIExposureSample
#
#  Created by Laurent Sansonetti on 12/13/06.
#  Copyright (c) 2006 Apple Computer. All rights reserved.
#

class CIExposureView <  NSView

  def sliderChanged(sender)
    @exposureValue = sender.floatValue
    setNeedsDisplay(true)
  end
  ib_action :sliderChanged
  
  def drawRect(rect)
    cg = CGRect.new(CGPoint.new(rect.origin.x, rect.origin.y), 
                    CGSize.new(rect.size.width, rect.size.height))
    
    context = NSGraphicsContext.currentContext.CIContext
    
    unless @filter
      path = NSBundle.mainBundle.pathForResource_ofType('Rose', 'jpg')
      url = NSURL.fileURLWithPath(path)
      image = CIImage.imageWithContentsOfURL(url)
      @filter = CIFilter.filterWithName('CIExposureAdjust')
      @filter.setValue_forKey(image, 'inputImage')
    end
    
    @exposureValue ||= 0.0
    @filter.setValue_forKey(NSNumber.numberWithFloat(@exposureValue), 'inputEV')
    
    context.drawImage_atPoint_fromRect(@filter.valueForKey('outputImage'), cg.origin, cg) if context
  end

end
