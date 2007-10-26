#
#  MyDocument.h
#  RubyRaiseMan
#
#  Created by FUJIMOTO Hisakuni on Sun Aug 11 2002.
#  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
#

require 'osx/cocoa'
require 'PreferenceController'

class MyDocument < OSX::NSDocument
  include OSX

  ib_outlets :deleteButton, :tableView

  def initialize
    @employees = Array.new
	NSNotificationCenter.defaultCenter.
	  addObserver self,
	  :selector, 'handleColorChange:',
	  :name, PreferenceController::BNRColorChanged,
	  :object, nil
  end

  def dealloc
	NSNotificationCenter.defaultCenter.removeObserver(self)
	super_dealloc
  end

  def deleteEmployee (sender)
    peapleToRemove = Array.new
    iter = @tableView.selectedRowEnumerator
	while index = iter.nextObject do
	  peapleToRemove.push @employees[index.to_i]
	end

    choice = OSX.NSRunAlertPanel NSLocalizedString("Delete"),
	  NSLocalizedString("SureDelete"),
	  NSLocalizedString("Yes"),
	  NSLocalizedString("No"),
	  nil, peapleToRemove.size

    if choice == NSAlertDefaultReturn then
	  peapleToRemove.each {|i| @employees.delete(i) }
	  updateChangeCount(NSChangeDone)
	  update_ui
	end
  end
  ib_action :deleteEmployee

  def newEmployee (sender)
    create_new_employee
    update_ui
  end
  ib_action :newEmployee

  # data source
  def numberOfRowsInTableView (tblView)
    @employees.size
  end

  def tableView_objectValueForTableColumn_row (tblView, col, row)
    identifier = col.identifier
    person = @employees[row]
    person.send(identifier.to_s.intern)
  end

  def tableView_setObjectValue_forTableColumn_row (tblView, obj, col, row)
    identifier = col.identifier
    person = @employees[row]
    if obj.isKindOfClass?(NSDecimalNumber) then
      obj = obj.to_f
    else
      obj = obj.to_s
    end
    person.send("#{identifier}=".intern, obj)
    updateChangeCount(NSChangeDone)
  end

  # delegate
  def tableViewSelectionDidChange (aNotification)
    @deleteButton.setEnabled(@employees.size > 0 && @tableView.selectedRow != -1)
  end

  def create_new_employee
    @employees.push(Person.new)
    updateChangeCount(NSChangeDone)
    @currentIndex = @employees.size - 1
  end

  def update_ui
    @tableView.reloadData if @tableView
    @deleteButton.setEnabled(@employees.size > 0 && @tableView.selectedRow != -1) if @deleteButton
  end

  def windowNibName
    return "MyDocument"
  end
    
  def windowControllerDidLoadNib (aController)
    super_windowControllerDidLoadNib(aController)

    defaults = NSUserDefaults.standardUserDefaults
    colorAsData = defaults[PreferenceController::BNRTableBgColorKey]
    @tableView.setBackgroundColor NSUnarchiver.unarchiveObjectWithData(colorAsData)

    update_ui
  end

  def dataRepresentationOfType (type)
    @tableView.deselectAll(nil)
    dumped_data = Marshal.dump @employees
    return NSArchiver.archivedDataWithRootObject(dumped_data)
  end
    
  def loadDataRepresentation_ofType (data, type)
    dumped_data = NSUnarchiver.unarchiveObjectWithData(data)
    @employees = Marshal.load(dumped_data.to_s)
    updateChangeCount(NSChangeCleared)
    update_ui
    return true
  end

  def handleColorChange (ntfy)
    @tableView.setBackgroundColor(ntfy.object)
    update_ui
  end

end
