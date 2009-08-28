def out(s)
  puts s
end

out <<EOS
<plist version="1.0">
<array>
EOS

Dir.glob('gems/**/*.gem').each do |x|
    bn = File.basename(x, '.gem')
    gem_name, gem_version = bn.split(/-(\d+\.\d+\.\d+)/)
    unless gem_version
      gem_name, gem_version = bn.split(/-(\d+\.\d+)/)
    end
    gem_sha1 = `openssl sha1 #{x}`.scan(/=\s(.*)$/)[0][0]
    gem_homepage = `cd gems && (gem specification #{gem_name} | egrep "^homepage")`.scan(/homepage: (.*)$/)[0][0]
    gem_import_date = `svn info #{x} | egrep "^Last Changed Date"`.strip
    raise "#{x} doesn't have an SVN last changed date" if gem_import_date.empty?
    gem_import_date = gem_import_date.scan(/Last Changed Date: (\d\d\d\d-\d\d-\d\d)/)[0][0]
    out <<EOS
    <dict>
        <key>OpenSourceProject</key>
        <string>#{gem_name}</string>
        <key>OpenSourceVersion</key>
        <string>#{gem_version}</string>
        <key>OpenSourceWebsiteURL</key>
        <string>#{gem_homepage}</string>
        <key>OpenSourceSHA1</key>
        <string>#{gem_sha1}</string>
        <key>OpenSourceImportDate</key>
        <string>#{gem_import_date}</string>
        <key>OpenSourceLicenseFile</key>
        <string>#{gem_name}.txt</string>
        <key>OpenSourceModifications</key>
        <array>
        </array>
    </dict>
EOS
end

out "</array>"
