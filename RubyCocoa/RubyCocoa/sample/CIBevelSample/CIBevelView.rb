#
#  CIBevelView.rb
#  CIBevelSample
#
#  Created by Laurent Sansonetti on 12/15/06.
#  Copyright (c) 2006 Apple Computer. All rights reserved.
#

require 'SampleCIView'

class CIBevelView < SampleCIView

  def initWithFrame(frameRect)
    return nil if super_initWithFrame(frameRect).nil?
    
    @points = []
    @points << CGPoint.new(0.5 * frameRect.size.width, frameRect.size.height - 100.0)
    @points << CGPoint.new(150.0, 100.0)
    @points << CGPoint.new(frameRect.size.width - 150.0, 100.0)
    @points << CGPoint.new(0.7 * @points[0].x + 0.3 * @points[2].x, 0.7 * @points[0].y + 0.3 * @points[2].y)

    url = NSURL.fileURLWithPath(NSBundle.mainBundle.pathForResource_ofType('lightball', 'tiff'))
    lightball = CIImage.imageWithContentsOfURL(url)
    
    @heightFieldFilter = CIFilter.filterWithName('CIHeightFieldFromMask')
    @heightFieldFilter.setValue_forKey(NSNumber.numberWithFloat(15.0), 'inputRadius')
    
    @twirlFilter = CIFilter.filterWithName('CITwirlDistortion')
    @twirlFilter.setValue_forKey(CIVector.vectorWithX_Y(0.5 * frameRect.size.width, 0.5 * frameRect.size.height), 'inputCenter')
    @twirlFilter.setValue_forKey(NSNumber.numberWithFloat(300.0), 'inputRadius')
    @twirlFilter.setValue_forKey(NSNumber.numberWithFloat(0.0), 'inputAngle')

    @shadedFilter = CIFilter.filterWithName('CIShadedMaterial')
    @shadedFilter.setValue_forKey(lightball, 'inputShadingImage')
    @shadedFilter.setValue_forKey(NSNumber.numberWithFloat(20.0), 'inputScale')

    # 1/30 second should give us decent animation
    @angleTime = 0
    @currentPoint = 0
    NSTimer.scheduledTimerWithTimeInterval_target_selector_userInfo_repeats(
      1.0 / 30.0, self, 'changeTwirlAngle:', nil, true)

    return self
  end

  def changeTwirlAngle(timer)
    @angleTime += timer.timeInterval
    @twirlFilter.setValue_forKey(NSNumber.numberWithFloat(-0.2 * Math.sin(@angleTime * 5.0)), 'inputAngle')
    updateImage
  end
  
  def mouseDragged(event)
    loc = convertPoint_fromView(event.locationInWindow, nil)
    point = @points[@currentPoint]
    point.x = loc.x
    point.y = loc.y
    @lineImage = nil

    # normally we'd want this, but the timer will cause us to redisplay anyway
    setNeedsDisplay(true)
  end

  def mouseDown(event)
    loc = convertPoint_fromView(event.locationInWindow, nil)
    d = 1e4
    @points.each_with_index do |point, i|
      x = point.x - loc.x
      y = point.y - loc.y
      t = x * x + y * y
      
      if t < d
        @currentPoint = i
        d = t
      end
    end
    mouseDragged(event)
  end

  def updateImage
    context = NSGraphicsContext.currentContext.CIContext
    unless @lineImage
      bounds = self.bounds
      layer = context.createCGLayerWithSize_info(CGSize.new(bounds.size.width, bounds.size.height), nil)
      cg = CGLayerGetContext(layer)
      
      CGContextSetRGBStrokeColor(cg, 1,1,1,1)
      CGContextSetLineCap(cg, KCGLineCapRound)

      CGContextSetLineWidth(cg, 60.0)
      CGContextMoveToPoint(cg, @points[0].x, @points[0].y)
      @points[1..-1].each { |point| CGContextAddLineToPoint(cg, point.x, point.y) }
      CGContextStrokePath(cg);

      @lineImage = CIImage.alloc.initWithCGLayer(layer)
    end

    @heightFieldFilter.setValue_forKey(@lineImage, 'inputImage')
    @twirlFilter.setValue_forKey(@heightFieldFilter.valueForKey('outputImage'), 'inputImage')
    @shadedFilter.setValue_forKey(@twirlFilter.valueForKey('outputImage'), 'inputImage')
    
    setImage(@shadedFilter.valueForKey('outputImage'))
  end
end
