#!/bin/sh

# How to marathon tests
#
# 1. Create or adjust a ".ant.properties" file
# - set "marathon.duration" to the time the test should run in milliseconds !!
# - set "marathon.timeout" to add least 3 minutes longer than the duration !!
# - set "marathon.threadcount" to the number of concurrent thread running in this test
#       NOTE: that each thread represents a customer in this test
#
# 2. Start JBoss 3
#
# 3. Adjust jndi.properties if you want to access a remote JBoss 3 server (recommended)
# 
# 4. Start this script and wait
#
# 5. Have a look at the generated output file
#

echo ".ant.properties" file contains:
cat .ant.properties

echo build.sh tests-standard-marathon tests-reports $*

build.sh tests-standard-marathon tests-reports $*
