#
#  RubyRocks.rb
#
#  Created by Tim Burks on 2/18/06.
#  Copyright (c) 2006 Neon Design Technology, Inc. Some rights reserved.
#  This sample code is licensed the same license as RubyCocoa.
#  Find more information about this file online at http://www.rubycocoa.com/ruby-rocks
#
NUMBER_OF_ROCKS = 10
MISSILE_SPEED   = 10
MISSILE_LIFE    = 50
TURN_ANGLE      = 0.2
ACCELERATION    = 1

KEY_SPACE    = 49
KEY_LEFT_ARROW  = 123
KEY_RIGHT_ARROW = 124
KEY_DOWN_ARROW  = 125
KEY_UP_ARROW    = 126
KEY_P           = 35

class GameView < OSX::NSView
  ib_outlets :game

  def awakeFromNib
    window.setOpaque(false)
    @game.bounds = bounds
    @timer = OSX::NSTimer.scheduledTimerWithTimeInterval_target_selector_userInfo_repeats_(1.0/60.0, self, :tick, nil, true)    
  end

  def drawRect(rect)
    @game.draw
  end

  def acceptsFirstResponder
    true
  end

  def keyDown(event)
    @game.keyDown(event.keyCode)
  end

  def keyUp(event)
    @game.keyUp(event.keyCode)
  end

  def tick(timer=nil)
    @game.tick(timer)
    setNeedsDisplay(true)
  end
end

class Game < OSX::NSDocument
  attr_accessor :bounds

  def windowNibName
    return "Game" 
  end

  def awakeFromNib
    @paused = false
    @ship = Ship.alloc.initWithPosition_(OSX::NSPoint.new(@bounds.width/2, @bounds.height/2))
    @rocks = []
    NUMBER_OF_ROCKS.times {@rocks << Rock.alloc.initWithPosition_(OSX::NSPoint.new(rand(@bounds.width), rand(@bounds.height)))}
    @rocks.delete_if{|rock| rock.collidesWith_?(@ship)}
    @missiles = []
    @sounds = {
        :shipDestroyed => OSX::NSSound.soundNamed(:Submarine),
        :rockDestroyed => OSX::NSSound.soundNamed(:Bottle),
        :shoot => OSX::NSSound.soundNamed(:Pop)
    }
  end

  def draw
    OSX::NSColor.blackColor.colorWithAlphaComponent(0.9).set
    OSX::NSRectFill(@bounds)
    @rocks.each {|rock| rock.draw}
    @ship.draw if @ship
    @missiles.each {|missile| missile.draw}
  end

  def tick(timer)
    return if @paused
    @rocks.each {|rock| rock.moveWithBounds_(@bounds)}
    @ship.moveWithBounds_(@bounds) if @ship
    @missiles.each {|missile| missile.moveWithBounds_(@bounds)}
    @rocks.each {|rock|
        @missiles.each {|missile| 
             if missile.collidesWith_?(rock)
             missile.ttl = rock.ttl = 0 
             @sounds[:rockDestroyed].play
         end
        }
        if @ship and @ship.collidesWith_?(rock)
            @ship.ttl = rock.ttl = 0 
            @sounds[:shipDestroyed].play
        end
    }

    @ship = nil if @ship and @ship.ttl == 0
    @rocks.delete_if {|rock| rock.ttl == 0}
    @missiles.delete_if {|missile| missile.ttl == 0}
  end

  def keyDown(code)
    case code
    when KEY_SPACE:
        if @ship
        @missiles << @ship.shoot 
        @sounds[:shoot].play
    end
    when KEY_LEFT_ARROW:
        @ship.angle = TURN_ANGLE if @ship
    when KEY_RIGHT_ARROW:
        @ship.angle = -TURN_ANGLE if @ship
    when KEY_UP_ARROW:
        @ship.acceleration = ACCELERATION if @ship
    when KEY_DOWN_ARROW:
        @ship.acceleration = -ACCELERATION if @ship
    when KEY_P:
        @paused = ! @paused
    end
  end

  def keyUp(code)
    case code
    when KEY_LEFT_ARROW, KEY_RIGHT_ARROW:
        @ship.angle = 0 if @ship    
    when KEY_UP_ARROW, KEY_DOWN_ARROW:
        @ship.acceleration = 0 if @ship    
    end
  end
