module Hako

  class House
    def initialize(widget)
      @w = 4
      @h = 5
      @widget = widget
      @chips = []
      self
    end
    attr :widget
    attr :unit_size
    attr :chips

    def enter(chip)
      @chips.push(chip)
    end

    def wall
      hash = {}
      (-1..@w).each do |i|
	hash[[i, -1]] = true
	hash[[i, @h]] = true
      end
      (-1..@h).each do |i|
	hash[[-1, i]] = true
	hash[[@w, i]] = true
      end
      hash
    end

    def move?(chip, dx, dy)
      field = self.wall
      @chips.each do |c|
	unless c == chip
	  c.area.each do |a|
	    field[a] = chip
	  end
	end
      end

      chip.area(dx, dy).each do |a|
	return false if field[a]
      end
      return true
    end
  end

  class Chip
    def initialize(house, name, x, y, w=1, h=1)
      @name = name
      @w = w
      @h = h

      @house = house
      
      house.enter(self)
      house.widget.add_chip(self)
      moveto(x, y)
    end
    attr_reader :name, :w, :h

    def area(dx=0, dy=0)
      v = []
      for i in (1..@h)
	for j in (1..@w)
	  v.push([dx + @x + j - 1, dy + @y + i - 1])
	end
      end
      v
    end

    def moveto(x, y)
      @x = x
      @y = y
      @house.widget.moveto_chip(x, y, self)
    end

    def move(dx, dy)
      x = @x + dx
      y = @y + dy
      moveto(x, y)
    end
  
    def do_motion(x, y)
      try_move(dx, dy)
    end

    def try_move(dx, dy)
      if @house.move?(self, dx, dy)
	move(dx, dy)
      end
    end
  end

  class Game

    def initialize(widget, lang = nil)
      init_label_dic(lang)
      house = House.new(widget)
      she = Chip.new(house, label_of(:Musume), 1, 0, 2, 2)
      father = Chip.new(house, label_of(:Papa), 0, 0, 1, 2)
      mother = Chip.new(house, label_of(:Mama), 3, 0, 1, 2)
      kozou = []
      (0..3).each do |i|
	kozou.push Chip.new(house, label_of(:Kozou), i, 2)
      end
      genan = []
      (0..1).each do |i|
	genan.push Chip.new(house, label_of(:Genan), i * 3, 3, 1, 2)
      end
      bantou = Chip.new(house, label_of(:Bantou), 1, 3, 2, 1)
    end

    private

    def init_label_dic(lang)
      @labeldic = 
	if lang == 'ja' then
	  { :Musume => "娘", :Papa => "父親", :Mama => "母親",
	  :Kozou => "小僧", :Genan => "下男", :Bantou => "番頭" }
	else
	  { :Musume => "Daughter", :Papa => "Father", :Mama => "Mother",
	  :Kozou => "Kozou", :Genan => "Genan", :Bantou => "Clerk" }
	end
    end

    def label_of(key)
      name = @labeldic[key]
      name = key.to_s unless name
      return name
    end

  end
end
