#
#  Sticky.rb
#  Stickies
#
#  Created by Laurent Sansonetti on 1/4/07.
#  Copyright (c) 2007 Apple Computer. All rights reserved.
#

class Sticky < NSManagedObject
  
  ib_outlets :stickyWindow, :contents
  
  def stickyNib
    @stickyNib ||= NSNib.alloc.initWithNibNamed_bundle('Sticky', nil)
  end
  
  WINDOW_FRAME_KEY = 'windowFrameAsString'
  
  # Set up the sticky's window from the sticky nib file
  def setupSticky
    stickyNib.instantiateNibWithOwner_topLevelObjects(self, nil)
    raise "IBOutlets were not set correctly in Sticky.nib" if @stickyWindow.nil? or @contents.nil?
    
    @stickyWindow.setDelegate(self)
    @stickyWindow.
      objc_send :setFrame, NSRectFromString(valueForKey(WINDOW_FRAME_KEY)),
                :display,  true
    @stickyWindow.makeKeyAndOrderFront(self)
    
    # Register for KVO on the sticky's frame rect so that the window will redraw if we change the frame
    objc_send :addObserver, self,
              :forKeyPath, WINDOW_FRAME_KEY,
              :options, NSKeyValueObservingOptionNew,
              :context, nil
  end

  def observeValueForKeyPath_ofObject_change_context(keyPath, object, change, context)
    # If the value of a sticky's frame changes in the managed object, we update the window to match
    return unless keyPath.to_s == WINDOW_FRAME_KEY
    val = change.objectForKey(NSKeyValueChangeNewKey)
    return unless val.is_a?(NSString)
    newFrame = NSRectFromString(val)
    # Don't setFrame if the frame hasn't changed; this prevents infinite recursion
    if @stickyWindow.frame != newFrame
      @stickyWindow.setFrame_display(newFrame, true)
    end
  end

  def awakeFromInsert
    super_awakeFromInsert
    stickyNib.instantiateNibWithOwner_topLevelObjects(self, nil)
    rememberWindowFrame # Need to get an initial value for the window size and ocation into the database.
    @stickyWindow.makeKeyAndOrderFront(self)
  end

  # Destroy the sticky
  def windowShouldClose(sender)
    NSApp.delegate.removeSticky(self)
    true
  end

  def rememberWindowFrame
    setValue_forKey(NSStringFromRect(@stickyWindow.frame), WINDOW_FRAME_KEY)
  end

  def windowDidMove(notification)
    rememberWindowFrame
  end
  
  def windowDidResize(notification)
    rememberWindowFrame
  end

=begin
- (void) dealloc
{
	if(stickyWindow) {
		[stickyWindow orderOut:self];
		[stickyWindow release];
    }
	[super dealloc];
}
=end
  
end
