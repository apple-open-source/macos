#
#  Copyright (c) 2007 Eloy Duran <e.duran@superalloy.nl>
#

class EmailProxy < ActiveRecordProxy
  on_get :body, :return => [OSX::NSAttributedString, :initWithString]
end
