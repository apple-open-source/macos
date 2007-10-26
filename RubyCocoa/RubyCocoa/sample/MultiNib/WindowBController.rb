require 'osx/cocoa'

class WindowBController < OSX::NSObject

  ns_outlets   :window

  def init
    OSX::NSBundle.loadNibNamed("WindowB", :owner, self)
    self
  end

  def showWindow
    @window.makeKeyAndOrderFront(self)
  end

end
