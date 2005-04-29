#
#   irb/ws-for-case-2.rb - 
#   	$Release Version: 0.9$
#   	$Revision: 1.2 $
#   	$Date: 2002/07/09 11:17:16 $
#   	by Keiju ISHITSUKA(keiju@ishitsuka.com)
#
# --
#
#   
#

while true
  IRB::BINDING_QUEUE.push b = binding
end
