#!/usr/bin/env ruby
#
# $Id: rb_main.rb 1359 2007-01-06 17:34:30Z lrz $
#

$KCODE = 'e'
require 'hako'
require 'cocoa_hako'

lang = 
  if /ja/ =~ ENV['LANG'] then 'ja' else nil end

Hako::Game.new(CocoaHako.alloc.init(nil, 64), lang)
OSX::NSApp.run
