#
#  MyDocument.rb
#  ÇPROJECTNAMEÈ
#
#  Created by ÇFULLUSERNAMEÈ on ÇDATEÈ.
#  Copyright (c) ÇYEARÈ ÇORGANIZATIONNAMEÈ. All rights reserved.
#


require 'osx/cocoa'

class MyDocument < OSX::NSDocument

  def windowNibName
    # Override returning the nib file name of the document If you need
    # to use a subclass of NSWindowController or if your document
    # supports multiple NSWindowControllers, you should remove this
    # method and override makeWindowControllers instead.
    return "MyDocument"
  end

  def windowControllerDidLoadNib(aController)
    super_windowControllerDidLoadNib(aController)
    # Add any code here that need to be executed once the
    # windowController has loaded the document's window.
  end

  def dataRepresentationOfType(aType)
    # Insert code here to write your document from the given data.
    # You can also choose to override
    # fileWrapperRepresentationOfType or writeToFile_ofType
    # instead.
    return nil
  end

  def loadDataRepresentation_ofType(data, aType)
    # Insert code here to read your document from the given data.  You
    # can also choose to override
    # loadFileWrapperRepresentation_ofType or readFromFile_ofType
    # instead.
    return true
  end
end
