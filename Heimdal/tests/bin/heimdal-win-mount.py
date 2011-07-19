#!/usr/bin/python

'''login to windows server, mount a share, check existance of file, umount'''

# disabled until we have a exepct we can use
exit(0)

import pexpect, sys, plistlib

pl = plistlib.readPlist(sys.argv[1])

win = pl['windows-machine']
host = win['host']
user = win['user']
pw   = win['password']

mhost = sys.argv[2]
muser = pl['mount']['user']
mpw   = pl['mount']['password']
msharename = pl['mount']['share-name']

conn = pexpect.spawn("telnet -l '{0}' {1}".format(user, host), logfile=sys.stdout, timeout=30)
i = conn.expect(["Welcome to Microsoft Telnet Service"])
if i != 0:
    conn.close()
    print("failed to connect to host\n")
    exit(1)

i = conn.expect(["password:"])
if i != 0:
    conn.close()
    print("failed to get the password prompt\n")
    exit(1)


conn.sendline(pw + "\r\n")

i = conn.expect(["C:"])
if i != 0:
    conn.close()
    print("failed to get the C: prompt after login\n")
    exit(1)


conn.sendline("net use Z: /delete\r\n")
i = conn.expect(["C:"])
if i != 0:
    conn.close()
    print("failed to get the C: prompt after unmount\n")
    exit(1)

conn.sendline("net use Z: \\\\{2}\\{3} {0} /user:{1}\r\n".format(mpw, muser, mhost, msharename))
i = conn.expect(["The command completed successfully",
                 "The network path was not found",
                 "The specified network password is not correct",
                 "System error \d+ has occurred",
                 "The network connection could not be found"])
if i != 0:
    conn.close()
    print("Failed mounting the directory\n")
    exit(1)

i = conn.expect(["C:"])
if i != 0:
    conn.close()
    print("failed to get the C: prompt after unmount\n")
    exit(1)

conn.sendline("dir Z:\r\n")
i = conn.expect(["there-is-a-file-here"])
if i != 0:
    conn.close()
    print("file missing\n")
    exit(1)

i = conn.expect(["C:"])
if i != 0:
    conn.close()
    print("failed to get the C: prompt\n")
    exit(1)

conn.sendline("net use Z: /delete\r\n")
i = conn.expect(["C:"])
if i != 0:
    conn.close()
    print("failed to get the C: prompt after unmount\n")
    exit(1)


conn.sendline("exit\r\n")

print("success testing {0}@{1}/{2}".format(muser, mhost, msharename))


