# $Id: pre-package.rb 2250 2009-09-15 15:33:54Z kimuraw $

require 'erb'
require 'fileutils'

work_dir = 'work'
contents_dir = File.join(work_dir, 'files')
resources_dir = File.join(work_dir, 'resources')

def erb(src, dest, bind)
  str = ERB.new(File.new(src).read).result(bind)
  open(dest, 'w') {|f| f.write str}
end

FileUtils.rm_rf work_dir
Dir.mkdir work_dir

# .plist
erb('tmpl/Info.plist', File.join(work_dir, 'Info.plist'), binding)
erb('tmpl/Description.plist', File.join(work_dir, 'Description.plist'), binding)

# Resources
Dir.mkdir resources_dir
Dir.mkdir File.join(resources_dir, 'English.lproj')
Dir.mkdir File.join(resources_dir, 'Japanese.lproj')

File.link '../COPYING', File.join(resources_dir, 'License.txt')
File.link '../ReadMe.html', 
          File.join(resources_dir, 'English.lproj', 'ReadMe.html')
File.link '../ReadMe.ja.html', 
          File.join(resources_dir, 'Japanese.lproj', 'ReadMe.html')

File.link('tmpl/background.gif', File.join(resources_dir, 'background.gif'))

# Contents
Dir.mkdir contents_dir

# Postflight and post-constent stuff
if @config['macosx-deployment-target'].to_f < 10.5
  postflight = File.join(resources_dir, 'postflight')
  erb('tmpl/postflight-universal.rb', postflight, binding)
  File.chmod(0755, postflight)
  usr_lib_dir = File.join(contents_dir, 'usr/lib')
  FileUtils.mkdir_p usr_lib_dir
  unless system("tar -xzf ../misc/libruby.1.dylib-tiger.tar.gz -C /tmp")
    raise "error when decompressing libruby" 
  end
  FileUtils.mv '/tmp/libruby.1.dylib', File.join(usr_lib_dir, 'libruby-with-thread-hooks.1.dylib')
end
