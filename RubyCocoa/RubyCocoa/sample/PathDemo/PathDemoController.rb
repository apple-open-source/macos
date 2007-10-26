#
#  PathDemoController.rb
#  PathDemo
#
#  Created by Laurent Sansonetti on 1/3/07.
#  Copyright (c) 2007 Apple Computer. All rights reserved.
#

class PathDemoController < NSObject

  ib_outlets :button, :popup, :window, :demoView

  def awakeFromNib
    @popup.removeAllItems
    ['Rectangles', 'Circles', 'Bezier Paths', 'Circle Clipping'].each do |title|
      @popup.addItemWithTitle(title)
    end
  end

  def runAgain(sender)
    select(self)
  end
  ib_action :runAgain
  
  def select(sender)
    @demoView.demoNumber = @popup.indexOfSelectedItem
    @demoView.setNeedsDisplay(true)
  end
  ib_action :select

  def print(sender)
    info = NSPrintInfo.sharedPrintInfo
	printOp = NSPrintOperation.printOperationWithView_printInfo(@demoView, info)
    printOp.setShowPanels(true)
	printOp.runOperation
  end
  ib_action :print

  def applicationShouldTerminateAfterLastWindowClosed(application)
    true
  end

end
