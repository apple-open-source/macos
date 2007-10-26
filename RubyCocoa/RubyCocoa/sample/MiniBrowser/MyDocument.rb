OSX.require_framework "WebKit"

class MyDocument < OSX::NSDocument
  ib_outlets :webView, :textField, :window, :backButton, :forwardButton

  def initialize
    @webView = @textField = @window = @backButton = @forwardButton = nil
    @docTitle = @frameStatus = @resourceStatus = @url = nil
    @urlToLoad = nil # URLToLoad;
    @resourceCount = @resourceFailedCount = @resourceCompletedCount = 0
  end

  def webView; return @webView end

  def windowNibName
    # Override returning the nib file name of the document
    # If you need to use a subclass of NSWindowController or if your
    # document supports multiple NSWindowControllers, you should
    # remove this method and override -makeWindowControllers instead.
    return "MyDocument"
  end

  def loadURL(url)
    @webView.mainFrame.loadRequest(OSX::NSURLRequest.requestWithURL(url))
  end

  def setURLToLoad(url)
    @urlToLoad = url
  end

  def windowControllerDidLoadNib(aController)
    super_windowControllerDidLoadNib(aController)
    # Add any code here that need to be executed once the
    # windowController has loaded the document's window.

    # Set the WebView delegates
    @webView.setFrameLoadDelegate(self)
    @webView.setUIDelegate(self)
    @webView.setResourceLoadDelegate(self)
    
    # Load a default URL
    url = if @urlToLoad != nil then @urlToLoad
          else OSX::NSURL.URLWithString("http://www.apple.com/") end
    self.loadURL(url)
    self.setURLToLoad(nil)
  end

  def dataRepresentationOfType(aType)
    @webView.mainFrame.dataSource.data
  end

  # - (BOOL)readFromURL:(NSURL *)URL ofType:(NSString *)type error:(NSError **)error
  def readFromURL_ofType_error(url, type, error)
    # If the WebView hasn't been created, load the URL in
    # windowControllerDidLoadNib:.
    if not @webView.nil?
    then self.loadURL(url)
    else self.setURLToLoad(url) end
    error = nil
    return true
  end

  # - (BOOL)readFromFile:(NSString *)path ofType:(NSString *)type
  def readFromFile_ofType(path, type)
    # This method is called on Panther and is deprecated on Tiger. 
    # On Tiger, readFromURL:ofType:error is called instead.
    error = OSX::OCObject.new
    return self.readFromURL_ofType_error(OSX::NSURL.fileURLWithPath(path),
                                         type, error)
  end

  # URL TextField Acton Methods
  # Action method invoked when the user enters a new URL
  def connectURL(sender)
    self.loadURL(OSX::NSURL.URLWithString(sender.stringValue))
  end
  ib_action :connectURL

  # Methods used to update the load status
  # Updates the status and error messages
  def updateWindow
    title = if @resourceStatus then
              format( "%s : %s", @docTitle, @resourceStatus )
            elsif @frameStatus then
              format( "%s : %s", @docTitle, @frameStatus )
            elsif @docTitle then
              @docTitle.to_s
            else
              "" end
    @webView.window.setTitle(title)
    @textField.setStringValue(url) if url
  end

  # Updates the resource status and error messages
  def updateResourceStatus
    stat = if @resourceFailedCount > 0 then
             format( "Loaded %d of %d, %d resource errors", 
                     @resourceCompletedCount,
                     @resourceCount - @resourceFailedCount,
                     @resourceFailedCount )
           else
             format( "Loaded %d of %d",
                     @resourceCompletedCount,
                     @resourceCount ) end
    setResourceStatus(stat)
    @webView.window.setTitle("%s : %s" % [@docTitle, @resourceStatus])
  end

  # Accessor methods for instance variables

  def docTitle; @docTitle end
  def setDocTitle(s)  @docTitle = s.to_s end

  def frameStatus; @frameStatus end
  def setFrameStatus(s)  @frameStatus = s.to_s end

  def resourceStatus; @resourceStatus end
  def setResourceStatus(s)  @resourceStatus = s.to_s end

  def url; @url end
  def setURL(s)  @url = s end

  # WebFrameLoadDelegate Methods

  def webView_didStartProvisionalLoadForFrame(sender, frame)
    # Only report feedback for the main frame.
    if frame == sender.mainFrame then
      # Reset resource status variables
      @resourceCount = @resourceCompletedCount = @resourceFailedCount = 0
      self.setFrameStatus("Loading...")
      self.setURL(frame.provisionalDataSource.request.URL.absoluteString)
      self.updateWindow
    end
  end

  def webView_didReceiveTitle_forFrame(sender, title, frame)
    # Only report feedback for the main frame.
    if frame.__ocid__ == sender.mainFrame.__ocid__ then
      self.setDocTitle(title)
      self.updateWindow
    end
  end

  def webView_didFinishLoadForFrame(sender, frame)
    # Only report feedback for the main frame.
    if frame.__ocid__ == sender.mainFrame.__ocid__ then
      self.setFrameStatus(nil)
      self.updateWindow
      @backButton.setEnabled(sender.canGoBack)
      @forwardButton.setEnabled(sender.canGoForward)
    end
  end

  def webView_didFailProvisionalLoadWithError_forFrame(sender, error, frame)
    # Only report feedback for the main frame.
    if frame.__ocid__ == sender.mainFrame.__ocid__ then
      self.setDocTitle("")
      self.setFrameStatus(error.description)
      self.updateWindow
    end
  end

  # WebUIDelegate Methods

  def webView_createWebViewWithRequest(sender, request)
    myDocument = OSX::NSDocumentController.sharedDocumentController.
      openUntitledDocumentOfType_display("HTML Document", true)
    myDocument.webView.mainFrame.loadRequest(request)
    return myDocument.webView
  end

  def webViewShow(sender)
    myDocument = OSX::NSDocumentController.sharedDocumentController.
      documentForWindow(sender.window)
    myDocument.showWindows
  end

  # WebResourceLoadDelegate Methods

  def webView_identifierForInitialRequest_fromDataSource(sender, request, dataSource)
    # Return some object that can be used to identify this resource
    return OSX::NSNumber.numberWithInt(@resourceCount += 1)
  end

  def webView_resource_willSendRequest_redirectResponse_fromDataSource(sender, identifier, request, redirectResponse, dataSource)
    # Update the status message
    self.updateResourceStatus
    return request
  end

  def webView_resource_didFailLoadingWithError_fromDataSource(sender, identifier, error, dataSource)
    # Increment the failed count and update the status message
    @resourceFailedCount += 1
    self.updateResourceStatus
  end

  def webView_resource_didFinishLoadingFromDataSource(sender, identifier, dataSource)
    # Increment the success count and pdate the status message
    @resourceCompletedCount += 1
    self.updateResourceStatus
  end

  # History Methods
  def goToHistoryItem(historyItem)
    # Load the history item in the main frame
    self.loadURL(OSX::NSURL.URLWithString(historyItem.URLString))
  end
end
