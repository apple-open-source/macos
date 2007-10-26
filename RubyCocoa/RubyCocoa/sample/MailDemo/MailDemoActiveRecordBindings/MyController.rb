#
#  Copyright (c) 2007 Eloy Duran <e.duran@superalloy.nl>
#

class MyController < NSObject
  
  kvc_accessor :mailboxes
  
  def init
    if super_init
      # This will load/create the dbfile in:
      # user/Library/Application Support/MailDemoActiveRecordBindingsApp/MailDemoActiveRecordBindingsApp.sqlite
      ActiveRecordConnector.connect_to_sqlite_in_application_support :log => true
      
      @mailboxes = Mailbox.find(:all).to_activerecord_proxies
      return self
    end
  end

end