end

class Sprite < OSX::NSObject
  attr_accessor :position, :velocity, :radius, :color, :ttl

  def initWithPosition_(position)
    @position = position
    @velocity = OSX::NSPoint.new(0, 0)
    @ttl = -1
    self
  end

  def moveWithBounds_(bounds)
    @ttl -= 1 if @ttl > 0
    @position.x += @velocity.x
    @position.y += @velocity.y    
    @position.x = bounds.width if @position.x < 0
    @position.x = 0 if @position.x > bounds.width
    @position.y = bounds.height if @position.y < 0
    @position.y = 0 if @position.y > bounds.height
  end

  def collidesWith_?(sprite)
    dx = @position.x - sprite.position.x
    dy = @position.y - sprite.position.y
    r = @radius + sprite.radius
    return false if dx > r or -dx > r or dy > r or -dy > r
    dx*dx + dy*dy < r*r
  end
end

class Rock < Sprite
  def initWithPosition_(position)
    super(position)
    @velocity = OSX::NSPoint.new(rand-0.5,rand-0.5)
    @color = OSX::NSColor.whiteColor
    @radius = 30
    self
  end

  def draw
    @color.set
    OSX::NSBezierPath.bezierPathWithOvalInRect(OSX::NSRect.new(@position.x-radius, @position.y-@radius, 2*@radius, 2*@radius)).stroke
  end
end

class Ship < Sprite
  attr_accessor :direction, :angle, :acceleration

  def initWithPosition_(position)
    super(position)
    @radius = 10
    @color = OSX::NSColor.redColor
    @direction = OSX::NSPoint.new(0, 1)
    @angle = @acceleration = 0
    self
  end

  def moveWithBounds_(bounds)
    super(bounds)
    if @angle != 0
        cosA = Math::cos(@angle)
        sinA = Math::sin(@angle)
        x = @direction.x * cosA - @direction.y * sinA
        y = @direction.y * cosA + @direction.x * sinA
        @direction.x, @direction.y = x, y
    end
    if @acceleration != 0
        @velocity.x += @acceleration * @direction.x
        @velocity.y += @acceleration * @direction.y
    end
  end

  def draw
    @color.set
    x0,y0 = @position.x, @position.y
    x, y  = @direction.x, @direction.y
    r = @radius
    path = OSX::NSBezierPath.bezierPath
    path.moveToPoint(OSX::NSPoint.new(x0 + r*x, y0 + r*y))
    path.lineToPoint(OSX::NSPoint.new(x0 + r * (-x +y), y0 + r * (-x -y)))
    path.lineToPoint(OSX::NSPoint.new(x0, y0))
    path.lineToPoint(OSX::NSPoint.new(x0 + r * (-x -y), y0 + r * (+x -y)))
    path.fill  
  end

  def shoot
    missilePosition = OSX::NSPoint.new(position.x+direction.x, position.y+direction.y)
    missileVelocity = OSX::NSPoint.new(MISSILE_SPEED * direction.x + velocity.x, MISSILE_SPEED * direction.y + velocity.y)
    return Missile.alloc.initWithPosition_velocity_color_(missilePosition, missileVelocity, @color)
  end
end

class Missile < Sprite
  def initWithPosition_velocity_color_(position, velocity, color)
    initWithPosition_(position)
    @velocity = velocity
    @color = color
    @radius = 3
    @ttl = MISSILE_LIFE
    self
  end

  def draw
    @color.set
    OSX::NSBezierPath.bezierPathWithOvalInRect(OSX::NSRect.new(@position.x-radius, @position.y-@radius, 2*@radius, 2*@radius)).fill
  end
end
