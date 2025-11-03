require 'webrick'

minver = Gem::Version.new('1.9.1')

gem = Gem.loaded_specs['webrick']
version = gem.version
path = gem.full_gem_path

STDERR.puts "found v#{version} at #{path}"

if version < minver
	STDERR.puts "error: version #{version} is less than expected #{minver}"
	exit 1
end

tserv = WEBrick::HTTPServer.new(Port: 8675)

# Our test will curl / and look for "PASS"
tserv.mount_proc '/' do |req, res|
	res.body = 'PASS'
	Thread.new {
		tserv.shutdown
	}
end

tserv.start
