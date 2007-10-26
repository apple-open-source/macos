#
#  MyDocument.rb
#  CGPDFViewer
#
#  Created by Laurent Sansonetti on 12/12/06.
#  Copyright (c) 2006 Apple Computer. All rights reserved.
#

class MyDocument < NSDocument
  attr_reader :pdfView
  ib_outlets :pdfView, :window

  # String constants for the toolbar.
  TB_IDENTIFIER = 'My Document Toolbar Identifier'
  TB_ROTATE_LEFT_IDENTIFIER = 'Rotate Left Toolbar Item Identifier'
  TB_ROTATE_RIGHT_IDENTIFIER = 'Rotate Right Toolbar Item Identifier'
  TB_PAGE_UP_IDENTIFIER = 'Page Up Toolbar Item Identifier'
  TB_PAGE_DOWN_IDENTIFIER = 'Page Down Toolbar Item Identifier'

  def windowNibName
    'MyDocument'
  end

  def windowControllerDidLoadNib(controller)
    doc = CGPDFDocumentCreateWithURL(@documentURL)
    super_windowControllerDidLoadNib(controller)
    @window = controller.window
    setupToolbar
    @pdfView.setPDFDocument(doc)
    @pdfView.setNeedsDisplay(true)
  end

  def dataRepresentationOfType(docType)
    # We aren't saving anything, so we can leave this method alone.
    nil
  end

  def readFromFile_ofType(name, docType)
    # We save the URL for now because this method gets called before
    # the PDFView has been created.
    @documentURL = NSURL.alloc.initFileURLWithPath(name)
    
    # Hold onto the file name for use in the window's displayName.
    @fileName = name.componentsSeparatedByString("/").lastObject
    true
  end

  # Changes the display name so we can see the document's page count in the
  # window title.
  def displayName
    count = @pdfView.pageCount
    count == 1 ? "#{fileName} (1 page)" : "#{fileName} (#{count} pages)"
  end

  #
  # Toolbar methods
  #

  def setupToolbar
    # Create a new toolbar instance, and attach it to our document window.
    @toolbar ||= NSToolbar.alloc.initWithIdentifier(TB_IDENTIFIER)
    
    # Set up toolbar properties: Allow customization, give a default display mode
    # and remember state in user defaults.
    @toolbar.setAllowsUserCustomization(true);
    @toolbar.setAutosavesConfiguration(true);
    @toolbar.setDisplayMode(NSToolbarDisplayModeIconAndLabel)
    
    # We are the delegate.
    @toolbar.setDelegate(self)
    
    # Attach the toolbar to the document window.
    @window.setToolbar(@toolbar)
  end
  
  def toolbarDefaultItemIdentifiers(toolbar)
    [ TB_PAGE_UP_IDENTIFIER, TB_PAGE_DOWN_IDENTIFIER,
      NSToolbarSeparatorItemIdentifier, 
      TB_ROTATE_LEFT_IDENTIFIER, TB_ROTATE_RIGHT_IDENTIFIER ]
  end

  def toolbarAllowedItemIdentifiers(toolbar)
    [ NSToolbarPrintItemIdentifier,
      NSToolbarCustomizeToolbarItemIdentifier,
      NSToolbarFlexibleSpaceItemIdentifier,
      NSToolbarSpaceItemIdentifier,
      NSToolbarSeparatorItemIdentifier,
      TB_ROTATE_LEFT_IDENTIFIER, TB_ROTATE_RIGHT_IDENTIFIER,
      TB_PAGE_UP_IDENTIFIER, TB_PAGE_DOWN_IDENTIFIER ]
  end

  def validateToolbarItem(item)
    identifier = item.itemIdentifier.to_s
    (identifier != TB_PAGE_DOWN_IDENTIFIER or @pdfView.pageCount != @pdfView.pageNumber) \
    or (identifier != TB_PAGE_UP_IDENTIFIER or @pdfView.pageCount != 1)
  end

  def toolbar_itemForItemIdentifier_willBeInsertedIntoToolbar(toolbar, identifier, flag)
    label, tooltip, image, action = 
      case identifier.to_s
        when TB_ROTATE_LEFT_IDENTIFIER
          [ "Rotate Left", 
            "Rotate every page 90 degrees to the left", 
            "RotateLeftToolbarImage",
            'rotateLeft:' ]
        when TB_ROTATE_RIGHT_IDENTIFIER
          [ "Rotate Right",
            "Rotate every page 90 degrees to the right",
            "RotateRightToolbarImage",
            'rotateRight:' ]
        when TB_PAGE_UP_IDENTIFIER
          [ "Page Up",
            "Go to the previous page",
            "PageUpToolbarImage",
            'decrementPageNumber:' ]
        when TB_PAGE_DOWN_IDENTIFIER
          [ "Page Down",
            "Go to the next page",
            "PageDownToolbarImage",
            'incrementPageNumber:' ]
        else
          return nil
      end

    toolbarItem = NSToolbarItem.alloc.initWithItemIdentifier(identifier)
    toolbarItem.setLabel(label)
    toolbarItem.setPaletteLabel(label)
    toolbarItem.setToolTip(tooltip)
    toolbarItem.setImage(NSImage.imageNamed(image))
    toolbarItem.setTarget(@pdfView)
    toolbarItem.setAction(action)
  
    return toolbarItem
  end
end
