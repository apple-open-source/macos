require 'osx/cocoa'
require 'kconv'

WIN_TITLE = "The Daughter In A Box"

class BackView < OSX::NSView
  def drawRect (rect)
    # Set the window background to transparent
    OSX::NSColor.clearColor.colorWithAlphaComponent(0.4).set
    OSX::NSRectFill(bounds)
  end
end

class CocoaHako < OSX::NSObject

  def init (parent = nil, unit_size = 64)
    @w = 4
    @h = 5
    @unit_size = unit_size
    app_init
    @house = house_new(@unit_size * @w, @unit_size * @h)
    @chips = {}
    return self
  end

  def add_chip(chip)
    widget = ChipLabel.alloc.initWithChip(chip)
    @house.addSubview(widget)
    @chips[chip] = widget
    widget.bind_b1_motion {|x, y| do_motion(x, y, chip)}
  end

  def do_motion(x, y, chip)
    if x >= @unit_size * chip.w
      dx = 1
    elsif x < 0
      dx = -1
    else
      dx = 0
    end

    if y >= @unit_size * chip.h
      dy = 1
    elsif y < 0
      dy = -1
    else
      dy = 0
    end

    if (dx != 0)
      dy = 0
    end

    return if (dx == 0) and (dy == 0)
    chip.try_move(dx, dy)
  end

  def moveto_chip(x, y, chip)
    widget = @chips[chip]
    widget.setFrame [@unit_size * x, @unit_size * (@h-y-chip.h),
      @unit_size * chip.w, @unit_size * chip.h]
    widget.superview.setNeedsDisplay(true)
    widget.setNeedsDisplay(true)
  end

  # Cocoa GUI

  def app_init
    OSX.ruby_thread_switcher_start(0.001, 0.01)
    app = OSX::NSApplication.sharedApplication
    app.setMainMenu(OSX::NSMenu.alloc.init)
  end

  def house_new (w, h)
    @window = OSX::NSWindow.alloc.
      initWithContentRect( [0, 0, w, h],
	:styleMask, OSX::NSTitledWindowMask + OSX::NSClosableWindowMask,
	:backing, OSX::NSBackingStoreBuffered,
	:defer, true)
    @window.setReleasedWhenClosed(false)
    @window.setTitleWithRepresentedFilename(WIN_TITLE)
    house = view_new
    @window.setContentView(house)
    @window.makeKeyAndOrderFront(self)
    @window.orderFrontRegardless
    @window.setDelegate(self)	# for handling windowShouldClose
    @window.setOpaque(false)
    @window.setHasShadow(true)
    return house
  end

  def view_new
    BackView.alloc.init
  end
  
  def windowShouldClose(sender)
    OSX::NSApp.stop(nil)
    false
  end

end

class ChipLabel < OSX::NSTextField

  def initWithChip (chip)
    initWithFrame [0, 0, chip.w, chip.h]
    name = chip.name.to_s
    setStringValue(name)
    setEditable(false)
    setBackgroundColor(ChipLabel.color_for(name))
    self
  end

  def mouseDragged (evt)
    point = convertPoint(evt.locationInWindow, :fromView, nil)
    @b1_motion.call(point.x, point.y)
  end

  def bind_b1_motion (&proc)
    @b1_motion = proc
  end

  def ChipLabel.color_for (str)
    if @hue.nil? then
      @hue = 0.0
      @col_dic = Hash.new
    end
    unless color = @col_dic[str] then
      color = OSX::NSColor.
	colorWithCalibratedHue(@hue, :saturation, 0.5, :brightness, 1.0, :alpha, 1.0)
      @hue += 0.1
      @col_dic[str] = color
    end
    return color
  end

end
