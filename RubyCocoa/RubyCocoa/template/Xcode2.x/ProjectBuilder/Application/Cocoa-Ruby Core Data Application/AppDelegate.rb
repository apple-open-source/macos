#  ÇPROJECTNAMEASIDENTIFIERÈAppDelegate.rb
#  ÇPROJECTNAMEÈ
#
#  Created by ÇFULLUSERNAMEÈ on ÇDATEÈ.
#  Copyright ÇORGANIZATIONNAMEÈ ÇYEARÈ . All rights reserved.

require 'osx/cocoa'
OSX.require_framework 'CoreData'

class AppDelegate < OSX::NSObject
  
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
    OSX.NSHomeDirectory.stringByAppendingPathComponent(File.join('Library', 'Application Support', 'ÇPROJECTNAMEASIDENTIFIERÈ'))
  end
  
  def managedObjectContext
    return @managedObjectContext if @managedObjectContext
    
    fileManager = OSX::NSFileManager.defaultManager
    storeFolder = applicationSupportFolder
    unless fileManager.fileExistsAtPath_isDirectory?(storeFolder, nil)
      fileManager.createDirectoryAtPath_attributes(storeFolder, nil)
    end
    
    url = OSX::NSURL.fileURLWithPath(storeFolder.stringByAppendingPathComponent("ÇPROJECTNAMEASIDENTIFIERÈ.xml"))
    coordinator = OSX::NSPersistentStoreCoordinator.alloc.initWithManagedObjectModel(managedObjectModel)
    # FIXME: cannot get errors
    error = OSX::OCObject.new
    if coordinator.addPersistentStoreWithType_configuration_URL_options_error(OSX::NSXMLStoreType, nil, url, nil, error) then
      @managedObjectContext = OSX::NSManagedObjectContext.alloc.init
      @managedObjectContext.setPersistentStoreCoordinator(coordinator)
    else
      OSX::NSApplication.sharedApplication.presentError(error)
    end
    
    return @managedObjectContext
  end

  def windowWillReturnUndoManager(window)
    return managedObjectContext.undoManager
  end
  
  def saveAction(sender)
    error = nil
    unless managedObjectContext.save?(error)
      OSX::NSApplication.sharedApplication.presentError(error)
    end
  end
  ib_action :saveAction
  
  def applicationShouldTerminate(sender)
    reply = OSX::NSTerminateNow
    
    context = managedObjectContext
    if context
      if context.commitEditing?
        # FIXME: cannot get errors
        error = OSX::OCObject.new
        unless context.save?(error)
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
