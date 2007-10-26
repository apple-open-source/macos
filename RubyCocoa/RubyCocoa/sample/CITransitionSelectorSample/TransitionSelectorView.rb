#
#  TransitionSelectorView.rb
#  CITransitionSelectorSample
#
#  Created by Laurent Sansonetti on 12/13/06.
#  Copyright (c) 2006 Apple Computer. All rights reserved.
#

class TransitionSelectorView <  NSView

  def awakeFromNib
    @thumbnailWidth  = 340.0
    @thumbnailHeight = 240.0
    @thumbnailGap    = 20.0

    @sourceImage = CIImageFromResources('Rose')
    @targetImage = CIImageFromResources('Frog')

    timer = NSTimer.scheduledTimerWithTimeInterval_target_selector_userInfo_repeats(
      1.0 / 30.0, self, 'timerFired:', nil, true)
    
    @base = NSDate.timeIntervalSinceReferenceDate

    crl = NSRunLoop.currentRunLoop
    crl.addTimer_forMode(timer, NSDefaultRunLoopMode)
    crl.addTimer_forMode(timer, NSEventTrackingRunLoopMode)
  end

  def shadingImage
    @shadingImage ||= CIImageFromResources('Shading', 'tiff')
  end
  
  def blankImage
    @shadingImage ||= CIImageFromResources('Blank')
  end
  
  def maskImage
    @maskImage ||= CIImageFromResources('Mask')
  end

  def timerFired(sender)
    setNeedsDisplay(true)
  end

  def imageForTransition_atTime(transitionNumber, time)
    transition = @transitions[transitionNumber]
    
    images = [@sourceImage, @targetImage]
    images.reverse! unless (time % 2.0) < 1.0
    
    transition.setValue_forKey(images.first, 'inputImage')
    transition.setValue_forKey(images.last, 'inputTargetImage')
    
    f = 0.5 * (1 - Math.cos((time % 1.0) * Math::PI))
    transition.setValue_forKey(NSNumber.numberWithFloat(f), 'inputTime')

    crop = CIFilter.filterWithName('CICrop')
    crop.setValue_forKey(transition.valueForKey('outputImage'), 'inputImage')
    crop.setValue_forKey(CIVector.vectorWithX_Y_Z_W(0, 0, @thumbnailWidth, @thumbnailHeight), 'inputRectangle')
  
    return crop.valueForKey('outputImage')
  end

  def drawRect(rectangle)
    thumbFrame = CGRect.new(CGPoint.new(0, 0), CGSize.new(@thumbnailWidth, @thumbnailHeight))
    t = 0.4 * (NSDate.timeIntervalSinceReferenceDate - @base)
    
    context = NSGraphicsContext.currentContext.CIContext
    
    setupTransitions unless @transitions
    
    w = Math.sqrt(@transitions.length).ceil
    
    @transitions.length.times do |i|
      point = CGPoint.new(((i % w) * (@thumbnailWidth  + @thumbnailGap)), 
                          ((i / w) * (@thumbnailHeight  + @thumbnailGap)))
      
      if context
        context.drawImage_atPoint_fromRect(imageForTransition_atTime(i, t + 0.1 * i), point, thumbFrame)
      end
    end
  end

  def CIImageFromResources(name, ext='jpg')
    path = NSBundle.mainBundle.pathForResource_ofType(name, ext) 
    CIImage.imageWithContentsOfURL(NSURL.fileURLWithPath(path))
  end

  def setupTransitions
    w, h = @thumbnailWidth, @thumbnailHeight
    extent = CIVector.vectorWithX_Y_Z_W(0, 0, w, h)
    @transitions = []
    
    filter = CIFilter.filterWithName('CISwipeTransition')
    filter.setValue_forKey(CIColor.colorWithRed_green_blue_alpha(0, 0, 0, 0), 'inputColor')
    filter.setValue_forKey(NSNumber.numberWithFloat(0.3 * Math::PI), 'inputAngle')
    filter.setValue_forKey(NSNumber.numberWithFloat(80.0), 'inputWidth')
    filter.setValue_forKey(NSNumber.numberWithFloat(0.0), 'inputOpacity')
    @transitions << filter
    
    @transitions << CIFilter.filterWithName('CIDissolveTransition')
  
    filter = CIFilter.filterWithName('CISwipeTransition')
    filter.setValue_forKey(CIColor.colorWithRed_green_blue_alpha(0, 0, 0, 0), 'inputColor')
    filter.setValue_forKey(NSNumber.numberWithFloat(Math::PI), 'inputAngle')
    filter.setValue_forKey(NSNumber.numberWithFloat(2.0), 'inputWidth')
    filter.setValue_forKey(NSNumber.numberWithFloat(0.2), 'inputOpacity')
    @transitions << filter

    filter = CIFilter.filterWithName('CIModTransition')
    filter.setValue_forKey(CIVector.vectorWithX_Y(0.5 * w, 0.5 * h), 'inputCenter')
    filter.setValue_forKey(NSNumber.numberWithFloat(0.1 * Math::PI), 'inputAngle')
    filter.setValue_forKey(NSNumber.numberWithFloat(30.0), 'inputRadius')
    filter.setValue_forKey(NSNumber.numberWithFloat(10.0), 'inputCompression')
    @transitions << filter

    filter = CIFilter.filterWithName('CIFlashTransition')
    filter.setValue_forKey(extent, 'inputExtent')
    filter.setValue_forKey(CIVector.vectorWithX_Y(0.3 * w, 0.7 * h), 'inputCenter')
    filter.setValue_forKey(CIColor.colorWithRed_green_blue_alpha(1, 0.8, 0.6, 1), 'inputColor')
    filter.setValue_forKey(NSNumber.numberWithFloat(2.5), 'inputMaxStriationRadius')
    filter.setValue_forKey(NSNumber.numberWithFloat(0.5), 'inputStriationStrength')
    filter.setValue_forKey(NSNumber.numberWithFloat(1.37), 'inputStriationContrast')
    filter.setValue_forKey(NSNumber.numberWithFloat(0.85), 'inputFadeThreshold')
    @transitions << filter

    filter = CIFilter.filterWithName('CIDisintegrateWithMaskTransition')
    filter.setValue_forKey(self.maskImage, 'inputMaskImage')
    filter.setValue_forKey(NSNumber.numberWithFloat(10.0), 'inputShadowRadius')
    filter.setValue_forKey(NSNumber.numberWithFloat(0.7), 'inputShadowDensity')
    filter.setValue_forKey(CIVector.vectorWithX_Y(0.0, -0.05 * h), 'inputShadowOffset')
    @transitions << filter

    filter = CIFilter.filterWithName('CIRippleTransition')
    filter.setValue_forKey(extent, 'inputExtent')
    filter.setValue_forKey(self.shadingImage, 'inputShadingImage')
    filter.setValue_forKey(CIVector.vectorWithX_Y(0.5 * w, 0.5 * h), 'inputCenter')
    filter.setValue_forKey(NSNumber.numberWithFloat(80.0), 'inputWidth')
    filter.setValue_forKey(NSNumber.numberWithFloat(30.0), 'inputScale')
    @transitions << filter

    filter = CIFilter.filterWithName('CICopyMachineTransition')
    filter.setValue_forKey(extent, 'inputExtent')
    filter.setValue_forKey(CIColor.colorWithRed_green_blue_alpha(0.6, 1, 0.8, 1), 'inputColor')
    filter.setValue_forKey(NSNumber.numberWithFloat(0), 'inputAngle')
    filter.setValue_forKey(NSNumber.numberWithFloat(40.0), 'inputWidth')
    filter.setValue_forKey(NSNumber.numberWithFloat(1.0), 'inputOpacity')
    @transitions << filter

    filter = CIFilter.filterWithName('CIPageCurlTransition')
    filter.setValue_forKey(extent, 'inputExtent')
    filter.setValue_forKey(self.shadingImage, 'inputShadingImage')
    filter.setValue_forKey(self.blankImage, 'inputBacksideImage')
    filter.setValue_forKey(NSNumber.numberWithFloat(-0.2 * Math::PI), 'inputAngle')
    filter.setValue_forKey(NSNumber.numberWithFloat(70.0), 'inputRadius')
    @transitions << filter
  end

end
