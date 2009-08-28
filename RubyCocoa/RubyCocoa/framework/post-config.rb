# create osx_ruby.h and osx_intern.h
# avoid `ID' and `T_DATA' confict headers between Cocoa and Ruby.
new_filename_prefix = 'osx_'
ruby_h = File.join(@config['ruby-header-dir'], 'ruby.h')
intern_h = File.join(@config['ruby-header-dir'], 'intern.h')
build_universal = (@config['build-universal'] == 'yes')
[ ruby_h, intern_h ].each do |src_path|
  dst_fname = new_filename_prefix + File.basename(src_path)
  dst_fname = "src/objc/" + dst_fname
  $stderr.puts "create #{File.expand_path(dst_fname)} ..."
  File.open(dst_fname, 'w') do |dstfile|
    IO.foreach(src_path) do |line|
      unless @config['macosx-deployment-target'].to_f > 10.5
        line.gsub!( /\b(ID|T_DATA)\b/, 'RB_\1' )
        line.gsub!( /\bintern\.h\b/, "#{new_filename_prefix}intern.h" )
      end
      dstfile.puts( line )
    end
  end
end

if @config['gen-bridge-support'] != 'no'
  # generate bridge support metadata files
  out_dir = File.join(Dir.pwd, 'bridge-support')
  sdkroot = @config['sdkroot']
  cflags = build_universal ? "-arch ppc -arch i386 -isysroot #{sdkroot}" : ''
  Dir.chdir('../misc/bridgesupport') do
    command("BSROOT=\"#{out_dir}\" CFLAGS=\"#{cflags}\" #{@config['ruby-prog']} build.rb")
  end
end
