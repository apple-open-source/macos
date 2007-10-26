#
#  AppController.rb
#  SimpleApp
#
#  Created by FUJIMOTO Hisakuni on Sat Sep 07 2002.
#  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
#

require 'osx/cocoa'

class AppController < OSX::NSObject
  include OSX

  ib_outlets :myView, :imageView, :colorWell, :slider, 
    :window, :threadBtn

  def awakeFromNib
    @window.setOpaque(false)
    @myView.set_alpha(@slider.floatValue)
    @myView.set_color(@colorWell.color)
  end

  def aboutApp (sender)
    NSApp.orderFrontStandardAboutPanelWithOptions("Copyright" => "RubyCocoa #{RUBYCOCOA_VERSION}",
						    "ApplicationVersion" => "Ruby #{VERSION}")
  end
  ib_action :aboutApp

  def colorBtnClicked (sender)
    color = case sender.tag
	    when 0 then NSColor.redColor
	    when 1 then NSColor.greenColor
	    when 2 then NSColor.blueColor
	    end
    if color then
      @myView.set_color(color)
      @colorWell.setColor(color)
    end
  end
  ib_action :colorBtnClicked

  def colorWellChanged (sender)
    @myView.set_color(sender.color)
  end
  ib_action :colorWellChanged

  def sliderChanged (sender)
    @myView.set_alpha(sender.floatValue)
  end
  ib_action :sliderChanged

  def threadStart (sender)
    alpha_saved = @myView.alpha
    Thread.start do
      @slider.setEnabled(false)
      @threadBtn.setEnabled(false)
      100.times do |i|
	alpha = alpha_saved + (i.to_f / 100.0)
	alpha = alpha - 1.0 if alpha > 1.0
	@myView.set_alpha(alpha)
	@slider.setFloatValue(alpha)
	sleep 0.02
      end
      @slider.setEnabled(true)
      @threadBtn.setEnabled(true)
    end
    @myView.set_alpha(alpha_saved)
    @slider.setFloatValue(alpha_saved)
  end
  ib_action :threadStart

end
