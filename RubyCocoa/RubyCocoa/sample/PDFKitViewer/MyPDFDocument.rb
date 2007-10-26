#
#  MyPDFDocument.rb
#  PDFKitViewer
#
#  Created by Laurent Sansonetti on 12/11/06.
#  Copyright (c) 2006 Apple Computer. All rights reserved.
#

class MyPDFDocument < NSDocument

  ib_outlets :pdfView, :drawer, :outlineView, :noOutlineText, :searchTable, :searchProgress

  def initialize
    @searchResults = []
  end

  def dealloc
    NSNotificationCenter.defaultCenter.removeObserver(self)
    super_dealloc
  end

  def windowNibName
    'MyPDFDocument'
  end
  
  def windowControllerDidLoadNib(controller)  
    # Super.
    super_windowControllerDidLoadNib(controller)

    # Load PDF.
    if self.fileName
      pdfDoc = PDFDocument.alloc.initWithURL(NSURL.fileURLWithPath(self.fileName))
      @pdfView.setDocument(pdfDoc)
    end
  
    # Page changed notification.
    notifCenter = NSNotificationCenter.defaultCenter
    notifCenter.addObserver_selector_name_object(self, 'pageChanged:', PDFViewPageChangedNotification, @pdfView)

    # Find notifications.
    doc = @pdfView.document
    notifCenter.addObserver_selector_name_object(self, 'startFind:', PDFDocumentDidBeginFindNotification, doc)
    notifCenter.addObserver_selector_name_object(self, 'findProgress:', PDFDocumentDidEndPageFindNotification, doc)
    notifCenter.addObserver_selector_name_object(self, 'endFind:', PDFDocumentDidEndFindNotification, doc)
	
    # Set self to be delegate (find).
    doc.setDelegate(self)
	
    # Get outline.
    @outline = doc.outlineRoot
    if @outline
      # Remove text that says, "No outline."
      @noOutlineText.removeFromSuperview
      @noOutlineText = nil
      
      # Force it to load up.
      @outlineView.reloadData
    else
      # Remove outline view (leaving instead text that says, "No outline.").
      @outlineView.enclosingScrollView.removeFromSuperview
      @outlineView = nil
    end
    
    # Open drawer.
    @drawer.open
  end   
    
  def dataRepresentationOfType(type)
    nil
  end
    
  def loadDataRepresentation_ofType(data, type)
    true
  end

  def toggleDrawer(sender)
    @drawer.toggle(self)
  end
  ib_action :toggleDrawer

  def takeDestinationFromOutline(sender)
    # Get the destination associated with the search result list.  Tell the PDFView to go there.
    item = sender.itemAtRow(sender.selectedRow)
    @pdfView.goToDestination(item.destination) if item
  end
  ib_action :takeDestinationFromOutline

  def displaySinglePage(sender)
    # Display single page mode.
    if @pdfView.displayMode > KPDFDisplaySinglePageContinuous
      @pdfView.setDisplayMode(@pdfView.displayMode - 2)
    end
  end
  ib_action :displaySinglePage

  def displayTwoUp(sender)
	  # Display two-up.
	  if @pdfView.displayMode < KPDFDisplayTwoUp
		  @pdfView.setDisplayMode(@pdfView.displayMode + 2)
    end
  end
  ib_action :displayTwoUp
  
  def pageChanged(notification)
    doc = @pdfView.document
    
    # Skip out if there is no outline.
    return if doc.outlineRoot.nil?
      
    # What is the new page number (zero-based).
    newPageIndex = doc.indexForPage(@pdfView.currentPage)
    
    # Walk outline view looking for best firstpage number match.
    newlySelectedRow = -1
    numRows = @outlineView.numberOfRows
    numRows.times do |i|      
      # Get the destination of the given row....
      outlineItem = @outlineView.itemAtRow(i)
      
      index = @pdfView.document.indexForPage(outlineItem.destination.page)
      if index == newPageIndex
        newlySelectedRow = i
        @outlineView.selectRow_byExtendingSelection(newlySelectedRow, false)
        break
      elsif index < newPageIndex
        newlySelectedRow = i - 1
        @outlineView.selectRow_byExtendingSelection(newlySelectedRow, false)
        break
      end
    end
    
    # Auto-scroll.
    if newlySelectedRow != -1
      @outlineView.scrollRowToVisible(newlySelectedRow)
    end
  end

  def doFind(sender)
    doc = @pdfView.document
    doc.cancelFindString if doc.isFinding    
    doc.beginFindString_withOptions(sender.stringValue, NSCaseInsensitiveSearch)
  end
  ib_action :doFind

  def startFind(notification)
    # Empty arrays.
    @searchResults.clear

    @searchTable.reloadData
    @searchProgress.startAnimation(self)
  end

  def findProgress(notification)
    pageIndex = notification.userInfo.objectForKey('PDFDocumentPageIndex').doubleValue
    @searchProgress.setDoubleValue(pageIndex / @pdfView.document.pageCount)
  end
  
  # Called when an instance was located. Delegates can instantiate.
  def didMatchString(instance)
    # Add page label to our array.
    @searchResults << instance.copy
    
    # Force a reload.
    @searchTable.reloadData
  end

  def endFind(notification)
    @searchProgress.stopAnimation(self)
    @searchProgress.setDoubleValue(0)
  end

  # The table view is used to hold search results.  Column 1 lists the page number for the search result, 
  # column two the section in the PDF (x-ref with the PDF outline) where the result appears.
  def numberOfRowsInTableView(aTableView)
    @searchResults ? @searchResults.length : 0
  end
  
  def tableView_objectValueForTableColumn_row(aTableView, theColumn, rowIndex)
    case theColumn.identifier.to_s
      when 'page'
        @searchResults[rowIndex].pages.objectAtIndex(0).label
      when 'section'
        item = @pdfView.document.outlineItemForSelection(@searchResults[rowIndex])
        item ? item.label : nil
    end
  end

  def tableViewSelectionDidChange(notification)
    # What was selected.  Skip out if the row has not changed.
    rowIndex = notification.object.selectedRow
    if rowIndex >= 0
      @pdfView.setCurrentSelection(@searchResults[rowIndex])
      @pdfView.centerSelectionInVisibleArea(self)
    end
  end

  # The outline view is for the PDF outline.  Not all PDF's have an outline.
  def outlineView_numberOfChildrenOfItem(outlineView, item)
    if item
      item.numberOfChildren
    else
      @outline ? @outline.numberOfChildren : 0
    end
  end

  def outlineView_child_ofItem(outlineView, index, item)
    if item
      item.childAtIndex(index)
    else
      @outline ? @outline.childAtIndex(index) : nil
    end
  end

  def outlineView_isItemExpandable(outlineView, item)
    if item
      item.numberOfChildren > 0
    else
      @outline ? @outline.numberOfChildren > 0 : false
    end
  end

  def outlineView_objectValueForTableColumn_byItem(outlineView, tableColumn, item)
    item.label
  end
end
