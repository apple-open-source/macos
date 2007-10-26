#
#  AppDelegate.rb
#  PDFKitViewer
#
#  Created by Laurent Sansonetti on 12/11/06.
#  Copyright (c) 2006 Apple Computer. All rights reserved.
#

class AppDelegate < NSObject
  def applicationShouldOpenUntitledFile(application)
    false
  end
end
