# -*- mode:ruby; indent-tabs-mode:nil; coding:utf-8 -*-
#
#  Evaluator.rb
#  CocoaRepl
#
#  Created by Fujimoto Hisa on 07/03/01.
#  Copyright (c) 2007 Fujimoto Hisa. All rights reserved.
#
require 'observable'

class Evaluator
  include ReplObservable
  attr_reader :history

  @@instance = nil

  def self.create(max=nil) self.new(max) end
  def self.instance; @@instance end

  def self.evaluate(source, fname=nil, lineno=nil)
    instance.evaluate!(source, fname, lineno)
  end

  def initialize(max=nil)
    raise "Can't create multiple instance" if @@instance
    @max = max
    @history = []
    @@instance = self
  end

  def max=(newval)
    @max = newval
    adjust_size!
  end

  def evaluate!(source, fname=nil, lineno=nil)
    result = Result.evaluate(source, fname, lineno)
    adjust_size!
    @history << result
    notify_to_observers(:evaluate!, result)
    result
  end

  private

  def adjust_size!
    if @max and @history.size >= @max
      @history.slice!(0, @history.size - @max)
    end
  end
end

class Evaluator
  class Result
    attr_reader :source, :retval, :error, :fname, :lineno
    attr_reader :start_time, :stop_time

    def self.evaluate(source, fname=nil, lineno=nil)
      self.new(source, fname, lineno).evaluate!
    end

    def initialize(source, fname=nil, lineno=nil)
      @source = source
      @fname  = fname  || "(program)"
      @lineno = lineno || 1
      @binding = nil
      @retval = @error = nil
      @start_time = @stop_time = nil
      @done = false
    end

    def done?; @done end

    def seconds
      @start_time && @stop_time &&
        (@stop_time - @start_time)
    end

    def evaluate!(binding = nil)
      return self if @done
      begin
        @binding = binding || TOPLEVEL_BINDING
        @start_time = Time.now
        @retval = eval(@source, @binding, @fname, @lineno)
        self
      rescue => err
        $stderr.puts "#{err.class}: #{err.message}"
        @error = err
        self
      rescue Exception => err
        $stderr.puts "#{err.class}: #{err.message}"
        err.backtrace.each { |i| $stderr.puts "  #{i}" }
        @error = err
        self
      ensure
        @stop_time = Time.now
        @done = true
        self
      end
    end
  end
end
