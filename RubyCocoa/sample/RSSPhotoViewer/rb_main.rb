#
#  rb_main.rb
#  RSSPhotoViewer
#
#  Created by Laurent Sansonetti on 2/6/07.
#  Copyright (c) 2007 Apple Computer. All rights reserved.
#

ENV['BRIDGE_SUPPORT_PATH'] = '/Volumes/Data/src/svn-rubycocoa-apple/framework/bridge-support'
require 'osx/cocoa'

include OSX
require_framework 'ImageKit'

def rb_main_init
  path = OSX::NSBundle.mainBundle.resourcePath.fileSystemRepresentation
  rbfiles = Dir.entries(path).select {|x| /\.rb\z/ =~ x}
  rbfiles -= [ File.basename(__FILE__) ]
  rbfiles.each do |path|
    require( File.basename(path) )
  end
end

if $0 == __FILE__ then
  rb_main_init
  OSX.NSApplicationMain(0, nil)
end
