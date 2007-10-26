#
#  Email.rb
#  MailDemo
#
#  Created by Laurent Sansonetti on 1/7/07.
#  Copyright (c) 2007 Apple Computer. All rights reserved.
#

class Email < NSObject

  kvc_accessor :properties

  def init
    if super_init
      @properties = NSMutableDictionary.dictionaryWithObjects_forKeys(
        ['test@test.com', 'Subject', NSDate.date, NSString.string],
        ['address', 'subject', 'date', 'body'])
      return self
    end
  end

end
