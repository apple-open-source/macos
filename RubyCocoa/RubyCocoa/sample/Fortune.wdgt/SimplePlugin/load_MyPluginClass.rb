# -*- mode:ruby; indent-tabs-mode:nil; coding:utf-8 -*-
require 'osx/cocoa'

OSX.init_for_bundle do |bdl,param,log|
  require 'MyPluginClass'
end
