# -*- mode:ruby; indent-tabs-mode:nil; coding:utf-8 -*-
#
#  io.rb
#  CocoaRepl
#
#  Created by Fujimoto Hisa on 07/03/01.
#  Copyright (c) 2007 Fujimoto Hisa. All rights reserved.
#
require 'observable'

class << STDOUT
  include ReplObservable

  alias_method :original_write, :write

  def write(str)
    notify_to_observers(:write, str)
    original_write(str)
  end
end

class << STDERR
  include ReplObservable

  alias_method :original_write, :write

  def write(str)
    notify_to_observers(:write, str)
    original_write(str)
  end
end
