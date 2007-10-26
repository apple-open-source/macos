require 'osx/cocoa'
require 'MyNotification'

class WinCtrl < OSX::NSObject

  ImageSizeMIN = 32

  private

  @@wincount = 0		# static int wincount = 0;
  
  def windowSetUp(image)
    cas = (@@wincount & 07) * 20.0
    @@wincount += 1
    imageview = nil
    rect = OSX::NSRect.new

    rect.size = image.size
    rect.origin = OSX::NSPoint.new(100.0 + cas, 150.0 + cas)
    @window = OSX::NSWindow.alloc.
      initWithContentRect(rect,
			  :styleMask, (OSX::NSTitledWindowMask |
				       OSX::NSClosableWindowMask),
			  :backing, OSX::NSBackingStoreBuffered,
			  :defer, true)
    @window.setReleasedWhenClosed(true)
    rect.origin = OSX::NSPoint.new
    imageview = OSX::NSImageView.alloc.initWithFrame(rect)
    imageview.setImage(image)
    imageview.setEditable false
    imageview.setImageScaling true
    @window.setContentView(imageview)
  end

  def initWithPath (path)
    @filename = nil		# NSString *filename;
    @docImage = nil		# id docImage;
    @window = nil		# NSWindow *window;
    @mag = nil			# float mag;

    @docImage = OSX::NSImage.alloc.initWithContentsOfFile(path)
    return nil if @docImage == nil
    @filename = path
    self.windowSetUp @docImage
    @mag = 1.0
    OSX::NSNotificationCenter.defaultCenter.
      addObserver(self,
		  :selector, 'shrink:',
		  :name, OSX::ShrinkAllNotification,
		  :object, nil)
    @window.setTitleWithRepresentedFilename @filename
    @window.setDelegate(self)
    @window.makeKeyAndOrderFront(self)
  end

  def dealloc
    OSX::NSNotificationCenter.defaultCenter.removeObserver(self)
    @window.setDelegate(nil)
    # [filename release];
    # [docImage release];
    # [super dealloc];
  end

  def desc
    sz = @docImage.size
    "Filename: %s\nSize: %d x %d\nMagnification: %.1f%%" %
      [ File.basename(@filename.to_s), sz.width.to_i, sz.height.to_i, @mag * 100.0 ]
  end

  def shrink (notification)
    view = @window.contentView
    sz = view.frame.size
    rect = @window.frame
    xd = yd = wt = 0

    return if sz.width < ImageSizeMIN || sz.height < ImageSizeMIN

    @mag *= 0.5
    xd = sz.width.to_i / 2
    yd = sz.height.to_i / 2
    sz.width -= xd
    sz.height -= yd
    view.setFrameSize(sz)
    wt = rect.origin.y + rect.size.height
    rect.size.width -= xd
    rect.size.height -= yd
    rect.origin.y = wt - rect.size.height
    @window.setFrame(rect, :display, true)
    @window.setDocumentEdited(true)
  end

  def windowShouldClose (sender)
    if @window.isDocumentEdited? then
      OSX::NSRunAlertPanel("Warning", "File '%@' is edited.",
		      "OK", "Cancel", nil, 
		      File.basename(@filename.to_s)) == OSX::NSAlertDefaultReturn
    else
      true
    end
  end

end
