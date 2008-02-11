#
#  RSSWindowController.rb
#  RSSPhotoViewer
#
#  Created by Laurent Sansonetti on 7/24/06.
#  Copyright (c) 2006 Apple Computer. All rights reserved.
#

require 'open-uri'
require 'rss'

class RSSWindowController < NSWindowController
  ib_outlet :imageBrowserView
  
  def awakeFromNib
    @cache = []
    @imageBrowserView.setAnimates(true)
    @imageBrowserView.setDataSource(self)
    @imageBrowserView.setDelegate(self)
  end
  
  # Actions
  
  def zoomChanged(sender)
    @imageBrowserView.setZoomValue(sender.floatValue)
  end
  
  def parse(sender)
    begin
      urlString = sender.stringValue.to_s.strip
      return if urlString.empty?
      uri = URI.parse(urlString)
      raise "Invalid URL" unless uri.respond_to?(:read)
      @parser = RSS::Parser.parse(uri.read, false)
      @cache.clear
      @imageBrowserView.reloadData
    rescue => e
      NSRunAlertPanel("Can't parse URL", e.message, 'OK', nil, nil)
    end
  end
  
  # Image browser datasource/delegate

  def numberOfItemsInImageBrowser(browser)
    @parser ? @parser.items.length : 0
  end
  
  def imageBrowser_itemAtIndex(browser, index)
    photo = @cache[index]
    if photo.nil? 
      item = @parser.items[index]
      url = item.description.scan(/img src="([^"]+)/).first.first
      photo = RSSPhoto.new(url)
      @cache[index] = photo
    end
    return photo
  end

  def imageBrowser_cellWasDoubleClickedAtIndex(browser, index)
    NSWorkspace.sharedWorkspace.openURL(@cache[index].url)
  end
end

class RSSPhoto
  attr_reader :url
  
  def initialize(url)
    @urlString = url
    @url = NSURL.alloc.initWithString(url)
  end
  
  # IKImageBrowserItem protocol conformance
  
  def imageUID
    @urlString
  end
    
  def imageRepresentationType
    :IKImageBrowserNSImageRepresentationType
  end
  
  def imageRepresentation    
    @image ||= NSImage.alloc.initByReferencingURL(@url)
  end
end
