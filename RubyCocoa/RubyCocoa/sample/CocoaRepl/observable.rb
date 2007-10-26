# -*- mode:ruby; indent-tabs-mode:nil; coding:utf-8 -*-
#
#  observable.rb
#  CocoaRepl
#
#  Created by Fujimoto Hisa on 07/03/01.
#  Copyright (c) 2007 Fujimoto Hisa. All rights reserved.
#

module ReplObservable

  def notify_to_observers(key, *args)
    observers.each do |obj, msg|
      obj.send(msg, self, key, *args)
    end
  end

  def add_observer(obj, msg)
    observers[obj] = msg
  end

  def delete_observer(obj)
    observers.delete(obj)
  end

  def observers
    @observers = {} unless defined? @observers
    @observers
  end
end
