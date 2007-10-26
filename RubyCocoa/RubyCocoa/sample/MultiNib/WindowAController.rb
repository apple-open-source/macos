require 'osx/cocoa'

class WindowAController < OSX::NSWindowController

  def init
    initWithWindowNibName "WindowA"
  end

end
