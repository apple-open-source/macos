# Make the computer speak, using the SpeechSynthesis framework.
require 'osx/foundation'

path = '/System/Library/Frameworks/ApplicationServices.framework/Frameworks/SpeechSynthesis.framework'

unless File.exist?(File.join(path, 'Resources/BridgeSupport/SpeechSynthesis.bridgesupport'))
  $stderr.puts "SpeechSynthesis.framework not supported"
  exit 1
end

OSX.require_framework(path)

error, channel =  OSX.NewSpeechChannel(nil)
if error != 0
  $stderr.puts "Can't create new speech channel (error #{error})"
  exit 1
end

unless OSX.SpeakText(channel, 'hello Ruby!') == 0
  $stderr.puts "Can't speak text (error #{error})"
  exit 1
end
sleep 2 # sleeping enough time the computer talks
