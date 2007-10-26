#
#  ServiceWatcher.rb
#  ABPresence
#
#  Created by Laurent Sansonetti on 1/4/07.
#  Copyright (c) 2007 Apple Computer. All rights reserved.
#

KAddressBookPersonStatusChanged = "AddressBookPersonStatusChanged"

class ServiceWatcher < NSObject

  def startMonitoring
    nCenter = IMService.notificationCenter
    nCenter.objc_send :addObserver, self,
                      :selector, 'imPersonStatusChangedNotification:',
                      :name, IMPersonStatusChangedNotification,
                      :object, nil
    nCenter.objc_send :addObserver, self,
                      :selector, 'imPersonInfoChangedNotification:',
                      :name, IMPersonInfoChangedNotification,
                      :object, nil
  end
  
  def stopMonitoring
    IMService.notificationCenter.removeObserver(self)
  end
  
  def awakeFromNib
    startMonitoring
  end
  
  # Notifications
  def forwardToObservers(notification)
    service = notification.object
    screenName = notification.userInfo.objectForKey(IMPersonScreenNameKey)
    nCenter = NSNotificationCenter.defaultCenter
    service.peopleWithScreenName(screenName).to_a.each do |person|
      nCenter.postNotificationName_object(KAddressBookPersonStatusChanged, person)
    end
  end
  
  def imPersonStatusChangedNotification(notification)
    forwardToObservers(notification)
  end
  
  def imPersonInfoChangedNotification(notification)
    forwardToObservers(notification)
  end
  
end
