install_root = @config['install-root']

# If required, backup files create here.
backup_dir = '/tmp/rubycocoa_backup'

# Install RubyCocoa.framework
frameworks_dir = File.expand_path("#{install_root}#{@config['frameworks']}")
framework_name = "RubyCocoa.framework"
framework_path = "#{frameworks_dir}/#{framework_name}"

if FileTest.exist?( framework_path ) then
  command "rm -rf '#{backup_dir}/#{framework_name}'"
  command "mkdir -p '#{backup_dir}'"
  command "mv '#{framework_path}' '#{backup_dir}/'"
end
command "mkdir -p '#{frameworks_dir}'"
command "cp -R '#{framework_obj_path}/#{framework_name}' '#{framework_path}'"
