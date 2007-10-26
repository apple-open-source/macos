# -*- mode:ruby; indent-tabs-mode:nil; coding:utf-8 -*-
#
#  simple_decorator.rb
#  CocoaRepl
#
#  Created by Fujimoto Hisa on 07/03/01.
#  Copyright (c) 2007 Fujimoto Hisa. All rights reserved.
#
require 'decorator'
require 'strscan'

class SimpleStyle < DecoratorStyle
  install :simple
  style :keyword, :color => :blueColor, :bold   => true
end

class SimpleDecorator < Decorator
  install :simple

  def SimpleDecorator.instance
    @instance = new if not defined? @instance
    @instance
  end

  def initialize(style = :simple)
    super(style)
  end

  private

  BLOCK_BEGIN = %w( begin class module do def for )

  PATTERN_BLOCK_BEGIN = /\b(#{BLOCK_BEGIN.join('|')})\b/u #
  PATTERN_BLOCK_END   = /\bend\b/u
  PATTERN_SPACE       = /\s+/u

  def each_token(source)
    scanner = StringScanner.new(source)

    while not scanner.eos? do

      if scanner.match?(PATTERN_SPACE) then
        val = scanner.scan(PATTERN_SPACE)
        token = Decorator::Token.new(:misc, val)

      elsif scanner.match?(PATTERN_BLOCK_BEGIN) then
        val = scanner.scan(PATTERN_BLOCK_BEGIN)
        token = Decorator::Token.new(:keyword, val)

      elsif scanner.match?(PATTERN_BLOCK_END) then
        val = scanner.scan(PATTERN_BLOCK_END)
        token = Decorator::Token.new(:keyword, val)

      else
        val = scanner.getch
        token = Decorator::Token.new(:misc, val)
      end
      yield(token)
    end
  end
end
