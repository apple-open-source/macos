# This script watches modifications on the given directory, using the new
# FSEvents API in Leopard.

require 'osx/foundation'
OSX.require_framework '/System/Library/Frameworks/CoreServices.framework/Frameworks/CarbonCore.framework'
include OSX

def die(s)
  $stderr.puts s
  exit 1
end

die "Usage: #{__FILE__} <path>" unless ARGV.size == 1

fsevents_cb = proc do |stream, ctx, numEvents, paths, marks, eventIDs|
  paths.regard_as('*')
  numEvents.times do |n|
    puts "Event on path `#{paths[n]}' ID `#{eventIDs[n]}'"
  end
end

path = ARGV.first
stream = FSEventStreamCreate(
  KCFAllocatorDefault,
  fsevents_cb,
  nil,
  [path],
  KFSEventStreamEventIdSinceNow,
  1.0,
  0)

die "Failed to create the FSEventStream" unless stream

FSEventStreamScheduleWithRunLoop(
  stream,
  CFRunLoopGetCurrent(),
  KCFRunLoopDefaultMode)

ok = FSEventStreamStart(stream)
die "Failed to start the FSEventStream" unless ok

begin
  CFRunLoopRun()
rescue Interrupt
  FSEventStreamStop(stream)
  FSEventStreamInvalidate(stream)
  FSEventStreamRelease(stream)
end
