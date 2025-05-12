#!/usr/bin/env python3

from subprocess import run
from sys import stderr

# The two payload generators below are as published by the libexpat project
# at https://github.com/libexpat/libexpat/issues/893, modified to 10x the
# default count to reliably reproduce.

def generate_payload1(count=250000):
    # Confidential, written by Sebastian Pipping <sebastian@pipping.org>
    payload = "<!DOCTYPE doc [\n"
    payload += "  <!ENTITY e0 ''>\n"
    for i in range(count):
        payload += f"  <!ENTITY e{i + 1} '&e{i};'>\n"
    payload += "]>\n"
    payload += f"<doc>&e{count};</doc>\n"
    return payload

def generate_payload2(count=585000):
    # Confidential, written by Sebastian Pipping <sebastian@pipping.org>
    payload = "<!DOCTYPE a [\n"
    payload += "  <!ENTITY e0 ''>\n"
    for i in range(count):
        payload += f"  <!ENTITY e{i + 1} '&e{i};'>\n"
    payload += "]>\n"
    payload += f"<a key='&e{count};'/>\n"
    return payload

def generate_payload3(count=105500):
    # Confidential, written by Sebastian Pipping <sebastian@pipping.org>
    payload = "<!DOCTYPE doc [\n"
    payload += "  <!ENTITY % p0 ''>\n"
    for i in range(count):
        payload += f"  <!ENTITY % p{i + 1} '&#37;p{i};'>\n"
    payload += f"  <!ENTITY % define_g0 \"<!ENTITY g0 '&#37;p{count};'>\">\n"
    payload += "  %define_g0;\n"
    payload += "]>\n"
    payload += "<doc/>\n"
    return payload

def run_payload(payload, want_p=False):
    args = ["/AppleInternal/Tests/expat/xmlwf"]
    if want_p:
        args.append("-p")
    args.append("-r")
    args.append("/dev/stdin")
    process = run(args, input=payload, text=True)
    if process.returncode == 0:
        print("PASS: xmlwf OK")
        return True
    # Signalled cgild is th
    if process.returncode < 0:
        signal = -process.returncode
        print(f"FAIL: xmlwf was signalled {signal}", file=stderr)
    else:
        print("FAIL: xmlwf failed (error in the test script?)", file=stderr)
    return False

# run_payload() will output exactly one PASS/FAIL for each payload, and we'll
# just tally up how many fails we had.
tests_failed = 0
if not run_payload(generate_payload1()):
    tests_failed += 1

if not run_payload(generate_payload2()):
    tests_failed += 1

if not run_payload(generate_payload3(), True):
    tests_failed += 1

exit(tests_failed)
