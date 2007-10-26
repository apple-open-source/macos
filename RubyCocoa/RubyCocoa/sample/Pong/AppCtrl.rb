require 'osx/cocoa'

class AppCtrl < OSX::NSObject

  ib_outlets :playingView, :startBtn

  def awakeFromNib
    paddle_init
    ball_init(@paddle)
  end

  def start
    if @thread == nil then
      @movings.each {|m| m.died = false }
      @startBtn.setTitle "Stop"
      @thread = Thread.start do
	loop do
	  sleep 0.02
	  ary = @movings.select {|m| ! m.died }
	  if ary.size > 0 then
	    ary.each {|m| m.next }
	  else
	    break
	  end
	  redraw
	end
	@thread = nil
	reset
      end
    end
  end

  def reset
    stop
    @playingView.balls.replace []
    ball_init(@paddle)
  end

  def stop
    @thread.kill if @thread
    @thread = nil
    @startBtn.setTitle "Start"
  end

  def redraw
    @playingView.setNeedsDisplay(true)
  end

  def mouseDragged(evt)
    point = @playingView.convertPoint_fromView(evt.locationInWindow, nil)
    @paddle.hpos = point.x
    redraw
  end

  def startBtnClicked(sender)
    if @thread then
      stop
    else
      start
    end
  end
  ib_action :startBtnClicked

  def resetBtnClicked(sender)
    stop
    reset
    redraw
  end
  ib_action :resetBtnClicked

  def windowShouldClose(sender)
    OSX::NSApp.stop(self)
    true
  end
  ib_action :windowShouldClose

  def ball_init(paddle)
    @movings = Array.new
    [
      [ rand(200), rand(200), 5.0, angle_new, OSX::NSColor.redColor ],
      [ rand(200), rand(200), 4.0, 180 - angle_new, OSX::NSColor.blueColor ]
    ].each do |h,v,s,a,c|
      ball = Ball.new(c)
      ball.hpos = h
      ball.vpos = v
      moving = BallMoving.new(ball, paddle)
      moving.speed = s
      moving.angle = a
      moving.area = @playingView
      @playingView.balls.push(ball)
      @movings.push(moving)
    end
  end

  def paddle_init
    @paddle = Paddle.new
    @paddle.hpos  = 50
    @paddle.vpos = 15
    @playingView.paddle = @paddle
  end

  def angle_new
    20 + rand(60)
  end

end
