OSX.require_framework "WebKit"

class AppController < OSX::NSObject
  ib_outlet :historyMenu

  def dealloc
    OSX::NSNotificationCenter.defaultCenter.removeObserver(self)
    super_dealloc
  end

  def applicationDidFinishLaunching(aNotification)
    # Create a shared WebHistory object
    myHistory = OSX::WebHistory.alloc.init
    OSX::WebHistory.setOptionalSharedHistory(myHistory)
    
    # Observe WebHistory notifications
    nc = OSX::NSNotificationCenter.defaultCenter

    nc.addObserver_selector_name_object(self, 
                                        'historyDidLoad:',
                                        OSX::WebHistoryLoadedNotification,
                                        myHistory)

    # objc_send is a DSL to send a message with objective-c like
    # syntax. it's implemented in rb_main.rb.
    nc.objc_send  :addObserver, self, 
                  :selector,    'historyDidRemoveAllItems:',
                  :name,        OSX::WebHistoryAllItemsRemovedNotification,
                  :object,      myHistory

    nc.objc_send( :addObserver, self, 
                  :selector,    'historyDidAddItems:',
                  :name,        OSX::WebHistoryItemsAddedNotification,
                  :object,      myHistory )

    # '_' is a syntax sugar for objc_send
    nc._ :addObserver, self, 
         :selector,    'historyDidRemoveItems:',
         :name,        OSX::WebHistoryItemsRemovedNotification,
         :object,      myHistory
  end

  # WebHistory Notification Messages

  def historyDidLoad(aNotification)
    # Delete the old menu items
    self.removeAllHistoryMenuItems

    # Get the new history items
    items = OSX::WebHistory.optionalSharedHistory.
      orderedItemsLastVisitedOnDay(OSX::NSCalendarDate.calendarDate)
    enumerator = items.objectEnumerator

    # For each item, create a menu item with the history item as the
    # represented object
    while (historyItem = enumerator.nextObject) do
      self.addNewMenuItemForHistoryItem(historyItem)
    end
  end

  def historyDidAddItems(aNotification)
    # Get the new history items
    items = aNotification.userInfo.objectForKey(OSX::WebHistoryItemsKey)
    enumerator = items.objectEnumerator

    # For each item, create a menu item with the history item as the
    # represented object
    while (historyItem = enumerator.nextObject) do
      self.addNewMenuItemForHistoryItem(historyItem)
    end
  end

  def historyDidRemoveItems(aNotification)
    # Get the removed history items
    items = aNotification.userInfo.objectForKey(OSX::WebHistoryItemsKey)
    enumerator = items.objectEnumerator

    # For each item, remove the corresponding menu item
    while (historyItem = enumerator.nextObject) do
      self.removeMenuItemForHistoryItem(historyItem)
    end
  end

  def historyDidRemoveAllItems(aNotification)
    self.removeAllHistoryMenuItems
  end

  # Methods in support of adding and removing menu items

  # This method leaves the first menu item which is the Clear action
  def removeAllHistoryMenuItems
    while ((count = @historyMenu.numberOfItems) != 1)
      @historyMenu.removeItemAtIndex(count-1)
    end
  end

  def addNewMenuItemForHistoryItem(historyItem)
    # Create a new menu item, and set its action and target to invoke
    # goToHistoryItem:

    # objc_send is a DSL to send a message by objective-c like
    # syntax. it's implemented in rb_main.rb.
    menuItem = OSX::NSMenuItem.alloc.
       objc_send( :initWithTitle, historyItem.URLString,
                         :action, "goToHistoryItem:",
                  :keyEquivalent, "" )

    menuItem.setTarget(self)
    menuItem.setRepresentedObject(historyItem)

    # Add it to the menu
    @historyMenu.addItem(menuItem)
  end

  def removeMenuItemForHistoryItem(historyItem)
    # Get the menu item corresponsing to historyItem

    # *** Should really check for the represented object, not the
    # *** title because itemWithTitle: will return the first match

    # *** unless history items are guarantted not to have the same title?
    menuItem = @historyMenu.itemWithTitle(historyItem.originalURLString)
    if (menuItem.representedObject == historyItem)
      @historyMenu.removeItem(menuItem)
    else
      OSX.NSLog("Opps, multiple menu items with the same title, can't remove the right one!")
    end
  end

  # History Action Methods

  # Removes all the history items
  def clearHistory(sender)
    OSX::WebHistory.optionalSharedHistory.removeAllItems
  end
  ib_action :clearHistory

  # This method will be invoked by the menu item whose represented
  # object is the selected history item

  def goToHistoryItem(sender)
    # Ask the key window's delegate to load the history item
    keyWindow = OSX::NSApplication.sharedApplication.keyWindow
    if keyWindow != nil then
      keyWindow.delegate.goToHistoryItem(sender.representedObject)
    else
      # If there is no key window, create a new window to load the
      # history item
      aDocument = OSX::NSDocumentController.sharedDocumentController.
        openUntitledDocumentOfType_display("HTML Document", true)
      urlString = sender.representedObject.URLString
      aDocument.webView.mainFrame.
        loadRequest(OSX::NSURLRequest.
                    requestWithURL(OSX::NSURL.URLWithString(urlString)))
    end
  end
  ib_action :goToHistoryItem
end
