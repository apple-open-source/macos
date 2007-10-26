#
#  Person.rb
#  RubyRaiseMan
#
#  Created by FUJIMOTO Hisakuni on Sun Aug 11 2002.
#  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
#

class Person
  attr_accessor :personName, :expectedRaise
  def initialize
    @personName = "New Employee"
    @expectedRaise = 0.0
  end
end
