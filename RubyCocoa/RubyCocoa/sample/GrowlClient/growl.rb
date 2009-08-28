#
# = growl.rb
#
# Growl Notifier
#
# Copyright (c) 2007 Satoshi Nakagawa <artension@gmail.com>
# You can redistribute it and/or modify it under the same terms as Ruby.
#
# == Overview
#
# Growl Notifier is a class to post notifications to the Growl daemon.
# And also it can receive clicked and timeout notifications from Growl.
#
# == Requirements
#
# Mac OS X 10.4 or 10.5
# Ruby 1.8 (http://ruby-lang.org/)
# RubyCocoa (http://rubycocoa.sourceforge.net/)
#
# == How to use Growl Notifier
#
# A simple example:
#
#   require 'growl'
#
#   g = Growl::Notifier.alloc.init
#   g.start('test_app', ['message_type'])
#   g.notify('message_type', 'title', 'desc')
#
# How to receive clicked and timeout notifications in your application:
#
#   require 'growl'
#
#   class GrowlController < OSX::NSObject
#     def init
#       @g = Growl::Notifier.alloc.initWithDelegate(self)
#       @g.register('test_app', ['message_type'])
#       @g.notify('message_type', 'title', 'desc', 'click_context')
#       self
#     end
#  
#     def growl_onClicked(context)
#       puts 'clicked'
#     end
#  
#     def growl_onTimeout(context)
#       puts 'timeout'
#     end
#   end
#

require 'osx/cocoa'

module Growl
  class Notifier < OSX::NSObject
    include OSX
    attr_accessor :delegate
    
    GROWL_IS_READY = "Lend Me Some Sugar; I Am Your Neighbor!"
    GROWL_NOTIFICATION_CLICKED = "GrowlClicked!"
    GROWL_NOTIFICATION_TIMED_OUT = "GrowlTimedOut!"
    GROWL_KEY_CLICKED_CONTEXT = "ClickedContext"
    
    
    def initWithDelegate(delegate)
      init
      @delegate = delegate
      self
    end
    
    def start(appname, notifications, default_notifications=nil, appicon=nil)
      @appname = appname
      @notifications = notifications
      @default_notifications = default_notifications
      @appicon = appicon
      @default_notifications = @notifications unless @default_notifications
      register
    end
    
    def notify(type, title, desc, click_context=nil, sticky=false, priority=0, icon=nil)
      dic = {
        :ApplicationName => @appname,
        :ApplicationPID => NSProcessInfo.processInfo.processIdentifier,
        :NotificationName => type,
        :NotificationTitle => title,
        :NotificationDescription => desc,
        :NotificationPriority => priority,
      }
      dic[:NotificationIcon] = icon.TIFFRepresentation if icon
      dic[:NotificationSticky] = 1 if sticky
      dic[:NotificationClickContext] = click_context if click_context
      
      c = NSDistributedNotificationCenter.defaultCenter
      c.postNotificationName_object_userInfo_deliverImmediately(:GrowlNotification, nil, dic, true)
    end
    
    KEY_TABLE = {
      :type => :NotificationName,
      :title => :NotificationTitle,
      :desc => :NotificationDescription,
      :clickContext => :NotificationClickContext,
      :sticky => :NotificationSticky,
      :priority => :NotificationPriority,
      :icon => :NotificationIcon,
    }
    
    def notifyWith(hash)
      dic = {}
      KEY_TABLE.each {|k,v| dic[v] = hash[k] if hash.key?(k) }
      dic[:ApplicationName] = @appname
      dic[:ApplicationPID] = NSProcessInfo.processInfo.processIdentifier
      
      c = NSDistributedNotificationCenter.defaultCenter
      c.postNotificationName_object_userInfo_deliverImmediately(:GrowlNotification, nil, dic, true)
    end
    
    
    def onReady(n)
      register
    end
    
    def onClicked(n)
      context = n.userInfo[GROWL_KEY_CLICKED_CONTEXT].to_s
      @delegate.growl_onClicked(self, context) if @delegate && @delegate.respond_to?(:growl_onClicked)
    end
    
    def onTimeout(n)
      context = n.userInfo[GROWL_KEY_CLICKED_CONTEXT].to_s
      @delegate.growl_onTimeout(self, context) if @delegate && @delegate.respond_to?(:growl_onTimeout)
    end
    
    
    private
    
    def register
      pid = NSProcessInfo.processInfo.processIdentifier.to_i
      
      c = NSDistributedNotificationCenter.defaultCenter
      c.addObserver_selector_name_object(self, 'onReady:', GROWL_IS_READY, nil)
      c.addObserver_selector_name_object(self, 'onClicked:', "#{@appname}-#{pid}-#{GROWL_NOTIFICATION_CLICKED}", nil)
      c.addObserver_selector_name_object(self, 'onTimeout:', "#{@appname}-#{pid}-#{GROWL_NOTIFICATION_TIMED_OUT}", nil)
      
      icon = @appicon || NSApplication.sharedApplication.applicationIconImage
      dic = {
        :ApplicationName => @appname,
        :AllNotifications => @notifications,
        :DefaultNotifications => @default_notifications,
        :ApplicationIcon => icon.TIFFRepresentation,
      }
      c.postNotificationName_object_userInfo_deliverImmediately(:GrowlApplicationRegistrationNotification, nil, dic, true)
    end
  end
end
