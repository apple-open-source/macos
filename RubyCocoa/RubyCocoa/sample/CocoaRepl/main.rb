# -*- mode:ruby; indent-tabs-mode:nil; coding:utf-8 -*-
#
#  main.rb
#  CocoaRepl
#
#  Created by Fujimoto Hisa on 07/03/01.
#  Copyright (c) 2007 FUJIMOTO Hisa. All rights reserved.
#
$KCODE = 'utf-8'
require 'osx/cocoa'
require 'observable'
require 'evaluator'
require 'io'

DEFAULT_HISTORY_SIZE = 500
INSPECT_MAX = 80

class Object
  def self.def_truncated_inspect(prefix_len, suffix)
    if defined? inspect and not defined? cocoarepl_original_inspect then
      alias_method  :cocoarepl_original_inspect, :inspect
      define_method :inspect do
        ret = cocoarepl_original_inspect
        if ret.size > INSPECT_MAX
        then  "#{ret[0, INSPECT_MAX - prefix_len - suffix.size]}#{suffix}"
        else  ret  end
      end
    end
  end
end

class Object; def_truncated_inspect 2, "...>" end
class Array;  def_truncated_inspect 1, "...]" end
class Hash;   def_truncated_inspect 1, "...}" end
class String; def_truncated_inspect 1, '..."' end

class OSX::NSPoint
  unless defined? inspect_at_init_cocoarepl then
    alias_method :inspect_at_init_cocoarepl, :inspect
    def inspect
      inspect_at_init_cocoarepl[0..-2] << format(" x=%d y=%d>", x, y)
    end
  end
end

class OSX::NSSize
  unless defined? inspect_at_init_cocoarepl then
    alias_method :inspect_at_init_cocoarepl, :inspect
    def inspect
      inspect_at_init_cocoarepl[0..-2] <<
        format(" width=%d height=%d>", width, height)
    end
  end
end

class OSX::NSRect
  unless defined? inspect_at_init_cocoarepl then
    alias_method :inspect_at_init_cocoarepl, :inspect
    def inspect
      inspect_at_init_cocoarepl[0..-2] <<
        format(" x=%d y=%d width=%d height=%d>",
               origin.x, origin.y, size.width, size.height)
    end
  end
end

def load_rc
  path = File.join(ENV['HOME'], ".cocoareplrc")
  load(path) if File.exist? path
end

def load_decorator
  require 'decorator'
  (ARGV.size > 0 && Decorator.require_decorator(ARGV.shift.to_sym)) ||
    %w( syntax ripper simple ).find { |i| Decorator.require_decorator(i.to_sym) }
end

ARGV.delete_if {|i| /-psn/ =~ i }

OSX.init_for_bundle do |bdl,prm,lgr|
  load_rc
  load_decorator
  Evaluator.create(DEFAULT_HISTORY_SIZE)
  require 'cocoa_sweets'
  require 'RubyProgramTextView'
  require 'ReplController'
end
