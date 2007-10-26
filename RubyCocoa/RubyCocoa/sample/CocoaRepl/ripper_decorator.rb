# -*- mode:ruby; indent-tabs-mode:nil; coding:utf-8 -*-
#
#  ripper_decorator.rb
#  CocoaRepl
#
#  Created by Fujimoto Hisa on 07/03/01.
#  Copyright (c) 2007 Fujimoto Hisa. All rights reserved.
#
require 'decorator'
require 'langscan/ruby/compat/ripper'

class RipperStyle < DecoratorStyle
  install :ripper

  style :comment,   :color => :darkGrayColor, :italic => true
  style :keyword,   :color => :redColor,      :bold   => true
  style :kw,        :color => :redColor,      :bold   => true # Ripper
  style :method,    :color => '#077'
  style :const,     :color => '#074' # Ripper
  style :class,     :color => '#074'
  style :module,    :color => '#050'
  style :punct,     :color => '#447'
  style :symbol,    :color => '#099'
  style :string,    :color => '#944', :background => '#FFE'
  style :char,      :color => '#F07'
  style :ident,     :color => '#004'
  style :constant,  :color => '#07F'
  style :regexp,    :color => '#B66', :background => '#FEF' # Ripper
  style :regex,     :color => '#B66', :background => '#FEF'
  style :number,    :color => '#F99'
  style :int,       :color => '#F99' # Ripper
  style :float,     :color => '#F99' # Ripper
  style :attribute, :color => '#7BB'
  style :global,    :color => '#7FB'
  style :expr,      :color => '#227'
  style :escape,    :color => '#277'
end

class RipperDecorator < Decorator
  install :ripper

  def RipperDecorator.instance
    @instance = new if not defined? @instance
    @instance
  end

  def initialize(style = :ripper)
    super(style)
  end

  private

  KWDMAP = {
    :symbeg      => :symbol,
    :regexp_beg  => :regexp,
    :tstring_beg => :string,
    :heredoc_beg => :string,
    :qwords_beg  => :string,
    :words_beg   => :string,
  }

  def _key_(k) KWDMAP[k] || k end

  def each_token(source)
    regions = []
    Ripper.lex(source).each do |ary, kwd, val|
      token = nil
      kwd = kwd.to_s.sub(/^on_/,'').to_sym

      case kwd
      when :symbeg then
        regions.push( [Decorator::Token.new(_key_(kwd), val)] )

      when :tstring_beg, :heredoc_beg, :qwords_beg, :words_beg, :regexp_beg then
        regions.push( [Decorator::Token.new(_key_(kwd), val)] )

      when :tstring_end, :heredoc_end, :regexp_end then
        tokens = regions.pop
        token = tokens.shift
        token.value = tokens.inject(token.value) { |p,tkn| p << tkn.value }
        token.value << val

      when :ident then
        if regions.empty? then
          token = Decorator::Token.new(_key_(kwd), val)
        else
          last = regions.last
          if last.size == 1 and last.first.kind == :symbol then
            token = regions.pop.shift
            token.value << val
          else
            regions.last.push( Decorator::Token.new(_key_(kwd), val) )
          end
        end

      else
        if regions.empty? then
          token = Decorator::Token.new(_key_(kwd), val)
        else
          regions.last.push( Decorator::Token.new(_key_(kwd), val) )
        end
      end
      yield(token) if token
    end
  end
end
