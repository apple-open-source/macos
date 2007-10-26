#
#  PeopleDataSource.rb
#  ABPresence
#
#  Created by Laurent Sansonetti on 1/4/07.
#  Copyright (c) 2007 Apple Computer. All rights reserved.
#

class PeopleDataSource < NSObject
  ib_outlet :table

  # Initialize and register for AddressBook notifications
  def awakeFromNib
    @imPersonStatus = []
    @abPeople = []

    nCenter = NSNotificationCenter.defaultCenter
    nCenter.objc_send :addObserver, self,
                      :selector, 'abDatabaseChangedExternallyNotification:',
                      :name, KABDatabaseChangedExternallyNotification,
                      :object, nil
    nCenter.objc_send :addObserver, self,
                      :selector, 'addressBookPersonStatusChanged:',
                      :name, KAddressBookPersonStatusChanged,
                      :object, nil
    
    reloadABPeople
  end

  # Data Loading

  def bestStatusForPerson(person)
    bestStatus = IMPersonStatusOffline # Let's assume they're offline to start
    IMService.allServices.to_a.each do |service|
      service.screenNamesForPerson(person).to_a.each do |screenName|
        dict = service.infoForScreenName(screenName)
        next if dict.nil?
        status = dict.objectForKey(IMPersonStatusKey)
        next if status.nil?
        thisStatus = status.intValue
        if thisStatus > bestStatus
          bestStatus = thisStatus 
        end
      end
    end
    return bestStatus
  end

  # This dumps all the status information and rebuilds the array against the current _abPeople
  # Fairly expensive, so this is only done when necessary
  def rebuildStatusInformation
    @imPersonStatus = @abPeople.map { |person| bestStatusForPerson(person) }
    @table.reloadData
  end
	

  # Rebuild status information for a given person, much faster than a full rebuild
  def rebuildStatusInformationForPerson(forPerson)
    @abPeople.each_with_index do |person, i|
      next unless person == forPerson
      @imPersonStatus[i] = bestStatusForPerson(person)
    end
    @table.reloadData
  end
  
  # This will do a full flush of people in our AB Cache, along with rebuilding their status 
  def reloadABPeople
    @abPeople = ABAddressBook.sharedAddressBook.people.to_a.sort do |x, y|
      x.displayName.to_s <=> y.displayName.to_s
    end
    rebuildStatusInformation
  end

  # NSTableView Data Source
  
  def numberOfRowsInTableView(tableView)
    @abPeople ? @abPeople.size : 0
  end
  
  def tableView_objectValueForTableColumn_row(tableView, tableColumn, row)
    case tableColumn.identifier.to_s
      when 'image'
        case @imPersonStatus[row]
          when IMPersonStatusAvailable, IMPersonStatusIdle
            NSImage.imageNamed('online')
          when IMPersonStatusAway
            NSImage.imageNamed('offline')
        end
      when 'name'
        @abPeople[row].displayName
    end
  end

  # Notifications

  # Posted from ServiceWatcher
  # The object of this notification is an ABPerson who's status has changed
  def addressBookPersonStatusChanged(notification)
    rebuildStatusInformationForPerson(notification.object)
  end

  # If the AB database changes, force a reload of everyone
  # We could look in the notification to catch differential updates, but for now
  # this is fine.
  def abDatabaseChangedExternallyNotification(notification)
    reloadABPeople
  end

end
