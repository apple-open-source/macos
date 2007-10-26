# Build the libffi.a library if needed.
h = [
  '/usr/lib/libffi.a', 
  '/usr/lib/libffi.dylib', 
  '/usr/local/lib/libffi.a', 
  '/usr/local/lib/libffi.dylib'
]
unless h.any? { |p| File.exist?(p) }
    Dir.chdir('./misc/libffi') { command('make -f Makefile.rubycocoa') }
end
