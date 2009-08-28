require 'osx/cocoa'
require 'growl'
include OSX

class GrowlController
  MESSAGE_KIND = 'message'
  CLICKED_KIND = 'clicked'
  
  def initialize
    # Register this application to Growl.
    @growl = Growl::Notifier.alloc.initWithDelegate(self)
    @growl.start(:GrowlClientDemo, [MESSAGE_KIND, CLICKED_KIND])
  end
  
  def show_message
    # Send a notification to Growl.
    @growl.notify(MESSAGE_KIND, 'RubyCocoa Growl Client', 'click this!', 'you can write some tags here', true)
  end

  def growl_onClicked(sender, context)
    # Send a notification with a hash form.
    @growl.notifyWith(
            :type => CLICKED_KIND,
            :title => 'Clicked',
            :desc => context,
            :sticky => false,
            :priority => 0
          )
    exit!
  end
end

if $0 == __FILE__
  controller = GrowlController.new
  controller.show_message
  NSApplication.sharedApplication
  NSApp.run
end
