require 'osx/cocoa'
require 'WinCtrl'
require 'MyInspector'

class MyViewerCtrl < OSX::NSObject

  def mainMenu
    menu = OSX::NSMenu.alloc.initWithTitle "MyViewer"
    item = menu.
      addItemWithTitle( "File",
		       :action, "submenuAction:",
		       :keyEquivalent, "")
    submenu = OSX::NSMenu.alloc.initWithTitle "File"
    menu.setSubmenu(submenu, :forItem, item)
    item = submenu.
      addItemWithTitle("Open File...",
		       :action, "openFile:",
		       :keyEquivalent, "o")
    item.setTarget(self)
    item = submenu.
      addItemWithTitle("Inspector...",
		       :action, "activateInspector:",
		       :keyEquivalent, "i")
    item.setTarget(self)
    menu
  end

  def openFile (sender)
    fileTypes = OSX::NSImage.imageFileTypes
    oPanel = OSX::NSOpenPanel.openPanel
    filesToOpen = nil
    enumerator = nil
    aFile = nil
    result = nil

    oPanel.setAllowsMultipleSelection(true)
    result = oPanel.
      runModalForDirectory(OSX.NSHomeDirectory,
			   :file, nil,
			   :types, fileTypes)
    return if result != OSX::NSOKButton
    filesToOpen = oPanel.filenames
    enumerator = filesToOpen.objectEnumerator
    while (aFile = enumerator.nextObject) != nil do
      WinCtrl.alloc.initWithPath(aFile)
    end
  end
  ib_action :openFile

  def activateInspector (sender)
    MyInspector.defaultInstance
  end
  ib_action :activateInspector

  def showTitleMessage (title, message)
    panel = OSX::NSGetAlertPanel(title, "%@", nil, nil, nil, message)
    panel.center
    panel.makeKeyAndOrderFront(panel)
    OSX::NSThread.sleepUntilDate(OSX::NSDate.dateWithTimeIntervalSinceNow(3))
    panel.close
    OSX::NSReleaseAlertPanel(panel)
  end

  def applicationDidFinishLaunching (aNotification)
    showTitleMessage("Start", "Hello, World")
  end

  def applicationWillTerminate (aNotification)
    showTitleMessage("Exit", "Sayonara, Baby.")
  end

end
