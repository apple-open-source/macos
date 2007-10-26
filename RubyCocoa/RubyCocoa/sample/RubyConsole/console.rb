# Copyright (c) 2007 The RubyCocoa Project.
# Copyright (c) 2006 Tim Burks, Neon Design Technology, Inc.
#  
# Find more information about this file online at:
#    http://www.rubycocoa.com/mastering-cocoa-with-ruby

require 'irb'
require 'osx/cocoa'

class ConsoleWindowController < OSX::NSObject
  attr_accessor :window, :textview, :console
  def initWithFrame(frame)
    init
    styleMask = OSX::NSTitledWindowMask + OSX::NSClosableWindowMask + 
      OSX::NSMiniaturizableWindowMask + OSX::NSResizableWindowMask
    @window = OSX::NSWindow.alloc.initWithContentRect_styleMask_backing_defer(
      frame, styleMask, OSX::NSBackingStoreBuffered, false)
    @textview = OSX::NSTextView.alloc.initWithFrame(frame)
    @console = RubyConsole.alloc.initWithTextView @textview
    with @window do |w|
      w.contentView = scrollableView(@textview)
      w.title = "RubyCocoa Console"
      w.delegate = self
      w.center
      w.makeKeyAndOrderFront(self)
    end
    self
  end

  def run
    @console.performSelector_withObject_afterDelay("run:", self, 0)
  end

  def windowWillClose(notification)
    OSX::NSApplication.sharedApplication.terminate(self)
  end

  def windowShouldClose(notification)
    @alert = OSX::NSAlert.alloc.init
    with @alert do |a|
      a.messageText = "Do you really want to close this console?\n"
        + "Your application will exit."
      a.alertStyle = OSX::NSCriticalAlertStyle
      a.addButtonWithTitle("OK")
      a.addButtonWithTitle("Cancel")
      a.beginSheetModalForWindow_modalDelegate_didEndSelector_contextInfo(
        window, self, "alertDidEnd:returnCode:contextInfo:", nil)
    end
    false
  end

  def alertDidEnd_returnCode_contextInfo(alert, code, contextInfo)
    window.close if (code == OSX::NSAlertFirstButtonReturn)
  end
end

def with(x)
  yield x if block_given?; x
end if not defined? with

def scrollableView(content)
  scrollview = OSX::NSScrollView.alloc.initWithFrame(content.frame)
  clipview = OSX::NSClipView.alloc.initWithFrame(scrollview.frame)
  scrollview.contentView = clipview
  scrollview.documentView = clipview.documentView = content
  content.frame = clipview.frame
  scrollview.hasVerticalScroller = scrollview.hasHorizontalScroller = 
    scrollview.autohidesScrollers = true
  resizingMask = OSX::NSViewWidthSizable + OSX::NSViewHeightSizable
  content.autoresizingMask = clipview.autoresizingMask = 
    scrollview.autoresizingMask = resizingMask
  scrollview
end

class RubyCocoaInputMethod < IRB::StdioInputMethod
  def initialize(console)
    super() # superclass method has no arguments
    @console = console
    @history_index = 1
    @continued_from_line = nil
  end

  def gets
    m = @prompt.match(/(\d+)[>*]/)
    level = m ? m[1].to_i : 0
    if level > 0
      @continued_from_line ||= @line_no
    elsif @continued_from_line
      mergeLastNLines(@line_no - @continued_from_line + 1)
      @continued_from_line = nil
    end
    @console.write @prompt+"  "*level
    string = @console.readLine
    @line_no += 1
    @history_index = @line_no + 1
    @line[@line_no] = string
    string
  end

  def mergeLastNLines(i)
    return unless i > 1
    range = -i..-1
    @line[range] = @line[range].map {|l| l.chomp}.join("\n")
    @line_no -= (i-1)
    @history_index -= (i-1)
  end

  def prevCmd
    return "" if @line_no == 0
    @history_index -= 1 unless @history_index <= 1
    @line[@history_index]
  end

  def nextCmd
    return "" if (@line_no == 0) or (@history_index >= @line_no)
    @history_index += 1
    @line[@history_index]
  end
end

