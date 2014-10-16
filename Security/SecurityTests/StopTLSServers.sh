#!/bin/sh

#  StopTLSServers.sh
#  Security
#
#  Created by Fabrice Gautier on 6/9/11.
#  Copyright 2011 Apple, Inc. All rights reserved.

echo "Killing all servers..."

killall openssl
killall gnutls-serv

echo "Done killing~~"
