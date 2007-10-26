#
#  AppController.rb
#  RubyRaiseMan
#
#  Created by FUJIMOTO Hisakuni on Sat Aug 17 2002.
#  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
#

require 'osx/cocoa'
require 'PreferenceController'

class AppController < OSX::NSObject
  include OSX
  OSX.ns_autorelease_pool do
    defaultValues = NSMutableDictionary.dictionary
    colorAsData = NSArchiver.archivedDataWithRootObject(NSColor.yellowColor)
    defaultValues[PreferenceController::BNRTableBgColorKey] = colorAsData
    defaultValues[PreferenceController::BNREmptyDocKey] = true
    NSUserDefaults.standardUserDefaults.registerDefaults(defaultValues)
  end

  def showPreferencePanel (sender)
    if @preferenceController.nil? then
      @preferenceController = PreferenceController.alloc.init
    end
    @preferenceController.showWindow(self)
  end
  ib_action :showPreferencePanel

  def applicationShouldOpenUntitledFile (sender)
    NSUserDefaults.standardUserDefaults.
      boolForKey(PreferenceController::BNREmptyDocKey)
  end

  def applicationDidBecomeActive (ntfy)
	# OSX.NSBeep
  end

end
