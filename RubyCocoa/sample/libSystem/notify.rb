require 'osx/foundation'

SIGUSR1 = 30
status, token = OSX.notify_register_signal('org.rubycocoa.foo', SIGUSR1)

trap('SIGUSR1') { puts 'Received notification.' }

10.times do
  puts 'Sending notification.' 
  OSX.notify_post('org.rubycocoa.foo')
  sleep 1
end
