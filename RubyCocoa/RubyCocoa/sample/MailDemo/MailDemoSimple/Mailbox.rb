#
#  Mailbox.rb
#  MailDemo
#
#  Created by Laurent Sansonetti on 1/8/07.
#  Copyright (c) 2007 Apple Computer. All rights reserved.
#

class Mailbox < NSObject

  kvc_accessor :properties, :emails

  def init
    if super_init
      @properties = NSMutableDictionary.dictionaryWithObject_forKey('New Mailbox', 'title')
      @emails = NSMutableArray.array
      return self
    end
  end

end
