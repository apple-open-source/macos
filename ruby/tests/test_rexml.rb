require 'rexml/document'

minver = Gem::Version.new('3.4.2')

gem = Gem.loaded_specs['rexml']
version = gem.version
path = gem.full_gem_path

STDERR.puts "found v#{version} at #{path}"

if version < minver
	STDERR.puts "error: version #{version} is less than expected #{minver}"
	exit 1
end

plistf = File.new('ruby.plist')
plist = REXML::Document.new(plistf)
root = plist.root

name = root.name
if name != 'plist'
	STDERR.puts "error: expected test plist to have a root <plist> node, found <#{name}> instead"
	exit 1
end

puts "found <#{name}> root node, OK"
