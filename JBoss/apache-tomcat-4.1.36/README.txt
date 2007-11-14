$Id: README.txt 376462 2006-02-09 22:26:09Z markt $

                   The Tomcat 4.1 Servlet/JSP Container
                   ====================================

This subproject contains a server that conforms to the Servlet 2.3 and
JSP 1.2 specifications from Java Software.  It includes the following contents:

  BUILDING.txt                Instructions for building from sources
  LICENSE                     Apache Software License for this release
  README.txt                  This document
  RELEASE-NOTES-4.1.txt       Release Notes for this (and previous) releases
                              of Tomcat 4.1
  RUNNING.txt                 Instructions for installing Tomcat, as well as
                              starting and stopping the server
  bin/                        Binary executables and scripts
  common/                     Classes available to both Catalina internal
                              classes and web applications:
    classes/                  Unpacked common classes
    endorsed/                 JARs over-riding standard APIs as per the
                              "Endorsed Standards Override Mechanism"
    lib/                      Common classes in JAR files
  conf/                       Configuration files
  logs/                       Destination directory for log files
  server/                     Internal Catalina classes and their dependencies
    classes/                  Unpacked classes (internal only)
    lib/                      Classes packed in JAR files (internal only)
    webapps/                  Web applications for administration of Tomcat
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

Nightly Builds of Tomcat 4.1 are no longer produced.


Release Builds
--------------

Release Builds of Tomcat 4.1 are created and released periodically, and
announced to the interested mailing lists.  The current binary and source releases
are avaialble from:

http://tomcat.apache.org/download-41.cgi

Previous releases may be found in the Apache archives, available via the above
download page.
