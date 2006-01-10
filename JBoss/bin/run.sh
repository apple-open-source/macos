#!/bin/sh
### ====================================================================== ###
##                                                                          ##
##  JBoss Bootstrap Script                                                  ##
##                                                                          ##
### ====================================================================== ###
#
# Do not change this file. Use run.conf instead.
#

JBOSS_HOME=/Library/JBoss/3.2
RUNFILE=$JBOSS_HOME/run/jboss.pid
JAVA=/System/Library/Frameworks/JavaVM.framework/Versions/1.4/Home/bin/java

if [ ! -e $RUNFILE ] ; then
    touch $RUNFILE
    chmod 664 $RUNFILE
fi
CURRENT_PID=`cat $RUNFILE`
if [ -n "$CURRENT_PID" ] ; then 
    kill -0 $CURRENT_PID 2>/dev/null
    if [ $? -eq 0 ] ; then
	/bin/echo "Another JBoss instance with pid $CURRENT_PID is already running. Exiting."
	exit 1
    fi
fi

if [ ! -x $JAVA ] ; then
   /bin/echo "JBoss requires the Java 1.4.2 VM. Exiting."
   exit 1
fi
# Increase the maximum file descriptors if we can
ulimit -S -n unlimited
if [ $? -ne 0 ] ; then
    /bin/echo "CAN'T SET THE FILE DESCRIPTOR SOFT LIMIT !!!!!!"
fi

# make sure private data is only accessible by the same user
umask 077

# Setup JBoss-specific properties
# we use /var/tmp for the deployment files because /etc/daily removes
# files and dirs that haven't been accessed for a few days, which may delete
# vital files for JBoss/Tomcat

JBOSSTMPDIR=/var/tmp/jbosstmpdata$$
mkdir $JBOSSTMPDIR

#
# Avoid a warning from Jacorb
touch $JBOSSTMPDIR/jacorb.properties

#
# We change directories here because some software components such as the Jasper compiler
# use the current directory for tmp files, and the current dir may not be writable
cd $JBOSSTMPDIR

#
# include JAVA_OPTS and other user settings
#
. $JBOSS_HOME/bin/run.conf

trap 'kill $PID; wait $PID; /bin/rm -rf $JBOSSTMPDIR; cat </dev/null 2>/dev/null >$RUNFILE' 1 2 # HUP INT

# The watchdog process uses SIGTERM to end the process, but will send SIGKILL
# when the process didn't end after 10 seconds. So we have to brutally kill -9
# the Java VM for our cleanup to finish, since JBoss takes a long time to shut down
trap 'kill -9 $PID; /bin/rm -rf $JBOSSTMPDIR; cat </dev/null >$RUNFILE 2>/dev/null' 15

# less restrictive permissions for log files and the like
umask 022

# Execute the JVM
/System/Library/Frameworks/JavaVM.framework/Versions/1.4/Home/bin/java $JAVA_OPTS -classpath $JBOSS_HOME/bin/run.jar org.jboss.Main "$@" &
PID=$!
/bin/echo $PID 2>/dev/null >$RUNFILE 
wait $PID
status=$?

cat </dev/null 2>/dev/null >$RUNFILE
/bin/rm -rf $JBOSSTMPDIR
exit $status
