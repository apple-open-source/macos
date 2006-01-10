# -----------------------------------------------------------------------------
#  Set CLASSPATH and Java options
#
#  $Id: setclasspath.sh,v 1.2 2005/11/13 23:39:55 kiddyr Exp $
# -----------------------------------------------------------------------------

JAVA_HOME=/System/Library/Frameworks/JavaVM.framework/Versions/1.4/Home

# Make sure prerequisite environment variables are set
if [ -z "$JAVA_HOME" ]; then
  echo "The JAVA_HOME environment variable is not defined"
  echo "This environment variable is needed to run this program"
  exit 1
fi
if [ ! -r "$JAVA_HOME"/bin/java -o ! -r "$JAVA_HOME"/bin/jdb -o ! -r "$JAVA_HOME"/bin/javac ]; then
  echo "The JAVA_HOME environment variable is not defined correctly"
  echo "This environment variable is needed to run this program"
  exit 1
fi
if [ -z "$BASEDIR" ]; then
  echo "The BASEDIR environment variable is not defined"
  echo "This environment variable is needed to run this program"
  exit 1
fi
if [ ! -r "$BASEDIR"/bin/setclasspath.sh ]; then
  echo "The BASEDIR environment variable is not defined correctly"
  echo "This environment variable is needed to run this program"
  exit 1
fi

# Set the default -Djava.endorsed.dirs argument
JAVA_ENDORSED_DIRS="$BASEDIR"/common/endorsed

# Set standard CLASSPATH
CLASSPATH="$JAVA_HOME"/lib/tools.jar

# OSX hack to CLASSPATH
JIKESPATH=
if [ `uname -s` = "Darwin" ]; then
  OSXHACK="/System/Library/Frameworks/JavaVM.framework/Versions/CurrentJDK/Classes"
  if [ -d "$OSXHACK" ]; then
    for i in "$OSXHACK"/*.jar; do
      JIKESPATH="$JIKESPATH":"$i"
    done
  fi
fi

# Set standard commands for invoking Java.
_RUNJAVA="$JAVA_HOME"/bin/java
_RUNJDB="$JAVA_HOME"/bin/jdb
_RUNJAVAC="$JAVA_HOME"/bin/javac
