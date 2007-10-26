

             ***  Apache FastCGI Module Installation  ***


  See docs/mod_fastcgi.html for configuration information.

  This module is maintained at http://www.fastcgi.com.  
  
  See the web page for mailing list information.
  
  
  *NIX
  ====

    $ cd <mod_fastcgi_dir>
    $ cp Makefile.AP2 Makefile
    $ make 
    $ make install

    If your Apache2 installation isn't in /usr/local/apache2, then
    set the top_dir variable when running make (or edit the
    Makefile), e.g. 

      $ make top_dir=/opt/httpd/2.0.40

    Add an entry to httpd.conf like this:

      LoadModule fastcgi_module modules/mod_fastcgi.so
 

  WIN
  ===

    To build mod_fastcgi as a project you'll need M$ VC++ 6.0 (the Makefile
    hasn't been updated for AP2 support):

    Open the mod_fastcgi project file with the VC++.

    Edit the Project for your configuration (update the Preprocessor
    and the Link paths). The default assumes a complete Apache2 
    installation in /Apache2.
    
    Build mod_fastcgi.so.

    Copy it to the Apache modules directory, e.g. /Apache2/modules.

    Add an entry to httpd.conf like this:

        LoadModule fastcgi_module modules/mod_fastcgi.so

