#
#   irb/ws-for-case-2.rb - 
#   	$Release Version: 0.7.3$
#   	$Revision: 1.1.1.1 $
#   	$Date: 2002/05/27 17:59:49 $
#   	by Keiju ISHITSUKA(keiju@ishitsuka.com)
#
# --
#
#   
#

while true
  IRB::BINDING_QUEUE.push b = binding
end
