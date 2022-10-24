"""
Managing UTD plist for libarchive manually is time consuming. The plist manages more than 600 test cases.
For each test command, the test name and test index must be matched with its position in the test set.
This python script helps test writers generate the plist quickly.
"""

import test_names_parser as parser

utd_plist = """<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>BATSConfigVersion</key>
	<string>0.2.0</string>
	<key>Project</key>
	<string>libarchive</string>
	<key>TestSpecificLogs</key>
        <array>
        <string>BATS_TMP_DIR/libarchive_test.*</string>
		<string>BATS_TMP_DIR/bsdtar_test.*</string>
		<string>BATS_TMP_DIR/bsdcpio_test.*</string>
		<string>/var/log/system.log</string>
        </array>
	<key>RadarComponents</key>
	<dict>
		<key>Name</key>
		<string>libarchive</string>
		<key>Version</key>
		<string>all</string>
	</dict>
	<key>Tests</key>
	<array>
		<dict>
			<key>Command</key>
			<array>
				<string>/bin/sh</string>
				<string>test_tar.sh</string>
			</array>
			<key>TestName</key>
			<string>libarchive.tar</string>
			<key>WhenToRun</key>
			<array>
				<string>PRESUBMISSION</string>
				<string>NIGHTLY</string>
			</array>
			<key>WorkingDirectory</key>
			<string>/AppleInternal/Tests/libarchive</string>
			<key>Timeout</key>
			<integer>3600</integer>
		</dict>
<LIBARCHIVE_TEST_CMD>
<BSDTAR_TEST_CMD>
<BSDCPIO_TEST_CMD>
	</array>
	<key>Timeout</key>
	<integer>3600</integer>
</dict>
</plist>
"""

libarchieve_test_cmd = """		<dict>
			<key>Command</key>
			<array>
            <string>/AppleInternal/Tests/libarchive/libarchive_test</string>
            <string>-i</string>
            <string>TESTINDEX</string>
			</array>
			<key>TestName</key>
			<string>libarchive.libarchive_test.TESTNAME</string>
			<key>WhenToRun</key>
			<array>
				<string>PRESUBMISSION</string>
				<string>NIGHTLY</string>
			</array>
			<key>WorkingDirectory</key>
			<string>/AppleInternal/Tests/libarchive</string>
			<key>Timeout</key>
			<integer>3600</integer>
		</dict>"""

bsdtar_test_cmd = """        <dict>
			<key>Command</key>
			<array>
				<string>/AppleInternal/Tests/libarchive/bsdtar_test</string>
				<string>-p</string>
				<string>/usr/bin/bsdtar</string>
                <string>-i</string>
                <string>TESTINDEX</string>
			</array>
			<key>TestName</key>
			<string>libarchive.bsdtar_test.TESTNAME</string>
			<key>WhenToRun</key>
			<array>
				<string>PRESUBMISSION</string>
				<string>NIGHTLY</string>
			</array>
			<key>WorkingDirectory</key>
			<string>/AppleInternal/Tests/libarchive</string>
			<key>Timeout</key>
			<integer>3600</integer>
		</dict>"""

bsdcpio_test_cmd = """		<dict>
			<key>Command</key>
			<array>
				<string>/AppleInternal/Tests/libarchive/bsdcpio_test</string>
				<string>-p</string>
				<string>/usr/bin/cpio</string>
				<string>-i</string>
				<string>TESTINDEX</string>
			</array>
			<key>TestName</key>
			<string>libarchive.bsdcpio_test.TESTNAME</string>
			<key>WhenToRun</key>
			<array>
				<string>PRESUBMISSION</string>
				<string>NIGHTLY</string>
			</array>
			<key>WorkingDirectory</key>
			<string>/AppleInternal/Tests/libarchive</string>
			<key>Timeout</key>
			<integer>3600</integer>
		</dict>"""

cmd_token_to_name_file_and_test_cmd = {
    "<LIBARCHIVE_TEST_CMD>" : ("../libarchive/libarchive/test/list.h", libarchieve_test_cmd),
    "<BSDTAR_TEST_CMD>" : ("../libarchive/tar/test/list.h", bsdtar_test_cmd),
    "<BSDCPIO_TEST_CMD>" : ("../libarchive/cpio/test/list.h", bsdcpio_test_cmd) 
}

def input_names_from_c_header_file(c_header_file):
    with open(c_header_file, 'r') as myFile:
        c_header = myFile.read()
        myFile.close()
    test_names = parser.extract_test_names_from_c_header(c_header)
    return test_names

def concrete_seperate_test_commands(test_names, test_command):
    result = ""
    for test_name, test_index in zip(test_names,range(len(test_names))):
        new_command = create_single_test_command(test_command, test_index, test_name)
        result += new_command
        result += '\n'
    return result

def create_single_test_command(test_command,test_index,test_name):
    index_token = "TESTINDEX"
    name_token = "TESTNAME"
    test_command = test_command.replace(index_token, str(test_index))
    test_command = test_command.replace(name_token, test_name.strip())
    return test_command

def output_to_plist(result):
    output_file = "libarchive.plist"
    with open(output_file, 'w') as myFile:
        myFile.write(result)
        myFile.close()

def main():
	global utd_plist
	
	for cmd_token in cmd_token_to_name_file_and_test_cmd:
		c_header_file = cmd_token_to_name_file_and_test_cmd[cmd_token][0]
		test_cmd = cmd_token_to_name_file_and_test_cmd[cmd_token][1]
		test_names = input_names_from_c_header_file(c_header_file)
		if len(test_names) > 0:
			concreted_test_command = concrete_seperate_test_commands(test_names, test_cmd)
			utd_plist = utd_plist.replace(cmd_token, concreted_test_command)
			print("Generated utd commands for {} tests from {}.".format(len(test_names), c_header_file))

	output_to_plist(utd_plist)

if __name__ == "__main__":
    main()
