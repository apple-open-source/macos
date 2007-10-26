require 'tk'
require 'hako'

module Hako

  class TkHako
    def initialize(parent=nil, unit_size=64)
      @w = 4
      @h = 5
      @unit_size = unit_size
      @house = TkFrame.new(parent,
			   'width' => @unit_size * @w,
			   'height' => @unit_size * @h).pack
      @chips = {}
    end
    
    def add_chip(chip)
      widget = TkLabel.new(@house,
			   'text' => chip.name,
			   'relief' => 'raised')
      @chips[chip] = widget

      widget.bind('B1-Motion', proc{|x, y| do_motion(x, y, chip)}, "%x %y")
    end

    def do_motion(x, y, chip)
      if x >= @unit_size * chip.w
	dx = 1
      elsif x < 0
	dx = -1
      else
	dx = 0
      end

      if y >= @unit_size * chip.h
	dy = 1
      elsif y < 0
	dy = -1
      else
	dy = 0
      end

      if (dx != 0)
	dy = 0
      end

      return if (dx == 0) and (dy == 0)

      chip.try_move(dx, dy)
    end

    def moveto_chip(x, y, chip)
      widget = @chips[chip]
      widget.place('x' => @unit_size * x,
		   'y' => @unit_size * y,
		   'width' => @unit_size * chip.w,
		   'height' => @unit_size * chip.h)
    end
  end

end

Hako::Game.new(Hako::TkHako.new(nil, 64))
Tk.mainloop
