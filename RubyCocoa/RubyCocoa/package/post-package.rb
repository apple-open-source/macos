# $Id: post-package.rb 2250 2009-09-15 15:33:54Z kimuraw $

work_dir = File.expand_path('work')
contents_dir = File.join(work_dir, 'files')
resources_dir = File.join(work_dir, 'resources')

system "find '#{contents_dir}' -name .svn -exec rm -rf {} \\; >& /dev/null"

package_name = "RubyCocoa-#{@config['rubycocoa-version']}-OSX#{@config['macosx-deployment-target']}"
dmg_dir = File.join(work_dir, package_name)
Dir.mkdir dmg_dir

pkgmaker = '/Developer/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker'

command "sudo chown -R root:admin \"#{contents_dir}\""

begin
  str = %Q!"#{pkgmaker}" -build ! +
	%Q!-p "#{File.join(dmg_dir, package_name)}.pkg" ! +
	%Q!-f "#{contents_dir}" -r "#{resources_dir}" ! +
	%Q!-i "#{File.join(work_dir, 'Info.plist')}" ! +
	%Q!-d "#{File.join(work_dir, 'Description.plist')}" !
  system str
  stat = $?.to_i / 256 
  if stat != 0 and stat != 1 # PackageMaker of 10.4 returns 1 (bug)
    raise RuntimeError, "'system #{str}' failed(#{stat})"
  end
ensure
  # revert owner 
  command "sudo chown -R `/usr/bin/id -un` \"#{contents_dir}\""
end

save_dir = Dir.pwd
Dir.chdir dmg_dir
resources = File.join(package_name + '.pkg', 'Contents', 'Resources')
File.symlink(File.join(resources, 'License.txt'), 'License.txt')
File.symlink(File.join(resources, 'English.lproj', 'ReadMe.html'), 'ReadMe.html')
File.symlink(File.join(resources, 'Japanese.lproj', 'ReadMe.html'), 'ReadMe.ja.html')
Dir.chdir save_dir

command %Q!/usr/bin/hdiutil create ! +
	%Q!-srcfolder "#{dmg_dir}" ! +
	%Q!-format UDZO -tgtimagekey zlib-level=9 ! +
	%Q!-fs HFS+ -volname "#{package_name}" ! +
	%Q!"#{File.join(work_dir, package_name)}.dmg" !