# this is an output handler for IRB 
# and a delegate and controller for an NSTextView
class RubyConsole < OSX::NSObject
  attr_accessor :textview, :inputMethod

  def initWithTextView(textview)
    init
    @textview = textview
    @textview.delegate = self
    @textview.richText = false
    @textview.continuousSpellCheckingEnabled = false
    @textview.font = @font = OSX::NSFont.fontWithName_size('Monaco', 18.0)
    @inputMethod = RubyCocoaInputMethod.new(self)
    @context = Kernel::binding
    @startOfInput = 0
    self
  end

  def run(sender = nil)
    @textview.window.makeKeyAndOrderFront(self)
    IRB.startInConsole(self)
    OSX::NSApplication.sharedApplication.terminate(self)
  end

  def attString(string)
    OSX::NSAttributedString.alloc.initWithString_attributes(
      string, { OSX::NSFontAttributeName, @font })
  end

  def write(object)
    string = object.to_s
    @textview.textStorage.insertAttributedString_atIndex(
      attString(string), @startOfInput)
    @startOfInput += string.length
    @textview.scrollRangeToVisible([lengthOfTextView, 0])
    handleEvents if OSX::NSApplication.sharedApplication.isRunning
  end

  def moveAndScrollToIndex(index)
    range = OSX::NSRange.new(index, 0)
    @textview.scrollRangeToVisible(range)
    @textview.setSelectedRange(range)
  end

  def lengthOfTextView
    @textview.textStorage.mutableString.length
  end

  def currentLine
    text = @textview.textStorage.mutableString
    text.substringWithRange(
      OSX::NSRange.new(@startOfInput, text.length - @startOfInput)).to_s
  end

  def readLine
    app = OSX::NSApplication.sharedApplication
    @startOfInput = lengthOfTextView
    loop do
      event = app.nextEventMatchingMask_untilDate_inMode_dequeue(
        OSX::NSAnyEventMask, 
        OSX::NSDate.distantFuture(), 
        OSX::NSDefaultRunLoopMode, 
        true)
      if (event.oc_type == OSX::NSKeyDown) and 
         event.window and 
         (event.window.isEqual? @textview.window)
        break if event.characters.to_s == "\r"
        if (event.modifierFlags & OSX::NSControlKeyMask) != 0:
          case event.keyCode
          when 0:  moveAndScrollToIndex(@startOfInput)     # control-a
          when 14: moveAndScrollToIndex(lengthOfTextView)  # control-e
          end
        end
      end
      app.sendEvent(event)
    end
    lineToReturn = currentLine
    @startOfInput = lengthOfTextView
    write("\n")
    return lineToReturn + "\n"
  end

  def handleEvents
    app = OSX::NSApplication.sharedApplication
    event = app.nextEventMatchingMask_untilDate_inMode_dequeue(
      OSX::NSAnyEventMask,
      OSX::NSDate.dateWithTimeIntervalSinceNow(0.01),
      OSX::NSDefaultRunLoopMode,
      true)
    if event
      if (event.oc_type == OSX::NSKeyDown) and
        event.window and
        (event.window.isEqualTo @textview.window) and
        (event.charactersIgnoringModifiers.to_s == 'c') and
        (event.modifierFlags & OSX::NSControlKeyMask)
        raise IRB::Abort, "abort, then interrupt!!" # that's what IRB says...
      else
        app.sendEvent(event)
      end
    end
  end

  def replaceLineWithHistory(s)
    range = OSX::NSRange.new(@startOfInput, lengthOfTextView - @startOfInput)
    @textview.textStorage.replaceCharactersInRange_withAttributedString(
      range, attString(s.chomp))
    @textview.scrollRangeToVisible([lengthOfTextView, 0])
    true
  end

  # delegate methods
  def textView_shouldChangeTextInRange_replacementString(
        textview, range, replacement)
    return false if range.location < @startOfInput
    replacement = replacement.to_s.gsub("\r","\n")
    if replacement.length > 0 and replacement[-1].chr == "\n"
      @textview.textStorage.appendAttributedString(
        attString(replacement)
      ) if currentLine != ""
      @startOfInput = lengthOfTextView
      false # don't insert replacement text because we've already inserted it
    else
      true  # caller should insert replacement text
    end
  end

  def textView_willChangeSelectionFromCharacterRange_toCharacterRange(
       textview, oldRange, newRange)
    return oldRange if (newRange.length == 0) and
                       (newRange.location < @startOfInput)
    newRange
  end

  def textView_doCommandBySelector(textview, selector)
    case selector
    when "moveUp:"
      replaceLineWithHistory(@inputMethod.prevCmd)
    when "moveDown:"
      replaceLineWithHistory(@inputMethod.nextCmd)
    else
      false
    end
  end
end

module IRB
  def IRB.startInConsole(console)
    IRB.setup(nil)
    @CONF[:PROMPT_MODE] = :SIMPLE
    @CONF[:VERBOSE] = false
    @CONF[:ECHO] = true
    irb = Irb.new(nil, console.inputMethod)
    @CONF[:IRB_RC].call(irb.context) if @CONF[:IRB_RC]
    @CONF[:MAIN_CONTEXT] = irb.context
    trap("SIGINT") do
      irb.signal_handle
    end
    old_stdout, old_stderr = $stdout, $stderr
    $stdout = $stderr = console
    catch(:IRB_EXIT) do
      loop do
        begin
          irb.eval_input
        rescue Exception
          puts "Error: #{$!}"
        end
      end
    end
    $stdout, $stderr = old_stdout, old_stderr
  end
  class Context
    def prompting?
      true
    end
  end
end

class ApplicationDelegate < OSX::NSObject
  def applicationDidFinishLaunching(sender)
    $consoleWindowController = ConsoleWindowController.alloc.
       initWithFrame([50,50,600,300])
    $consoleWindowController.run
  end
end

# set up the application delegate
$delegate = ApplicationDelegate.alloc.init
OSX::NSApplication.sharedApplication.setDelegate($delegate)

# run the main loop
OSX.NSApplicationMain(0, nil)