require 'osx/cocoa'

class AppController < OSX::NSObject

  ib_outlets :msgField, :myView

  def awakeFromNib
    update_time
  end

  ib_action :btnClicked do |sender|
    btn_name = sender.title.to_s.downcase
    if /^time/ =~ btn_name then
      update_time
    else
      @myView.set_color(btn_name)
    end
  end

  ib_action :windowShouldClose do |sender|
    quit
    true
  end

  private

  def update_time
    @msgField.setStringValue(Time.now.to_s)
  end

  def quit
    OSX::NSApp.stop(self)
  end

end
