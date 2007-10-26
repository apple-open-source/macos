require 'osx/cocoa'

snd_files =
  if ARGV.size == 0 then
    `ls /System/Library/Sounds/*.aiff`.split
  else
    ARGV
  end

OSX.ruby_thread_switcher_start(0.001, 0.1)
Thread.start { OSX::NSRunLoop.currentRunLoop.run }

snd_files.each do |path|
  snd = OSX::NSSound.alloc.initWithContentsOfFile_byReference(path, true)
  snd.play
  sleep 0.25 while snd.isPlaying?
end
