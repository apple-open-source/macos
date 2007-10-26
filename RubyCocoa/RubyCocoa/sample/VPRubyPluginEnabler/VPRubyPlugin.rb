# -*- mode:ruby; indent-tabs-mode:nil; coding:utf-8 -*-
#
#  VPRubyPlugin.rb
#  RubyPluginEnabler
#
#  Created by Fujimoto Hisa on 07/02/02.
#  Copyright (c) 2007 FOBJ SYSTEMS. All rights reserved.

require 'osx/cocoa'
require 'VPRubyScript'

class VPRubyPlugin < OSX::NSObject
  include OSX

  def self.logger=(log) @@logger = log end
  def self.loginfo(fmt, *args) @@logger.info(fmt, *args) end
  def self.logerror(err)       @@logger.error(err)       end

  def self.install(enabler)
    if not defined? @@instance then
      @@instance = self.alloc.initWithEnabler(enabler)
    end
  end

  def initWithEnabler(enabler)
    @scripts = []
    @enabler = enabler
    load_scripts
    install_ruby_menu
    install_menu
    return self
  end

  def manager; @enabler.pluginManager end

  def savePanelDidEnd_returnCode_contextInfo(savePanel, returnCode, windowController)
    if returnCode == OSX::NSOKButton then
      s = windowController.textView.string
      s.dataUsingEncoding(OSX::NSUTF8StringEncoding).
        writeToFile_atomically(savePanel.filename, true)
      VPRubyScript.load(savePanel.filename.to_s).install_menu(manager) 
      # privateness, please ignore.
      # [[self pluginManager] performSelector:@selector(sortMenu)];
    end
  end
  objc_method :savePanelDidEnd_returnCode_contextInfo, "v@:@i@"

  private

  def loginfo(fmt, *args) VPRubyPlugin.loginfo(fmt, *args) end
  def logerror(err)       VPRubyPlugin.logerror(err)       end

  def load_scripts
    collect_scripts.each do |path|
      begin
        @scripts << VPRubyScript.load(path)
      rescue Exception => err
        logerror(err)
      end
    end
  end

  def install_ruby_menu
    run_page_as_script.install_menu(manager)
    save_page_as_script.install_menu(manager)
  end

  def install_menu
    @scripts.each { |i| i.install_menu(manager) }
  end

  def run_page_as_script
    VPRubyScript.create(
      :superMenuTitle => "Ruby",
      :menuTitle      => "Run Page as Ruby Plugin") do

      |windowController|

      buffer = windowController.textView.textStorage.mutableString
      r = windowController.textView.selectedRange
      if r.length > 0 then
        buffer = buffer.substringWithRange(r)
        # now put the insertion point at the end of the selection.
        r.location += r.length
        r.length = 0
        windowController.textView.setSelectedRange(r)
      end
      script = VPRubyScript.parse(buffer.to_s)
      script.execute(windowController)
    end
  end

  def save_page_as_script
    VPRubyScript.create(
      :superMenuTitle => "Ruby",
      :menuTitle      => "Save Page as Ruby Plugin...") do
      
      |windowController|
      
      # OSX.NSRunAlertPanel(SavePageAsScript.menuTitle, "not implement yet", nil, nil, nil)
      
      key = windowController.key
      displayName = windowController.document.vpDataForKey(key).displayName
      
      if not displayName then
        OSX.NSLog("I could not figure out the name of this page!")
        OSX.NSBeep
        break                     # exit from the action
      end
      
      name = "#{displayName}.rb"
      
      savePanel = OSX::NSSavePanel.savePanel
      
      savePanel.setPrompt(OSX.NSLocalizedString("Save", "Save"))
      savePanel.setTitle(OSX.NSLocalizedString("Save as Ruby Plugin", "Save as Ruby Plugin"))
      savePanel.
        objc_send( :beginSheetForDirectory, path_to_user_scripts,
                                     :file, name,
                           :modalForWindow, windowController.window,
                            :modalDelegate, self,
                           :didEndSelector, 'savePanelDidEnd:returnCode:contextInfo:',
                              :contextInfo, windowController )
    end
  end

  def collect_scripts
    script_pathes.map{|path| Dir["#{path}/*.rb"] }.flatten
  end

  def script_pathes
    [ path_to_bundle_scripts, path_to_user_scripts ]
  end

  def path_to_bundle_scripts
    bundle = OSX::NSBundle.bundleForClass(self.class)
    "#{bundle.resourcePath.to_s}/Script PlugIns"
  end

  def path_to_user_scripts
    path = "~/Library/Application Support/VoodooPad/Script PlugIns"
    path = File.expand_path(path)
    require 'fileutils'
    FileUtils.mkdir_p(path) if not File.exist?(path)
    return path
  end
end
