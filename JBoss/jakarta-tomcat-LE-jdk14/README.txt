$Id: README.txt,v 1.20 2003/01/08 03:50:26 glenn Exp $

                   The Tomcat 4.1 Servlet/JSP Container
                   ====================================

This subproject contains a server that conforms to the Servlet 2.3 and
JSP 1.2 specifications from Java Software.  It includes the following contents:

  BUILDING.txt                Instructions for building from sources
  LICENSE                     Apache Software License for this release
  README.txt                  This document
  RELEASE-NOTES-*.txt         Release Notes for this (and previous) releases
                              of Tomcat 4.1
  RUNNING.txt                 Instructions for installing Tomcat, as well as
                              starting and stopping the server
  bin/                        Binary executables and scripts
  common/                     Classes available to both Catalina internal
                              classes and web applications:
    classes/                  Unpacked common classes
    lib/                      Common classes in JAR files
  conf/                       Configuration files
  logs/                       Destination directory for log files
  server/                     Internal Catalina classes and their dependencies
    classes/                  Unpacked classes (internal only)
    lib/                      Classes packed in JAR files (internal only)
  shared/                     Classes shared by all web applications
    classes/                  Unpacked shared classes
    lib/                      Shared classes in JAR files
  webapps/                    Base directory containing web applications
                              included with Tomcat 4.1
  work/                       Scratch directory used by Tomcat for holding
                              temporary files and directories
  temp/                       Directory used by JVM for temporary files
                              (java.io.tmpdir)

If you wish to build the Tomcat server from a source distribution,
please consult the documentation in "BUILDING.txt".

If you wish to install and run a binary distribution of the Tomcat server,
please consult the documentation in "RUNNING.txt".


                      Acquiring Tomcat 4.1 Releases
                      =============================

Nightly Builds
--------------

Nightly Builds of Tomcat 4.1 are built from the most recent CVS sources each
evening (Pacific Time).  The filename of the downloadable file includes the
date it was created (in YYYYMMDD format).  These builds are available at:

Binary:  http://jakarta.apache.org/builds/jakarta-tomcat-4.0/nightly/
Source:  http://jakarta.apache.org/builds/jakarta-tomcat-4.0/nightly/src/


Release Builds
--------------

Release Builds of Tomcat 4.1 are created and released periodically, and
announced to the interested mailing lists.  Each release build resides in its
own directories.  For example, the Tomcat 4.1.18 release is available at:

Binary:  http://jakarta.apache.org/builds/jakarta-tomcat-4.0/release/v4.1.18/bin/
Source:  http://jakarta.apache.org/builds/jakarta-tomcat-4.0/release/v4.1.18/src/

