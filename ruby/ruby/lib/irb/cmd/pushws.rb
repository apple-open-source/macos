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
require "irb/ext/workspaces.rb"

module IRB
  module ExtendCommand
    class Workspaces<Nop
      def execute(*obj)
	irb_context.workspaces.collect{|ws| ws.main}
      end
    end

    class PushWorkspace<Workspaces
      def execute(*obj)
	irb_context.push_workspace(*obj)
	super
      end
    end

    class PopWorkspace<Workspaces
      def execute(*obj)
	irb_context.pop_workspace(*obj)
	super
      end
    end
  end
end

