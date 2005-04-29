#
#   change-ws.rb - 
#   	$Release Version: 0.9$
#   	$Revision: 1.1 $
#   	$Date: 2002/07/09 11:17:17 $
#   	by Keiju ISHITSUKA(Nihon Rational Software Co.,Ltd)
#
# --
#
#   
#

require "irb/cmd/nop.rb"
require "irb/ext/change-ws.rb"

module IRB
  module ExtendCommand

    class CurrentWorkingWorkspace<Nop
      def execute(*obj)
	irb_context.main
      end
    end

    class ChangeWorkspace<Nop
      def execute(*obj)
	irb_context.change_workspace(*obj)
	irb_context.main
      end
    end
  end
end

