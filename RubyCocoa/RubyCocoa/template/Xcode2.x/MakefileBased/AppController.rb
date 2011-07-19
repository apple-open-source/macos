require 'osx/cocoa'

class AppController < OSX::NSObject

  ib_outlets :msgField

  def awakeFromNib
    update_time
  end

  def btnClicked(sender)
    update_time
  end

  def windowShouldClose(sender)
    quit
    true
  end

  def update_time
    @msgField.setStringValue(Time.now.to_s)
  end

  def quit
    OSX.NSApp.stop(self)
  end

end
