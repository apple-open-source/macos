# RubyController

require 'osx/cocoa'

class RubyController < OSX::NSObject

  ib_outlets :textField

  ib_action :btnClicked do |sender|
    @textField.setStringValue "#{sender.title} !!"
  end
end
