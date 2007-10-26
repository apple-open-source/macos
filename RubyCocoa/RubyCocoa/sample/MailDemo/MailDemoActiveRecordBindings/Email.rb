#
#  Copyright (c) 2007 Eloy Duran <e.duran@superalloy.nl>
#

class Email < ActiveRecord::Base
  belongs_to :mailbox
  
  validates_presence_of :address
  validates_format_of :address,
                      :with => /^([^@\s]+)@((?:[-a-z0-9]+\.)+[a-z]{2,})$/i,
                      :message => 'is not a valid email address'
  
  validates_presence_of :subject
end
