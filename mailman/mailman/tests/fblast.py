#! /usr/bin/env python
"""Throw email at Mailman as fast as you can.

This is not a unit test, it's a functional test, so you can't run it within
the unit test framework (hence its filename doesn't start with `test_').
Here's how I use this one:

- set up a dummy list

- add an alias to your MTA, say `devnull' that pipes its messages to, you
  guessed it, /dev/null

- make this address a member of your list

- add another address to `accept_these_non_members', let's call it ok@dom.ain

- change the FROMADDR variable to ok@dom.ain

- change the LISTADDR variable to point to your list

- run this program like so: python fblast.py N
  where N is the number of seconds to sleep before sending the next msg

- let this run until you're tired of it, then hit ^C
"""

FROMADDR = 'ok@dom.ain'
LISTADDR = 'list@dom.ain'

import sys
import time
import smtplib

conn = smtplib.SMTP()
conn.connect()

snooze = int(sys.argv[1])

try:
    i = 1
    while 1:
        sys.stdout.write('.')
        sys.stdout.flush()
        i += 1
        if i % 50 == 0:
            print
        for j in range(10):
            conn.sendmail(FROMADDR, [LISTADDR], """\
From: %(FROMADDR)s
To: $(LISTADDR)s
Subject: test %(num)d
X-No-Archive: yes

testing %(num)d
""" % {'num'     : i,
       'FROMADDR': FROMADDR,
       'LISTADDR': LISTADDR,
       })
        time.sleep(snooze)
finally:
    conn.quit()
