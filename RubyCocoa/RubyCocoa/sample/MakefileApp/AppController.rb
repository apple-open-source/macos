require 'osx/cocoa'

class AppController < OSX::NSObject

  ib_outlets :msgField

  def awakeFromNib
    update_time
  end

  ib_action :btnClicked do |sender|
    update_time
  end

  ib_action :windowShouldClose do |sender|
    quit
    true
  end

  def update_time
    @msgField.setStringValue(Time.now.to_s)
  end

  def quit
    OSX::NSApp.stop(self)
  end

end
