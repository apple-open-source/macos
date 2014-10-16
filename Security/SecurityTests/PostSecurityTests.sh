#!/bin/sh

#  PostSecurityTests.sh
#  Security
#
#  Created by Fabrice Gautier on 6/9/11.
#  Copyright 2011 Apple, Inc. All rights reserved.

echo "PostSecuritTests.sh: post-run script"

echo "Killing all servers..."

killall openssl
killall gnutls-serv
kill `cat tcprelay.pid`

echo "Done killing~~"
