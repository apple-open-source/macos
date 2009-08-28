require 'fileutils'

def cmd(cmd)
  $stderr.puts cmd
  `#{cmd}`
end

outdir = '/tmp/gems'
if File.exist?(outdir)
  $stderr.puts "WARNING: #{outdir} already exists"
end
FileUtils.mkdir_p(outdir)

current_gems = {}
Dir.glob('gems/**/*.gem').each do |g|
  r = File.basename(g).match(/\-([\d.]+)\.gem/)
  name = r.pre_match  
  vers = r[1]
  (current_gems[name] ||= []) << [vers, g]
end

Dir.chdir(outdir) do
  current_gems.each do |name, versions|
    versions.each do |vers, _|
      cmd("gem fetch #{name} --version '< #{vers[0].chr.succ}'")
    end
  end
end

Dir.glob(File.join(outdir, '**', '*.gem')) do |g|
  r = File.basename(g).match(/\-([\d.]+)\.gem/)
  name = r.pre_match  
  vers = r[1]
  versions = current_gems[name]
  if versions.nil?
    $stderr.puts "Looks like `#{name}' (#{g}) is not a current gem, skip..."
    next
  end
  a = versions.find { |vers2, _| vers2 != vers and vers2[0] == vers[0] }
  if a.nil?
    $stderr.puts "Could not find a candidate to upgrade to #{g} in #{versions}, skip..."
    next
  end
  if a[1] == g
    $stderr.puts "#{name} #{vers} untouched, skip..."
    next
  end
  FileUtils.cp(g, File.dirname(a[1]))
  cmd("svn rm #{a[1]}")
  cmd("svn add #{File.join(File.dirname(a[1]), File.basename(g))}")
end
