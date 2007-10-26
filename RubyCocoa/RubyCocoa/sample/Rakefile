# -*- mode:ruby -*-
require 'rake'

BUNDLES = %w( VPRubyPluginEnabler RubyAnywhere )

task :default => :build

task :build => [:build_apps, :build_bundle]
task :clean => [:clean_apps, :clean_bundle]

desc "build application samples into apps"
task :build_apps do
  mkdir_p "apps"
  ruby("buildall.rb", "apps")
end

desc "build bundle/plugin samples"
task :build_bundle do
  BUNDLES.each do |loc|
    chdir(loc) { system "xcodebuild" }
  end
end

task :clean_apps do
  rm_rf "apps"
end

task :clean_bundle do
  BUNDLES.each do |loc|
    chdir(loc) { rm_rf "build" }
  end
end
