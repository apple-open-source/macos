#!/bin/sh
 
# You may have to change this to fit your setup.
: ${JRE_HOME:="/usr/local/jdk1.3/jre"}
 
# call java interpreter
java \
    -Xbootclasspath:../lib/ext/jacorb.jar:${JRE_HOME}/lib/rt.jar:${CLASSPATH} \
    -Dorg.omg.CORBA.ORBClass=org.jacorb.orb.ORB \
    -Dorg.omg.CORBA.ORBSingletonClass=org.jacorb.orb.ORBSingleton \
    org.jacorb.ir.gui.IRBrowser `cat t.ior`
