#
#  StickiesAppDelegate.rb
#  Stickies
#
#  Created by Laurent Sansonetti on 1/4/07.
#  Copyright (c) 2007 Apple Computer. All rights reserved.
#

class StickiesAppDelegate < NSObject#< StickiesAppDelegateBase #NSObject

  ib_outlet :stickiesController
  kvc_writer :managedObjectContext
  kvc_writer :managedObjectModel
  
  def applicationDidFinishLaunching(notification)
    # Next time through the event loop, load all the stickies.
    # This delay is required because the NSArrayController's fetch request is delayed until the next event loop
    objc_send :performSelector, 'loadAllStickies',
              :withObject, nil,
              :afterDelay, 0.0
  end
  
  def loadAllStickies
    # Fault each sticky in the array controller and call it's setup function
    @stickiesController.arrangedObjects.to_a.each { |x| x.setupSticky }
  end
  
  def removeSticky(sticky)
    @stickiesController.removeObject(sticky)
  end

  def managedObjectModel
    return @managedObjectModel if @managedObjectModel

    allBundles = OSX::NSMutableSet.alloc.init
    allBundles.addObject(OSX::NSBundle.mainBundle)
    allBundles.addObjectsFromArray(OSX::NSBundle.allFrameworks)

    @managedObjectModel = OSX::NSManagedObjectModel.mergedModelFromBundles(allBundles.allObjects)
    OSX::CoreData.define_wrapper(@managedObjectModel)
  
    return @managedObjectModel
  end

  # Change this path/code to point to your App's data store.
  def applicationSupportFolder
    OSX.NSHomeDirectory.stringByAppendingPathComponent(File.join('Library', 'Application Support', 'CoreData Stickies'))
  end

  def managedObjectContext
    return @managedObjectContext if @managedObjectContext

    fileManager = OSX::NSFileManager.defaultManager
    storeFolder = applicationSupportFolder
    unless fileManager.fileExistsAtPath_isDirectory(storeFolder, nil)
      fileManager.createDirectoryAtPath_attributes(storeFolder, nil)
    end

    url = OSX::NSURL.fileURLWithPath(storeFolder.stringByAppendingPathComponent('Stickies.xml'))
    coordinator = OSX::NSPersistentStoreCoordinator.alloc.initWithManagedObjectModel(managedObjectModel)
    ok, error = coordinator.objc_send :addPersistentStoreWithType, NSXMLStoreType, 
                                      :configuration, nil, 
                                      :URL, url, 
                                      :options, nil, 
                                      :error, nil # XXX
    if ok
      @managedObjectContext = OSX::NSManagedObjectContext.alloc.init
      @managedObjectContext.setPersistentStoreCoordinator(coordinator)
    else 
      raise "error" # XXX
      OSX::NSApplication.sharedApplication.presentError(error)
    end
    return @managedObjectContext
  end

  def windowWillReturnUndoManager(window)
    return managedObjectContext.undoManager
  end

  def saveAction(sender)
    ok, error = managerObjectContext.save(nil) # XXX
    unless ok
      raise "error" # XXX
      OSX::NSApplication.sharedApplication.presentError(error)
    end
  end
  ib_action :saveAction

  def applicationShouldTerminate(sender)
    reply = OSX::NSTerminateNow

    context = managedObjectContext
    if context
      if context.commitEditing
        ok, error = context.save(nil) # XXX
        unless ok
          raise "error" # XXX
          errorResult = OSX::NSApplication.sharedApplication.presentError?(error)

          if errorResult
            reply = OSX::NSTerminateCancel
          else
            alertReturn = OSX.NSRunAlertPanel(nil, "Could not save changes while quitting. Quit anyway?", "Quit anyway", "Cancel", nil)
            if alertReturn == OSX::NSAlertAlternateReturn
              reply = OSX::NSTerminateCancel
            end
          end
        end
      else
        reply = OSX::NSTerminateCancel
      end
    end
    return reply
  end

end
