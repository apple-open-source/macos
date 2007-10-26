# -*- mode:ruby; indent-tabs-mode:nil; coding:utf-8 -*-
#
#  decorator.rb
#  CocoaRepl
#
#  Created by Fujimoto Hisa on 07/03/01.
#  Copyright (c) 2007 Fujimoto Hisa. All rights reserved.
#
require 'osx/cocoa'

class DecoratorStyle
  include OSX

  STYLES = {}
  def self.install(key)            STYLES[key] = self end
  def DecoratorStyle.instance(key) STYLES[key].new    end

  attr_writer :decorator

  def initialize(decorator = nil)
    @decorator = decorator
  end

  def normal_font; NSFont.userFixedPitchFontOfSize(font_size) end
  def bold_font;   NSFont.userFixedPitchFontOfSize(font_size) end
  def italic_font; NSFont.userFixedPitchFontOfSize(font_size) end

  DEFAULT_FONT_SIZE = 18.0

  def self.font_size=(val)
    @@font_size = val
  end

  def self.font_size
    if defined? @@font_size then
      @@font_size
    else
      DEFAULT_FONT_SIZE
    end
  end

  def self.style(name, opts = {})
    define_method("do_#{name}") do
      color      opts[:color]      if opts[:color]
      background opts[:background] if opts[:background]
      bold                         if opts[:bold]
      italic                       if opts[:italic]
    end
  end

  def do_misc
    color(:blackColor)
    background(:whiteColor)
    normal
  end

  def font_size
    self.class.font_size
  end

  def color(arg)
    _attr_ NSForegroundColorAttributeName, _color_(arg)
  end

  def background(arg)
    _attr_ NSBackgroundColorAttributeName, _color_(arg)
  end

  def underline
    _attr_ NSUnderlineStyleAttributeName, NSSingleUnderlineStyle
  end

  def normal
    # _attr_ NSFontAttributeName, normal_font
  end

  def bold
    # _attr_ NSFontAttributeName, bold_font
  end

  def italic
    # _attr_ NSFontAttributeName, italic_font
  end

  def _attr_(name, value)
    @decorator.text.objc_send :addAttribute, name,
                                     :value, value,
                                     :range, @decorator.range
  end

  def _color_(arg)
    re1 = /^#([0-9a-f])([0-9a-f])([0-9a-f])$/i
    re2 = /^#([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})$/i
    case arg
    when re1 then
      args = $~.to_a[1,3].map { |s| s.to_i(16) / 15.0 } << 1.0
      NSColor.colorWithCalibratedRed_green_blue_alpha(*args)
    when re2 then
      args = $~.to_a[1,3].map { |s| s.to_i(16) / 255.0 } << 1.0
      NSColor.colorWithCalibratedRed_green_blue_alpha(*args)
    when Symbol then
      NSColor.objc_send(arg)
    else
      NSColor.blackColor
    end
  end
end

class Decorator

  attr_reader :token, :range, :text

  Token = Struct.new(:kind, :value, :param)

  DECORATORS = {}
  def self.install(key)        DECORATORS[key] = self   end

  def Decorator.instance(key)
    klass = DECORATORS[key]
    klass && klass.instance
  end

  def Decorator.default
    Decorator.instance(:default)
  end

  def Decorator.default=(key)
    DECORATORS[:default] = DECORATORS[key]
  end

  def Decorator.require_decorator(key)
    require "#{key}_decorator"
    Decorator.default = key
    key
  rescue Exception => err
    warn(err.message)
    nil
  end

  def initialize(style)
    @style     ||= DecoratorStyle.instance(style)
    @style.decorator = self
  end

  def decorate(text, range = nil)            # AttributedMutableString
    @text  = text
    if range then
      @range = range
      source = @text.string.substringWithRange(range).to_s
    else
      @range = OSX::NSRange.new
      source = @text.string.to_s
    end
    source.gsub!(/\r/, "\n")
    each_token(source) do |token|
      @token = token
      @range.length   = token.value.split(//u).size
      msg = "do_#{token.kind}"
      if @style.respond_to?(msg) then
        @style.send(msg)
      else
        @style.do_misc
      end
      @range.location = @range.location + @range.length
    end
  end

  private

  def each_token(source)
    raise NotImplementedError
  end
end
