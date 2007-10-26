#
#  PDFView.rb
#  CGPDFViewer
#
#  Created by Laurent Sansonetti on 12/12/06.
#  Copyright (c) 2006 Apple Computer. All rights reserved.
#

class PDFView <  NSView

  def initWithFrame(frameRect)
    super_initWithFrame(frameRect)
    @angle = 0.0
    return self
  end

  # This is where the real work gets done.
  def drawRect(rect)
    gc = NSGraphicsContext.currentContext.graphicsPort
    cg_rect = CGRect.new(rect.origin, rect.size)

    CGContextSetGrayFillColor(gc, 1.0, 1.0)
    CGContextFillRect(gc, cg_rect)

    # These next two calls do everything that's necessary to prepare for drawing the
    # page. The last argument of the first function defines whether or not to maintain
    # the document's aspect ratio.
    #
    # NOTE: The angle must be a multiple of 90. This is why we control angle's value
    # precisely in -rotateRight and -rotateLeft.
    m = CGPDFPageGetDrawingTransform(@page, KCGPDFMediaBox, cg_rect,
                                     @angle, true)
    CGContextConcatCTM(gc, m)

    # Now that the page is rotated and scaled correctly, all that remains is to draw it.
    CGContextDrawPDFPage(gc, page)
  end

  #
  # Document
  #

  def PDFDocument
    @pdfDocument
  end

  def setPDFDocument(doc)
    @pdfDocument = doc
    setPageNumber(1)
  end
  
  #
  # Pages
  #

  attr_reader :page, :pageNumber

  # In addition to setting the integer page number, this method creates the new
  # CGPDFPageRef and saves it to the 'page' variable. This is later referenced in
  # -drawRect:.
  def setPageNumber(newPageNumber)
    return if @pageNumber == newPageNumber
    raise "invalid new page number" if newPageNumber < 1 or newPageNumber > pageCount
  
    newPage = CGPDFDocumentGetPage(@pdfDocument, newPageNumber)
    raise "failed to create page" if newPage.nil?
    
    @pageNumber = newPageNumber
    @page = newPage
    
    self.setNeedsDisplay(true)
  end

  # This method just asks the document for its page count. We could cache the number,
  # but the function already does that, so we would only be saving a single function
  # call.
  def pageCount
    CGPDFDocumentGetNumberOfPages(@pdfDocument)
  end
  
  # These two are convenience methods for the Page Up and Page Down toolbar items.
  ib_action :incrementPageNumber do |sender|
    setPageNumber(@pageNumber + 1) if @pageNumber < pageCount
  end

  ib_action :decrementPageNumber do |sender|
    setPageNumber(@pageNumber - 1) if @pageNumber > 1
  end
  
  #
  # Rotation
  #

  # As mentioned in -drawRect:, `angle' must be a multiple of 90. These methods
  # guarantee that it is.
  ib_action :rotateRight do |sender|
    @angle += 90
    self.setNeedsDisplay(true)
  end

  ib_action :rotateLeft do |sender|
    @angle -= 90
    self.setNeedsDisplay(true)
  end
  
end
