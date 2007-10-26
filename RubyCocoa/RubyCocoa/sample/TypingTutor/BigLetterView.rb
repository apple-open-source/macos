require 'osx/cocoa'

class BigLetterView < OSX::NSView
  include OSX

  def draggingEntered(sender)
    log "draggingEntered:"
    if sender.draggingSource != self then
      pb = sender.draggingPasteboard
      if pb.availableTypeFromArray [NSStringPboardType] then
	@highlighted = true
	setNeedsDisplay true
	return NSDragOperationCopy
      end
    end
    return NSDragOperationNone
  end

  def draggingExited(sender)
    log "draggingExited:"
    @highlighted = false
    setNeedsDisplay true
  end

  def prepareForDragOperation(sender)
    true
  end

  def performDragOperation(sender)
    pb = sender.draggingPasteboard
    unless readStringFromPasteboard(pb) then
      log "Error: Could not read from dragging pasteboard"
      return false
    end
    return true
  end

  def concludeDragOperation(sender)
    log "concludeDragOperation:"
    @highlighted = false
    setNeedsDisplay true
  end

  def initWithFrame(rect)
    super_initWithFrame(rect)
    log "initializing view"
    prepareAttributes
    setBgColor NSColor.yellowColor
    setString ""
    @bold = false
    @italic = false
    @fmgr = NSFontManager.sharedFontManager
    registerForDraggedTypes [NSStringPboardType]
    @highlighted = false
    self
  end

  def setBgColor(c)
    @bgColor = c
    setNeedsDisplay true
  end

  def setString(c)
    c = NSString.stringWithString(c) if c.is_a? String
    @string = c
    log "The string is now #{@string.to_s}"
    setNeedsDisplay true
  end

  def drawStringCenterdIn(r)
    sorig = NSPoint.new
    ssize = @string.sizeWithAttributes(@attributes)
    sorig.x = r.origin.x + (r.size.width - ssize.width) / 2
    sorig.y = r.origin.y + (r.size.height - ssize.height) / 2

    font = @attributes.objectForKey(NSFontAttributeName)

    mask = @bold ? NSBoldFontMask : NSUnboldFontMask
    font = @fmgr.convertFont_toHaveTrait(font, mask)
    @attributes.setObject_forKey(font, NSFontAttributeName)

    mask = @italic ? NSItalicFontMask : NSUnitalicFontMask
    font = @fmgr.convertFont_toHaveTrait(font, mask)
    @attributes.setObject_forKey(font, NSFontAttributeName)

    @string.drawAtPoint_withAttributes(sorig, @attributes)
  end

  def drawRect(rect)
    b = bounds
    (@highlighted ? NSColor.whiteColor : @bgColor).set
    NSBezierPath.fillRect(b)
    drawStringCenterdIn(b)
    if window.firstResponder == self then
      NSColor.blackColor.set
      NSBezierPath.strokeRect(b)
    end
  end

  def acceptsFirstResponder
    log "Accepting"
    true
  end

  def resignFirstResponder
    log "Resigning"
    setNeedsDisplay true
    true
  end

  def becomeFirstResponder
    log "Becoming"
    setNeedsDisplay true
    true
  end

  def keyDown(event)
    input = event.characters
    if input.isEqual? "\t" then
      window.selectNextKeyView(nil)
    elsif input.isEqual? "\031" then
      window.selectPreviousKeyView(nil)
    else
      setString(input)
    end
  end

  def prepareAttributes
    @attributes = NSMutableDictionary.alloc.init
    @attributes.
      setObject_forKey(NSFont.fontWithName_size("Helvetica", 75),
		       NSFontAttributeName)
    @attributes.
      setObject_forKey(NSSingleUnderlineStyle,
		       NSUnderlineStyleAttributeName)
    @attributes.
      setObject_forKey(NSColor.redColor,
		       NSForegroundColorAttributeName)
  end

  def savePDF(sender)
    panel = NSSavePanel.savePanel
    panel.setRequiredFileType "pdf"
    panel.
      objc_send :beginSheetForDirectory, nil,
                                  :file, nil,
                        :modalForWindow, window,
                         :modalDelegate, self,
                        :didEndSelector, "didEnd_returnCode_contextInfo_",
                           :contextInfo, nil
  end
  ib_action :savePDF

  def didEnd_returnCode_contextInfo(sheet, code, contextInfo)
    if code == NSOKButton then
      data = dataWithPDFInsideRect(bounds)
      data.writeToFile_atomically(sheet.filename, true)
    end
  end

  def boldClicked(sender)
    @bold = (sender.state == NSOnState)
    setNeedsDisplay true
  end
  ib_action :boldClicked
  
  def italicClicked(sender)
    @italic = (sender.state == NSOnState)
    setNeedsDisplay true
  end
  ib_action :italicClicked

  def writePDFToPasteboard(pb)
    types = [NSPDFPboardType]
    pb.addTypes_owner(types, self)
    pb.setData_forType(dataWithPDFInsideRect(bounds), types[0])
  end
  
  def writeStringToPasteboard(pb)
    types = [NSStringPboardType]
    pb.declareTypes_owner(types, self)
    pb.setString_forType(@string, types[0])
  end
  
  def readStringFromPasteboard (pb)
    if pb.availableTypeFromArray? [NSStringPboardType] then
      value = pb.stringForType(NSStringPboardType)
      value = value.to_s[0].chr if value.length > 1
      setString(value)
      return true
    end
    false
  end

  def cut(sender)
    copy(sender)
    setString ""
  end
  ib_action :cut

  def copy(sender)
    pb = NSPasteboard.generalPasteboard
    writeStringToPasteboard(pb)
    writePDFToPasteboard(pb)
  end
  ib_action :copy

  def paste(sender)
    pb = NSPasteboard.generalPasteboard
    NSBeep unless readStringFromPasteboard(pb)
  end
  ib_action :paste

  def draggingSourceOperationMaskForLocal(flag)
    NSDragOperationCopy
  end

  def mouseDragged(event)
    # create the image that will be dragged and
    # a rect in which you will draw the letter in the image
    s = @string.sizeWithAttributes(@attributes)
    return if s.width == 0
    an_image = NSImage.alloc.initWithSize(s)
    image_bounds = NSRect.new([0,0],s)

    # draw the letter on the image
    an_image.lockFocus
    drawStringCenterdIn(image_bounds)
    an_image.unlockFocus

    # get the location of the drag event
    p = convertPoint_fromView(event.locationInWindow, nil)
    
    # drag from the center of the image
    p.x = p.x  - s.width / 2
    p.y = p.y - s.height / 2

    # get the pasteboard
    pb = NSPasteboard.pasteboardWithName(NSDragPboard)

    # put the string on the pasteboard
    writeStringToPasteboard(pb)

    # start the drag
    objc_send :dragImage, an_image,
                     :at, p,
                 :offset, [0,0],
                  :event, event,
             :pasteboard, pb,
                 :source, self,
              :slideBack, true
  end

  private

  def log(msg)
    $stderr.puts msg
  end

end
