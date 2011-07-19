require 'rbconfig'

# Copy BridgeSupport files if needed
if `sw_vers -productVersion`.to_f >= 10.5 and @config['macosx-deployment-target'].to_f < 10.5
  DEST = 'build/Default/RubyCocoa.framework/Resources/BridgeSupport'
  SOURCE = '../misc/bridge-support-tiger.tar.gz'
  command "rm -rf #{DEST}"
  command "mkdir #{DEST}"
  command "tar -xzf #{SOURCE} -C #{DEST}"
end

# Xcode generates unwanted symlink "Headers" under Headers/
unwanted_symlink = 'build/Default/RubyCocoa.framework/Headers/Headers'
File.delete(unwanted_symlink) if File.symlink?(unwanted_symlink)
