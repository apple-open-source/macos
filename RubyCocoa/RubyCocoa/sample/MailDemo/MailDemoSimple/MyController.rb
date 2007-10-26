#
#  MyController.rb
#  MailDemo
#
#  Created by Laurent Sansonetti on 1/8/07.
#  Copyright (c) 2007 Apple Computer. All rights reserved.
#

class MyController < NSObject

  ib_outlets :mailboxTable, :emailTable, :previewPane, :emailStatusLine, :mailboxStatusLine

  def init
    if super_init
      @mailboxes = NSMutableArray.array
      return self
    end
  end

  def awakeFromNib
    @previewPane.setEditable(false)
  end
  
  def currentMailbox
    idx = @mailboxTable.selectedRow
    @mailboxes.objectAtIndex(idx) unless idx < 0
  end
  
  def currentMailboxAndEmail
    mailbox = currentMailbox
    if mailbox
      idx = @emailTable.selectedRow
      [mailbox, mailbox.emails.objectAtIndex(idx)] unless idx < 0
    end
  end

  def addEmail(sender)
    # get current mailbox
    mailbox = currentMailbox
    return if mailbox.nil?
    
    # get mutable array of emails
    # and add new instance
    mailbox.emails.addObject(Email.alloc.init)
    
    # reload table and select new item
    @emailTable.reloadData
    @emailTable.selectRow_byExtendingSelection(mailbox.emails.count - 1, false)
  end
  ib_action :addEmail

  def removeEmail(sender)
    # get current mailbox
    mailbox = currentMailbox
    return if mailbox.nil?

    # get selected email
    idx = @emailTable.selectedRow
    return if idx < 0
    
    # get email list
    emails = mailbox.emails
    return if idx > emails.count - 1
    
    # remove object
    emails.removeObjectAtIndex(idx)
    
    # refresh UI
    @emailTable.reloadData
    if emails.count > 0
      @emailTable.selectRow_byExtendingSelection(emails.count - 1, false)
    end
  end
  ib_action :removeEmail

  def addMailbox(sender)
    # create and add new mailbox
    mailbox = Mailbox.alloc.init
    @mailboxes.addObject(mailbox)
    
    # reload table and select new item
    @mailboxTable.reloadData
    @mailboxTable.selectRow_byExtendingSelection(@mailboxes.count - 1, false)
  end
  ib_action :addMailbox

  def removeMailbox(sender)
    # get current mailbox
    idx = @mailboxTable.selectedRow
    return if idx < 0
    return if idx > @mailboxes.count - 1
    
    @mailboxes.removeObjectAtIndex(idx)
    
    # reload table and select new item
    @mailboxTable.reloadData
    @mailboxTable.selectRow_byExtendingSelection(@mailboxes.count - 1, false)
  end
  ib_action :removeMailbox

  def numberOfRowsInTableView(tableView)
    case tableView
      when @mailboxTable
        @mailboxStatusLine.setStringValue("#{@mailboxes.count} Mailboxes")
        @mailboxes.count
        
      when @emailTable
        mailbox = currentMailbox
        if mailbox.nil?
          0
        else
          @emailStatusLine.setStringValue("#{mailbox.emails.count} Emails")
          mailbox.emails.count
        end
    end
  end

  def tableView_objectValueForTableColumn_row(tableView, column, row)
    key = column.identifier
    case tableView
      when @mailboxTable
        @mailboxes.objectAtIndex(row).properties.objectForKey(key)
        
      when @emailTable
        mailbox = currentMailbox
        if mailbox.nil?
          ''
        else
          mailbox.emails.objectAtIndex(row).properties.objectForKey(key)
        end
      end
  end

  def tableView_setObjectValue_forTableColumn_row(tableView, object, column, row)
    key = column.identifier    
    properties = case tableView
      when @mailboxTable
        @mailboxes.objectAtIndex(row).properties

      when @emailTable
        currentMailbox.emails.objectAtIndex(row).properties
    end
    properties.setObject_forKey(object, key)
    tableView.reloadData
  end

  def tableViewSelectionDidChange(notification)
    if notification.object == @mailboxTable
      @mailboxTable.reloadData
      return
    end
        
    mailbox, email = currentMailboxAndEmail
    if mailbox.nil? or email.nil?
      @emailStatusLine.setStringValue('0 Emails')
      @previewPane.setString('')
      @previewPane.setEditable(false)
      return
    end

    @previewPane.setString(email.properties.objectForKey('body'))
    @previewPane.setEditable(true)
  end
  
  def textDidEndEditing(notification)
    string = notification.object.string
 
    mailbox, email = currentMailboxAndEmail
    return if mailbox.nil? or email.nil?

    email.properties.setObject_forKey(string.copy, 'body')
  end

=begin
// Workaround for apparent reload bug in NSTableView.
//
// See this post for more info:
//    http://cocoa.mamasam.com/MACOSXDEV/2003/11/1/76580.php
=end

  def tableViewSelectionIsChanging(notification)
    if notification.object == @mailboxTable
      @emailTable.noteNumberOfRowsChanged
    end
  end

end
