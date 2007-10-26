require 'osx/cocoa'
require 'WindowAController'
require 'WindowBController'

class ApplicationDelegate < OSX::NSObject

  def createWindowA(sender)
    controller = WindowAController.alloc.init
    controller.showWindow(self)
  end
  ib_action :createWindowA

  def createWindowB(sender)
    controller = WindowBController.alloc.init
    controller.showWindow
  end
  ib_action :createWindowB
end
