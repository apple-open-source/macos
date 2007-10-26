require 'osx/cocoa'
require 'thread'

class AppController < OSX::NSObject

  ib_outlets :msgField

  def awakeFromNib
    @mutex = Mutex.new
    @thread = start_thread
  end

  def btnClicked (sender)
    return nil unless @thread
    @thread.kill
    @thread = nil
    Thread.start do
      sender.setEnabled(false)
      hello_sequence
      sender.setEnabled(true)
      @thread = start_thread
    end
  end
  ib_action :btnClicked

  def windowShouldClose (sender)
    quit
    true
  end

  def start_thread
    Thread.start {
      loop do
	set_msg(Time.now.to_s)
	sleep 0.5
      end
    }
  end

  def set_msg (str)
    @mutex.synchronize do
      @msgField.setStringValue(str)
    end
  end

  def quit
    OSX::NSApp.stop(self)
  end

  def hello_sequence
    [
      [ "Ruby",  1 ], [ "and",   1 ], [ "Cocoa", 1 ],
      [ "",      1 ], 
      [ "Hello RubyCocoa !", 0.3 ], [ "", 0.6 ],
      [ "Hello RubyCocoa !", 0.3 ], [ "", 0.6 ],
      [ "Hello RubyCocoa !", 0.3 ], [ "", 0.6 ],
      [ "Hello RubyCocoa !", 3 ]
    ].each do |msg, interval|
      set_msg(msg)
      sleep(interval)
    end
  end

end
