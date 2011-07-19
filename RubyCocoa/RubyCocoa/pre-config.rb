# create a real project.pbxproj file by applying libruby
# configuration.

require 'rbconfig'

target_files = %w[
  ext/rubycocoa/extconf.rb
  framework/GeneratedConfig.xcconfig
  framework/src/objc/Version.h
  tests/Makefile
]

install_path = @config['build-as-embeddable'] == 'yes' \
  ? "@executable_path/../Frameworks" \
  : @config['frameworks'].sub((ENV['DSTROOT'] or ''), '')

config_ary = [
  [ :frameworks,      @config['frameworks'] ],
  [ :ruby_header_dir, @config['ruby-header-dir'] ],
  [ :libruby_path,    @config['libruby-path'] ],
  [ :libruby_path_dirname,  File.dirname(@config['libruby-path']) ],
  [ :libruby_path_basename, File.basename(@config['libruby-path']) ],
  [ :rubycocoa_version,      @config['rubycocoa-version'] ],
  [ :rubycocoa_version_short,   @config['rubycocoa-version-short'] ],
  [ :rubycocoa_release_date, @config['rubycocoa-release-date'] ],
  [ :rubycocoa_svn_revision,  @config['rubycocoa-svn-revision'] ],
  [ :rubycocoa_framework_version,  @config['rubycocoa-framework-version'] ],
  [ :macosx_deployment_target, @config['macosx-deployment-target'] ],
  [ :build_dir, framework_obj_path ],
  [ :install_path, install_path ]
]

# build options
cflags = '-fno-common -g -fobjc-exceptions -Wall'
ldflags = '-undefined suppress -flat_namespace'
sdkroot = @config['sdkroot']
archs = @config['target-archs']

# add archs if given
arch_flags = archs.gsub(/\A|\s+/, ' -arch ')

if sdkroot.size > 0
  cflags << ' -isysroot ' << sdkroot
  ldflags << ' -Wl,-syslibroot,' << sdkroot
end

cflags << ' -DRB_ID=ID'  if @config['macosx-deployment-target'].to_f > 10.5

def lib_exist?(path, sdkroot=@config['sdkroot'])
  File.exist?(File.join(sdkroot, path))
end

if lib_exist?('/usr/include/libxml2') and lib_exist?('/usr/lib/libxml2.dylib')
  cflags << ' -I/usr/include/libxml2 -DHAS_LIBXML2 '
  ldflags << ' -lxml2 '
else
  puts "libxml2 is not available!"
end

raise 'ERROR: ruby must be built as a shared library' if Config::CONFIG["ENABLE_SHARED"] != 'yes'

# Add the libffi library to the build process.
if @config['macosx-deployment-target'].to_f < 10.5
  cflags << ' -I../../misc/libffi/include -I../misc/libffi/include ' 
  ldflags << ' -L../../misc/libffi -L../misc/libffi '
else
  if !lib_exist?('/usr/lib/libffi.a') and !lib_exist?('/usr/lib/libffi.dylib')
    if lib_exist?('/usr/local/lib/libffi.a') and lib_exist?('/usr/local/include/ffi')
      cflags << ' -I/usr/local/include/ffi '
      ldflags << ' -L/usr/local/lib '
    else
      cflags << ' -I../../misc/libffi/include -I../misc/libffi/include ' 
      ldflags << ' -L../../misc/libffi -L../misc/libffi '
    end
  else
    cflags << ' -I/usr/include/ffi '
  end
end
cflags << ' -DMACOSX '
ldflags << ' -lffi '

config_ary << [ :other_cflags, cflags ]
config_ary << [ :other_ldflags, ldflags ]
config_ary << [ :target_archs, archs.size > 0 ? archs : '$NATIVE_ARCH' ]
config_ary << [ :arch_flags, archs.size > 0 ? arch_flags : '' ]

target_files.each do |dst_name|
  src_name = dst_name + '.in'
  data = File.open(src_name) {|f| f.read }
  config_ary.each do |sym, str|
    data.gsub!( "%%%#{sym}%%%", str )
  end
  File.open(dst_name,"w") {|f| f.write(data) }
  $stderr.puts "create #{dst_name}"
end
