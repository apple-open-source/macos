# -*- mode:ruby; indent-tabs-mode:nil; coding:utf-8 -*-
#
#  syntax_decorator.rb
#  CocoaRepl
#
#  Created by Fujimoto Hisa on 07/03/01.
#  Copyright (c) 2007 Fujimoto Hisa. All rights reserved.
#
#  syntax --
#
require 'decorator'
require 'rubygems'
require 'syntax'

class SyntaxStyle < DecoratorStyle
  install :syntax

  style :comment,   :color => :darkGrayColor, :italic => true
  style :keyword,   :color => :redColor,      :bold   => true
  style :method,    :color => '#077'
  style :class,     :color => '#074'
  style :module,    :color => '#050'
  style :punct,     :color => '#447'
  style :symbol,    :color => '#099'
  style :string,    :color => '#944', :background => '#FFE'
  style :char,      :color => '#F07'
  style :ident,     :color => '#004'
  style :constant,  :color => '#07F'
  style :regex,     :color => '#B66', :background => '#FEF'
  style :number,    :color => '#F99'
  style :attribute, :color => '#7BB'
  style :global,    :color => '#7FB'
  style :expr,      :color => '#227'
  style :escape,    :color => '#277'
end

class SyntaxDecorator < Decorator
  install :syntax

  def SyntaxDecorator.instance
    @instance = new if not defined? @instance
    @instance
  end

  def initialize(style = :syntax, lang = "ruby")
    super(style)
    @tokenizer = Syntax.load(lang)
  end

  private

  def each_token(source)
    regions = []
    @tokenizer.tokenize(source) do |stk|
      case stk.instruction
      when :region_open then
        regions.push( [stk] )
      when :region_close then
        stks = regions.pop
        stk = stks[1..-1].inject(stks.first) {|p,t| p << t }
        token = Decorator::Token.new(stk.group, stk)
        yield(token)
      else
        if regions.empty? then
          token = Decorator::Token.new(stk.group, stk)
          yield(token)
        else
          regions.last.push(stk)
        end
      end
    end
  end
end
