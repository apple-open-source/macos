#
#  AppController.rb
#  Spotlight
#
#  Created by Norberto Ortigoza on 9/11/05.
#  Copyright (c) 2005 CrossHorizons. All rights reserved.
#
require 'osx/cocoa'
include OSX

class AppController < NSObject

  attr :spotlight
  ib_outlets :fileView, :mdItemController, :metaDataView

  def init
    @spotlight = Spotlight.alloc.init
    @spotlight.predicate = "kMDItemContentType == 'public.ruby-script'"
    return self
  end

  def awakeFromNib
    @metaDataView.setBackgroundColor(NSColor.lightGrayColor)
    @spotlight.register_for_notification(self, "updateSearching:")
    NSNotificationCenter.defaultCenter.objc_send :addObserver, self,
           :selector, "updateSearching:",
           :name, :NSTableViewSelectionDidChangeNotification, 
           :object, @fileView
  end

  def refresh_metaDataView
    info = NSMutableString.stringWithCapacity(150);
    @mdItemController.selectedObjects.to_a.each do |item| 
      item.attributes.to_a.each do |attr_name|
        info.appendString(attr_name)
        info.appendString(" = ")
        info.appendString(item.valueForAttribute(attr_name).description)
        info.appendString("\n")
      end
    end
    @metaDataView.textStorage.mutableString.setString(info)
  end
    
  def dealloc
    @spotlight.removeDelegate(self)
    NSNotificationCenter.defaultCenter.removeObserver(aDelegate,
              :name, :NSTableViewSelectionDidChangeNotification,
              :object, nil);

  end

  #Notification and Event Handlers  
  def updateSearching (sender)
    refresh_metaDataView
  end
  ib_action :updateSearching
  
  def search (sender)
    @spotlight.search()
  end
  ib_action :search

end
