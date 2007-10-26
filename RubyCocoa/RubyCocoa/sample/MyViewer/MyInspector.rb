require 'osx/cocoa'
require 'MyNotification'

class MyInspector < OSX::NSObject

  PanelWidth  = 200
  PanelHeight = 200
  BtnWidth    = 64
  BtnHeight   = 32
  Margin      = 20

  @@defaultInstance = nil

  def MyInspector.defaultInstance
    @@defaultInstance = MyInspector.alloc.init if @@defaultInstance == nil
    @@defaultInstance.activate
    @@defaultInstance
  end

  def panelSetting
    button = Array.new(2)

    @panel = OSX::NSPanel.alloc.
      initWithContentRect(OSX::NSRect.new(300, 300, PanelWidth, PanelHeight),
			  :styleMask, (OSX::NSTitledWindowMask | 
				       OSX::NSClosableWindowMask | 
				       OSX::NSResizableWindowMask),
			  :backing, OSX::NSBackingStoreBuffered,
			  :defer, true)
    @panel.setReleasedWhenClosed false
    @panel.setMinSize(OSX::NSSize.new(PanelWidth, PanelHeight))
    @panel.setTitle "Inspector"
    @panel.setDelegate(self)
    @isClosed = true

    2.times do |i|
      button[i] = OSX::NSButton.alloc.
	initWithFrame(OSX::NSRect.new(Margin+(Margin+BtnWidth)*i,
				      Margin, BtnWidth, BtnHeight))
      @panel.contentView.addSubview button[i]
      button[i].setAutoresizesSubviews true
      button[i].setAutoresizingMask(OSX::NSViewMaxXMargin | OSX::NSViewMaxYMargin)
      button[i].setTarget(self)
    end
    button[0].setTitle "Shrink"
    button[0].setAction "shrink:"
    button[1].setTitle "All"
    button[1].setAction "shrinkAll:"

    @text = OSX::NSTextField.alloc.
      initWithFrame(
		    OSX::NSRect.new(Margin, BtnHeight*2,
				    PanelWidth-Margin*2, 
				    PanelHeight-(Margin+BtnHeight*2)))
    @text.setSelectable false
    @text.setBezeled true
    @panel.contentView.addSubview @text
    @text.setAutoresizesSubviews true
    @text.setAutoresizingMask(OSX::NSViewWidthSizable | OSX::NSViewHeightSizable)
  end

  # def initialize
  def init
    panelSetting
    center = OSX::NSNotificationCenter.defaultCenter
    center.addObserver(self, :selector, "showMain:",
		       :name, OSX::NSWindowDidBecomeMainNotification,
		       :object, nil)
    center.addObserver(self, :selector, "windowClosed:",
		       :name, OSX::NSWindowWillCloseNotification,
		       :object, nil)
    self
  end

  def dealloc
    OSX::NSNotificationCenter.defaultCenter.removeObserver(self)
    # [panel release];
    # [super dealloc];
  end

  def activate
    @panel.setFloatingPanel true
    @panel.makeKeyAndOrderFront self
  end

  def showInfo (obj)
    if obj and obj.isKindOfClass? OSX::NSWindow then
      @text.setStringValue(obj.delegate.desc)
    else
      @text.setStringValue ""
    end
  end

  def showMain (aNotification)
    return if @isClosed
    showInfo aNotification.object
  end

  def windowClosed (aNotification)
    @text.setStringValue "" if OSX::NSApp.mainWindow == nil
  end

  def shrink (sender)
    obj = OSX::NSApp.mainWindow
    if obj then
      obj.delegate.shrink nil
      showInfo(obj)
    end
  end
  ib_action :shrink

  def shrinkAll (sender)
    obj = OSX::NSApp.mainWindow
    if obj then
      OSX::NSNotificationCenter.defaultCenter.
	postNotificationName(ShrinkAllNotification, :object, self)
      showInfo(obj)
    end
  end
  ib_action :shrinkAll

  def windowDidBecomeKey (aNotification)
    @isClosed = false
    showInfo(OSX::NSApp.mainWindow)
  end

  def windowShouldClose (sender)
    @isClosed = true
  end

end
