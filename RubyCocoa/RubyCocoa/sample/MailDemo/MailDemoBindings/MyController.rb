#
#  MyController.rb
#  MailDemo
#
#  Created by Laurent Sansonetti on 1/8/07.
#  Copyright (c) 2007 Apple Computer. All rights reserved.
#
#  Eloy Duran 5/8/07
#  Updated to the final stage of the tutorial at: http://cocoadevcentral.com/articles/000080.php

class MyController < NSObject
  
  kvc_accessor :mailboxes
  
  def init
    if super_init
      @mailboxes = NSMutableArray.array
      return self
    end
  end

end
