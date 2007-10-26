require 'osx/appkit'
require 'osx/foundation'
require 'MyViewerCtrl'

if __FILE__ == $0 then
  controller = MyViewerCtrl.alloc.init
  app = OSX::NSApplication.sharedApplication
  app.setDelegate(controller)
  app.setMainMenu(controller.mainMenu)
  OSX::NSApp.run
end
