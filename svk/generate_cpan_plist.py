#!/usr/bin/python

import os, re, sha, stat, time, urllib, xml.sax.saxutils

def generate_dict(name, version, tarball):
	regex = re.compile('^    <td><small>&nbsp;\[<a href=\"/CPAN/(.*)/%s\">Download</a>\]$' % tarball)
	found_license = -1

	# Handle CPAN's new "UNAUTHORIZED RELEASE" feature.
	if name in [ 'Algorithm-Diff', 'TermReadKey' ]:
		url = 'http://search.cpan.org/dist/%s-%s/' % (name, version)
	else:
		url = 'http://search.cpan.org/dist/%s/' % name

	f = urllib.urlopen(url)
	for line in f.readlines():
		# match download path
		m = regex.match(line)
		if m:
			path = m.group(1)
		# match license
		if found_license >= 0:
			found_license = found_license + 1
		if found_license == 2:
			license = line.strip()
			found_license = -1
		if line == '    <td class=label>License</td>\n':
			found_license = 0
	f.close()

	filename = 'modules/%s/%s' % (name, tarball)

	# Calculate checksum.
	h = sha.new()
	f = file(filename)
	h.update(f.read())
	f.close()
	checksum = h.hexdigest()

	# Determine import date.
	svn_info_command = os.popen('svn info %s | grep \'^Last Changed Date: \' | awk \'{ print $4 }\'' % filename)
	import_date = svn_info_command.read().strip()
	svn_info_command.close()

	# Output dictionary.
	print '\t<dict>'
	print '\t\t<key>OpenSourceProject</key>'
	print '\t\t<string>%s</string>' % name
	print '\t\t<key>OpenSourceVersion</key>'
	print '\t\t<string>%s</string>' % version
	print '\t\t<key>OpenSourceWebsiteURL</key>'
	print '\t\t<string>%s</string>' % url
	print '\t\t<key>OpenSourceURL</key>'
	print '\t\t<string>http://search.cpan.org/CPAN/%s/%s</string>' % (path, tarball)
	print '\t\t<key>OpenSourceSHA1</key>'
	print '\t\t<string>%s</string>' % checksum
	print '\t\t<key>OpenSourceImportDate</key>'
	print '\t\t<string>%s</string>' % import_date
	print '\t\t<key>OpenSourceLicense</key>'
	print '\t\t<string>%s</string>' % xml.sax.saxutils.escape(license)
	print '\t\t<key>OpenSourceLicenseFile</key>'
	print '\t\t<string>svk.txt</string>'
	print '\t</dict>'

# Grab the raw module list.
make_command = os.popen('make -C modules plist')
modules = map(lambda x: x.strip(), make_command.readlines())
make_command.close()

# Split and sort the raw list.
module_list = []
for idx in range(0, len(modules), 3):
	module_list.append(modules[idx:idx+3])
module_list.sort(key=lambda x: x[0].lower())

# Output property list header.
print '<?xml version="1.0" encoding="UTF-8"?>'
print '<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">'
print '<plist version="1.0">'
print '<array>'

# Output dictionary for each module.
map(lambda x: generate_dict(x[0], x[1], x[2]), module_list)

# Output property list footer.
print '</array>'
print '</plist>'
