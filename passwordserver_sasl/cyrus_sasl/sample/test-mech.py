#!/usr/bin/python

# Pipe sample-client <-> sample-server

import os
import subprocess
import fcntl
import errno
import time

os.system("xcodebuild -target sample-client")
os.system("xcodebuild -target sample-server")

MECH = "SRP"
CLIENT_PATH = "build/Deployment/sample-client"
SERVER_PATH = "build/Deployment/sample-server"

os.environ["SASL_SAMPLE_CLIENT_PASSWORD"] = "password123"

# test hash for 'password123'
auxprop_data_SRP_4096 = """
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
        <key>gp</key>
        <string>rfc5054_4096</string>
        <key>iterations</key>
        <integer>19417</integer>
        <key>md</key>
        <string>sha512</string>
        <key>salt</key>
        <data>
        ty2GM+1yN6QYYYQQ2LGIUM40YTPJZ+woNVKiEZeWo3M=
        </data>
        <key>verifier</key>
        <data>
        Q9IlglnV1RIK13Inwg04l2CKHH0apGJWejoXyGhe2oMZ194f14oEN+3Ql3MSWt+RywKo
        naH053gun6c0L1NcCkAVCIBQsFTARwGo+cgNoDgMbwnG1QXuFTc0LYC3/UMvKnP/VXPo
        8wV9m/YCTUaVyfEijbzLjq6j7qM26i4Kr8BE+Hu19ZvZz/w6IBz1AR5Yi5TXhGTX5jxr
        IvBDg+ALPgKWIlWk6K4xhAFthdg+JLY2FQwl6MS8Ab616WtbKf62ATYjFUH9PlX6xAEc
        Fe2s1lSx3WnKDQ+3wCsjDThYytCO+TxqWn482700r651Y7370k4E6uLB61NDKMJdcpQW
        Cu4yp9KdoeviVGDzRAmzXeQSf8pmRL4ACboaSl30JBxCQxLad3EXssgKdi94gwhQJlrZ
        KVDcWIhE1ttYxOKoyxBEaQFKedC5iYmUyaGiyd0cZLCxAHi9Iah0TQJhRoVkEC3MKd8S
        J2BJil4UsVTxAR1TQTRVTjv4jZCEmaVfFjF2/hRDDk3zKMz7FWjtz+5XhHbJ8iRDr76U
        0G7rwXU+H2FeDPs/IbCFUmO2KyLZ9YmKl2SdL/qrI6afxCJSdSR2/ymvMZzxumJYlStZ
        eeL+ZbZoAou/R9HRgKNKN+M3AgqzQ5BJdBuqmfrxFa40FSvkEegvqszJUM3XfvYbnTq3
        mBk=
        </data>
</dict>
</plist>
"""

