# -*- mode:ruby; indent-tabs-mode:nil; coding:utf-8 -*-
#
#  RcodeController.rb
#  RubyAnywhere
#
#  Created by Fujimoto Hisa on 07/01/23.
#  Copyright (c) 2007 FUJIMOTO Hisa FOBJ SYSTEMS. All rights reserved.

require 'osx/cocoa'
require 'stringio'

class RcodeController < OSX::NSObject
  include OSX

  ib_outlets :scratchText, :resultText, :outText
  ib_outlets :menu, :scratchPanel
  ib_outlets :rubyVersion, :rubyCocoaVersion, :aboutPanel

  FONTSIZE = 16
  STD_COLOR = NSColor.blackColor
  ERR_COLOR = NSColor.redColor
  OUT_COLOR = NSColor.blueColor

  def self.instance
    @@instance ||= self.alloc.init
  end

  def init
    super_init
    @host_app = NSApplication.sharedApplication
    @bundle = NSBundle.bundleForClass(self.oc_class)
    return self
  end

  def awakeFromNib
    install_menu(image_named("ruby_logo_small"))
    font = NSFont.userFixedPitchFontOfSize(FONTSIZE)
    @scratchText.setFont(font)
    @resultText.setFont(font)
    @outText.setFont(font)
    @rubyVersion.setStringValue(RUBY_VERSION)
    @rubyCocoaVersion.setStringValue("#{RUBYCOCOA_VERSION} (r#{RUBYCOCOA_SVN_REVISION})")
  end

  ib_action(:evaluate) do
    src = @scratchText.string.to_s.gsub(/\r/, "\n")
    result = with_io_redirect { eval(src, TOPLEVEL_BINDING) }
    @resultText.setString(result.inspect)
    @resultText.setTextColor(STD_COLOR)
  end

  ib_action(:showScratchPanel) do
    @scratchPanel.makeKeyAndOrderFront(self)
  end

  ib_action(:showAbout) do
    @aboutPanel.makeKeyAndOrderFront(self)
  end

  private

  def with_io_redirect
    saved_out = $stdout
    saved_err = $stderr
    out_io = StringIO.new
    $stdout = $stderr = out_io
    ret = yield
    @outText.setString(out_io.string)
    @outText.setTextColor(OUT_COLOR)
    ret
  rescue Exception => err
    str = err.message << "\n" << err.backtrace.join("\n")
    @outText.setString(str)
    @outText.setTextColor(ERR_COLOR)
    err
  ensure
    $stdout = saved_out
    $stderr = saved_err
  end

  def install_menu(menu_image = nil)
    item = @host_app.mainMenu.
      objc_send(:addItemWithTitle, "Rcode",
                :action, nil, :keyEquivalent, "")
    @menu.setTitle("Rcode")
    item.setSubmenu(@menu)
    item.setImage(menu_image) if menu_image
  end

  def image_named(name)
    path = @bundle.pathForImageResource(name)
    return OSX::NSImage.alloc.initWithContentsOfFile(path)
  end
end
