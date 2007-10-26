require 'osx/cocoa'

class Shape

  attr_reader :rect, :color

  def initialize(x, y, w, h, c)
    @rect = OSX::NSRect.new(x, y, w, h)
    @color = c
  end

  def hpos() @rect.origin.x end
  def vpos() @rect.origin.y end
  def width() @rect.size.width end
  def height() @rect.size.height end

  def hpos=(val) @rect.origin.x = val end
  def vpos=(val) @rect.origin.y = val end
  def width=(val) @rect.size.width = val end
  def height=(val) @rect.size.height = val end

end


class Paddle < Shape

  WIDTH = 30.0
  HEIGHT = 4.0
  COLOR = OSX::NSColor.darkGrayColor

  def initialize
    super(0.0, 0.0, WIDTH, HEIGHT, COLOR)
  end

end


class Ball < Shape

  RADIUS = 6.0

  def initialize(c)
    super(0.0, 0.0, RADIUS, RADIUS, c)
  end

end

class BallMoving

  attr_writer :area
  attr_accessor :died
  attr_reader :speed

  def initialize(ball, paddle)
    @died = true
    @ball = ball
    @paddle = paddle
    @area = nil
    @speed = 0
    @angle = 90
    recalc
    snd_init
  end

  def set_speed(new_speed)
    @speed = new_speed
    recalc
  end

  def speed=(new_speed)
    set_speed(new_speed)
  end

  def angle=(angle)
    @angle = angle
    recalc
  end

  def bound_vertical
    @angle *= -1
    @dv *= -1
  end

  def bound_horizontal
    @angle = 180 - @angle
    @dh *= -1
  end

  def ball_in_paddle?
    val = @ball.vpos
    range = (@dv >= 0) ? val.to_i..(val+@dv).to_i : (val+@dv).to_i..val.to_i
    r0 = @paddle.vpos
    r1 = r0 + @paddle.height
    return false unless range.find{|val| r0 <= val && val <= r1 }

    val = @ball.hpos
    r0 = @paddle.hpos
    r1 = r0 + @paddle.width
    return false if val < r0
    return false if val > r1

    true
  end

  def next
    return if @died

    if @dv < 0 && ball_in_paddle? then
      bound_vertical
      set_speed(@speed + 1)
      paddle_beep
    end
    
    @ball.rect.origin.y = @ball.rect.origin.y + @dv
    if @ball.rect.origin.y >= @area.height then
      @ball.rect.origin.y = @area.height - 1
      bound_vertical
      wall_beep
    elsif @ball.rect.origin.y <= 0 then
      @ball.rect.origin.y = 0
      bound_vertical
      loss_beep
      @died = true
    end

    @ball.rect.origin.x = @ball.rect.origin.x + @dh
    if @ball.rect.origin.x >= @area.width then
      @ball.rect.origin.x = @area.width - 1
      bound_horizontal
      wall_beep
    elsif @ball.rect.origin.x <= 0 then
      @ball.rect.origin.x = 0
      bound_horizontal
      wall_beep
    end

  end

  def recalc
    rad = radian(@angle)
    @dh = @speed * Math.cos(rad)
    @dv = @speed * Math.sin(rad)
  end

  def radian(angle)
    angle.to_f * Math::PI / 180.0
  end

  def wall_beep
    Thread.start { @wall_snd.play }
  end

  def paddle_beep
    Thread.start { @paddle_snd.play }
  end

  def loss_beep
    Thread.start { @loss_snd.play }
  end

  def snd_init
    require 'rbconfig'
    if `uname -r`.to_f >= 6.0 then
      @wall_snd = OSX::NSSound.soundNamed "Pop"
    else
      @wall_snd = OSX::NSSound.soundNamed "Bonk"
    end
    @wall_snd.play ; @wall_snd.stop	# for pre-load
    @paddle_snd = OSX::NSSound.soundNamed "Ping"
    @paddle_snd.play ; @paddle_snd.stop	# for pre-load
    @loss_snd = OSX::NSSound.soundNamed "Basso"
    @loss_snd.play ; @loss_snd.stop	# for pre-load
  end

end
