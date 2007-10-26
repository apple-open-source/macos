

                  Apache FastCGI Module Installation


!!!
!!!  See the INSTALL.AP2 document for information on how to build
!!!  mod_fastcgi for the Apache 2.X series.
!!!


Notes
=====

  See docs/mod_fastcgi.html for configuration information.
  
  This module supports Apache 1.3+.  If your server is 1.2 based, either
  upgrade or use mod_fastcgi v2.0.18.

  mod_fastcgi has not been tested on all of the Apache supported
  platforms.  These are known to work: SunOS, Solaris, SCO, Linux,
  NetBSD (see http://www.netbsd.org/packages/www/ap-fastcgi/), FreeBSD,
  Digital Unix, AIX, IRIX, FreeBSD, Windows (NT4 and NT2K), MacOSX
  (10.1.4), and QNX (Inet sockets only).  If you're successful in using
  this module on other platforms, please email 
  fastcgi-developers @ fastcgi.com.

  This module is maintained at http://www.fastcgi.com.  
  
  See the web page for mailing list information.
  
  
Introduction
============

  There are three approaches to configure, compile, and install Apache.

    o APACI - (Apache 1.3+) described in <apache_dir>/INSTALL

    o manual - (original) described in <apache_dir>/src/INSTALL
    
    o DSO (Dynamic Shared Object) - described in 
      <apache_dir>/htdocs/manual/dso.html
    
  If you have a binary Apache distribution, such as Red Hat's Secure
  Server (or prefer a DSO based Apache), you have to build mod_fastcgi 
  as a Dynamic Shared Object (DSO) - see Section 3.

  If your on Windows NT, see Section 4.


1) Installing mod_fastcgi with APACI
====================================

  1. Copy or move the mod_fastcgi distribution directory to
     <apache_dir>/src/modules/fastcgi.

  2. Specify "--activate-module=src/modules/fastcgi/libfastcgi.a" as an
     argument to ./configure from the <apache_dir> directory.  If you've
     previously used APACI to configure Apache, you can also specify this
     as an argument to ./config.status (Apache 1.3.1+) in order to
     preserve the existing configuration.

       <apache_dir>$ ./configure  \
                  --activate-module=src/modules/fastcgi/libfastcgi.a

     or

       <apache_dir>$ ./config.status  \
                  --activate-module=src/modules/fastcgi/libfastcgi.a

  3. Rebuild and reinstall Apache.

       <apache_dir>$ make
       <apache_dir>$ make install

  4. Edit the httpd configuration files to enable your FastCGI
     application(s).  See docs/mod_fastcgi.html for details.

  5. Stop and start the server.

       <apache_dir>$ /usr/local/apache/sbin/apachectl stop
       <apache_dir>$ /usr/local/apache/sbin/apachectl start


2) Installing mod_fastcgi manually
==================================

  1. Copy or move the mod_fastcgi distribution directory to
     <apache_dir>/src/modules/fastcgi.

  2. Add the FastCGI module to <apache_dir>/src/Configuration.  Note
     that modules are listed in reverse priority order --- the ones that
     come later can override the behavior of those that come earlier.  I
     put mine just after the mod_cgi entry.

       AddModule modules/fastcgi/libfastcgi.a

  3. From the <apache_dir>/src directory, reconfigure and rebuild Apache.

       <apache_dir>/src$ ./Configure
       <apache_dir>/src$ make
    
     Install the new httpd.

  4. Edit the httpd configuration files to enable your FastCGI
     application(s).  See docs/mod_fastcgi.html for details.

  5. Stop and start the server.

       $ kill -TERM `cat <run_dir>/logs/httpd.pid`
       $ <run_dir>/bin/httpd -f <run_dir>/conf/httpd.conf


3) Installing mod_fastcgi as a DSO
==================================

  NOTE: If you use FastCgiSuexec, mod_fastcgi cannot reliably 
  determine the suexec path when built as a DSO.  To workaround
  this, provide the full path in the FastCgiSuexec directive.
  
  1. From the mod_fastcgi directory, compile the module.
  
       $ cd <mod_fastcgi_dir>
       <mod_fastcgi_dir>$ apxs -o mod_fastcgi.so -c *.c
    
  2. Install the module.
  
       <mod_fastcgi_dir>$ apxs -i -a -n fastcgi mod_fastcgi.so
    
     This should create an entry in httpd.conf that looks like this: 
 
       LoadModule fastcgi_module  <some_path>/mod_fastcgi.so
    
     Note that if there's a ClearModuleList directive after new entry,
     you'll have to either move the LoadModule after the ClearModuleList
     or add (have a look at how the other modules are handled):

       AddModule mod_fastcgi.c

  3. Edit the httpd configuration file(s) to enable your FastCGI
     application(s).  See docs/mod_fastcgi.html for details.

     If you want to wrap the mod_fastcgi directives, use:

       <IfModule mod_fastcgi.c>
       .
       .
       </IfModule>

  4. Stop and start the server.

       $ <run_dir>/bin/apachectl stop
       $ <run_dir>/bin/apachectl start


4) Windows NT
=============

  To build mod_fastcgi from the command line:

    1. Edit the APACHE_SRC_DIR variable in Makefile.nt.

    2. Build the module

         > nmake -f Makefile.nt CFG=[debug | release]

  To build mod_fastcgi as a project you'll need M$ VC++ 6.0:

    1. Open the mod_fastcgi project file with the VC++.

    2. Edit the Project for your configuration.
    
      a) Select Project->Settings or press <ALT+F7>.

      b) Select "All Configurations" from "Settings For" drop-down menu.

      c) Select the "C/C++" tab.
      
      d) Select "Preprocessor" from the "Category" drop-down menu.

      e) Edit the path in "Additional include directories" to include your
         Apache source header files (e.g. C:\apache_1.3.12\src\include).

      f) Select the "Link" tab.
      
      g) Select "General" from the "Category" drop-down menu.
      
      h) Select "Win32 Release" from "Settings For" drop-down menu.

      i) Edit the path in "Object/library modules" to include your Apache
         Release library (e.g. C:\apache_1.3.12\src\CoreR\ApacheCore.lib).

      j) Select "Win32 Debug" from "Settings For" drop-down menu.

      k) Edit the path in "Object/library modules" to include your Apache
         Debug library (e.g. C:\apache_1.3.12\src\CoreD\ApacheCore.lib).

      l) Select OK.

    3. Select "Set Active Configuration" from the "Build" menu and choose
       either a release or debug build.
       
    4. Select "Build mod_fastcgi.dll" from the "Build" menu.

  To install mod_fastcgi (built above or retrieved from 
  http://fastcgi.com/dist/):

    1. Copy the mod_fastcgi.dll to the Apache modules directory 
       (e.g. C:\Apache\modules)
      
    2. Edit the httpd configurion file (e.g. C:\Apache\conf\httpd.conf)
       and add a line like:

         LoadModule fastcgi_module modules/mod_fastcgi.dll

       Note that if there's a ClearModuleList directive after new entry,
       you'll have to either move the LoadModule after the ClearModuleList
       or add (have a look at how the other modules are handled):

         AddModule mod_fastcgi.c

    3. Edit the httpd configuration file(s) to enable your FastCGI
       application(s).  See docs/mod_fastcgi.html for details.