auxprop_data_SRP_8192 = """
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
        <key>gp</key>
        <string>rfc5054_8192</string>
        <key>iterations</key>
        <integer>19569</integer>
        <key>md</key>
        <string>sha512</string>
        <key>salt</key>
        <data>
        2/ZU+qVAc4BMjCN8DVQdCnj3Cf4QyQbiHGJgjrp+gf4=
        </data>
        <key>verifier</key>
        <data>
        H57Wyg4hvAfOd3F5dzyXjevPS3MBrE7CLOXI1IlF52wWIEJo2pfdyq0kBaLrK4MyN7Ze
        2GTqpRa6rG6dyjwhfVmbAOVQkS8d3k0NPyyaNp6WFKYfuqjO5vSpvsWCnWyUdy8ZP6qt
        mPzQLkEWE+BI2c/0Fe9V8uhREUfIfDAkgp43JqDYtZT/rBb8sqVWWeB8sRyO2kvV+Nf5
        3GvGXLFpJpmyaaQysItje60x7zwdgS9XRQcDxT3zgJx07sNcRsCRnLBCalmW8jYY/9MO
        NsCeXlI5wHnyJFwTNAISYbCV3TuKA7YoxSEh9wrGpSy+yy7TV8y0anAPEZWYvr5w669e
        ywb3oAk8IVK9+m6kN1L3k8ogJ569YCSRz3juMZbId7pEcH+6avdsAHSdf7832/56NRq3
        yVLwSRU1ltPWheAiis8P0hf6F5PNAXnbt/LiLJj+2dFi4OgCu2eDSwdwKliqi4K07yjm
        JzgjxpRX5+nlV6yti9FqrvjpP8XkXKrUv2spbMJeE/lCS527RJrjy2wXhvhcrueGJmlI
        n9/6SeUw7mKKvOOePpOHLhV5H3ljBfRTtvQbVOZ0hL3f/3jLjhwdGvvEEBktGk6D5Twl
        Jf2G+dvSUwe4v5MCYYiwmYG6w5YZf339vy7iMB0P79YZmkhnW75su/ZDvJcUwz7J+4a1
        mf1Xf5dQR1lWuGY2NtrQ8HmqoUNuQ0H42C+H1deS8B2g56rw4G32v8fyZWLCRKpM9Nfb
        DPKlF0h/8UbR5+jcF8KmHc2HSzbQRjUw6tzrd8wCNtJoIMEs4bDKm59B9zWHoFa+kZyc
        iSlii4DDDV9v70FcxBA0WfKE0lL/zMn1EuY4ORBqSBExiDKRY7v73DGILx0ktawlz9mO
        XobQA8tjTOIlWDIfasesuhZJ2Hr654iI4BH+ebJZNYQ0h50I67i8llkq7BFQMX430/FM
        Ifno+cUvTveeCxIcZ6McmTuzqyWijaV3Jf7VxxB4CBNd7K6O6r2pdoY1e2kBkD/0+Pl+
        gy4HG2MXWGxgVoEbZZHenOURYKPBURZK7eQr7DKLvlKtFhddok7J6ZZRrZuQMlnltB00
        x/qGyaQNO9RSXZRuJ2bRaIPfavfOelMKhyK2JZryYCGMkeLb2Sb9hMkDcIjQw6wOaNeU
        r+07nRbuGoADCKo8+5EZgVmkxglFBQjeUDeHXGeisaeWPxCC7YBecDMzUgDKzvStvkzY
        LKAp/i0d6AI9iLQhImuH7lnQ/2GZBmfFY4q6bdurpJvgRRSBwNFT1sQh1o83AELrfZZP
        IjuQ2uSp6IyVS6y/k28Z3kmeAi7Y/szy6Gcgti3Kbb/v1g4Zns5//DU2cqjSpWbYgBd0
        H8z4Ng==
        </data>
</dict>
</plist>
"""

auxprop_data_SALTED_SHA512_PBKDF2 = """
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
<key>entropy</key>
<data>
    rq8zL3CUMvoFNeGEwLuxnJN7wwc0GTmGhXjwGQsQWDSFS/7++Z9577WM+DCbEkfe+jFI
    bOxnk0U4dM0Rc8duCCMAjN5JWWXVQyThdduDIg1HR4sTihLVteUanoj4Fp0BggLsEVx/
    w8PAw60rRp5i66LK+2tm/se28vQO2zI3AZ4=
    </data>
    <key>iterations</key>
    <integer>21929</integer>
    <key>salt</key>
    <data>
    NSqE+69X4JKeIvWdxMb6TQ89hFC5kmz9wEq5ZQ1CCbE=
    </data>
</dict>
</plist>"""

server = subprocess.Popen((SERVER_PATH, "-m", MECH, "-A", auxprop_data_SRP_4096), stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
client = subprocess.Popen((CLIENT_PATH, "-a", "diradmin"), stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

# nonblocking
def setNB(fd):
    fcntl.fcntl(fd, fcntl.F_SETFL, fcntl.fcntl(fd, fcntl.F_GETFL) | os.O_NONBLOCK)

def readNB(fd):
    try:
        return os.read(fd, 65535)
    except OSError as e:
        if e.errno == errno.EAGAIN:
            return ''

def filterLines(text, prefix):
    lines = ""
    for line in text.split("\n"):
        if not line.startswith(prefix):
            continue

        lines += line + "\n"

    return lines

clines = None

setNB(server.stdout.fileno())
setNB(client.stdout.fileno())

print "reading"
while True:
    sout = readNB(server.stdout.fileno())
    if len(sout) != 0:
        print "(S)", sout

    slines = filterLines(sout, "S: ")

    client.stdin.write(slines)
    cout = readNB(client.stdout.fileno())

    if len(cout) != 0:
        print "(C)", cout

    clines = filterLines(cout, "C: ")

    server.stdin.write(clines)

    # check if processes terminated
    server.poll()
    client.poll()
    if server.returncode is not None and client.returncode is not None: 
        print "Server finished:",server.returncode
        print "Client finished:",client.returncode
        raise SystemExit
