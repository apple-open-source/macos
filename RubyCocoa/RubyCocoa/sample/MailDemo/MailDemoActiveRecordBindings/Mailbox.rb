#
#  Copyright (c) 2007 Eloy Duran <e.duran@superalloy.nl>
#

class Mailbox < ActiveRecord::Base
  has_many :emails, :dependent => :destroy
  
  validates_presence_of :title
end
