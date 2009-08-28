require 'rbconfig'

# Build the libffi.a library if needed.
h = [
  '/usr/lib/libffi.a', 
  '/usr/lib/libffi.dylib', 
  '/usr/local/lib/libffi.a', 
  '/usr/local/lib/libffi.dylib'
]
if @config['macosx-deployment-target'].to_f < 10.5
  Dir.chdir('./misc/libffi') { command(%q[make -f Makefile.rubycocoa SYSROOT='-isysroot /Developer/SDKs/MacOSX10.4u.sdk']) }
elsif not h.any? { |p| File.exist?(p)}
  Dir.chdir('./misc/libffi') { command('make -f Makefile.rubycocoa') }
end
