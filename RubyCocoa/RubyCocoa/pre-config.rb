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

if @config['build-universal'] == 'yes' && @config['sdkroot'].size == 0
  # SDKROOT is required to build a universal-binary on 10.4 
  @config['sdkroot'] = 
    case @config['macosx-deployment-target'].to_f 
    when 10.4
      sdkroot = '/Developer/SDKs/MacOSX10.4u.sdk'
    else
      '' # not needed for 10.5
    end
end

# build options
cflags = '-fno-common -g -fobjc-exceptions'
ldflags = '-undefined suppress -flat_namespace'
sdkroot = @config['sdkroot'] 

if @config['build-universal'] == 'yes'
  cflags << ' -arch ppc -arch i386'
  ldflags << ' -arch ppc -arch i386'

  if @config['macosx-deployment-target'].to_f < 10.5
    cflags << ' -isysroot ' << sdkroot
    ldflags << ' -Wl,-syslibroot,' << sdkroot

    # validation
    raise "ERROR: SDK \"#{sdkroot}\" does not exist." unless File.exist?(sdkroot)
    libruby_sdk = @config['libruby-path']
    raise "ERROR: library \"#{libruby_sdk}\" does not exist." unless File.exist?(libruby_sdk)
  else
    cflags << ' -arch ppc64 -arch x86_64'
    ldflags << ' -arch ppc64 -arch x86_64'
  end
end

def lib_exist?(path, sdkoot=@config['sdkroot'])
  File.exist?(File.join(sdkoot, path))
end

if lib_exist?('/usr/include/libxml2') and lib_exist?('/usr/lib/libxml2.dylib')
  cflags << ' -I/usr/include/libxml2 -DHAS_LIBXML2 '
  ldflags << ' -lxml2 '
else
  puts "libxml2 is not available!"
end

raise 'ERROR: ruby must be built as a shared library' if Config::CONFIG["ENABLE_SHARED"] != 'yes'

# Add the libffi library to the build process.
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
cflags << ' -DMACOSX '
ldflags << ' -lffi '

config_ary << [ :other_cflags, cflags ]
config_ary << [ :other_ldflags, ldflags ]

target_files.each do |dst_name|
  src_name = dst_name + '.in'
  data = File.open(src_name) {|f| f.read }
  config_ary.each do |sym, str|
    data.gsub!( "%%%#{sym}%%%", str )
  end
  File.open(dst_name,"w") {|f| f.write(data) }
  $stderr.puts "create #{dst_name}"
end
