($Id: README.txt,v 1.1.1.2 2001/07/19 00:19:19 zarzycki Exp $)

STATUS

    Mailing lists and other information can be found on IRCG's homepage:

    http://schumann.cx/ircg/


INSTALLATION: THE QUICK WAY


1.  Download

    http://schumann.cx/ircg/install_ircg.sh

2.  Edit some variables at the top of that script:

    prefix
    	the directory where all software gets installed (the user running
    	the script must have write permission to that directory).
    
    php4_sourcedir
       	a very recent checkout of the PHP 4 Repository, fully buildconf'd.
    
    st_target
    	the State Threads Target (i.e. freebsd-optimized)

    st_targetdir
    	the State Threads Directory name (i.e. FREEBSD_`uname -r`_OPT)

    thttpd/IRCG/st    (probably don't need to be changed)
    	the latest version numbers of the respective software

    State Threads and IRCG currently require GNU make, so if your
    system make is not GNU make, apply s/make/gmake/ or whatever
    is appropiate for your system to install_ircg.sh.

3.  Run

    	chmod +x install_ircg.sh
    	./install_ircg.sh

    If you have a standard system (i.e. Linux or FreeBSD), the process 
    should finish without any problem.

    Note 1:  Currently, the shipped patches are in the unified diff
    format.  Some ancient patch tools cannot handle that format.  On
    those systems, installing GNU patch is recommened.

    Note 2:  The State Threads build system is quite simple.  On some
    ancient systems, State Threads handle only the vendor compiler,
    not GCC.  In that case, you might need to fiddle with the build
    system yourself.. Good luck.
    

    

THE MANUAL WAY


1.  Install SGI's State Threads Library

    http://oss.sgi.com/projects/state-threads/download/

    (Just copy st.h to /usr/include and libst.* to /usr/lib)
    
2.  Install the event-based IRC Gateway Library

    http://schumann.cx/ircg/

3.  Download and extract thttpd 2.20b or later

    http://www.acme.com/software/thttpd/

4.  Install PHP into thttpd

    $ ./configure [..] \
    		--with-thttpd=../thttpd-2.xx \
    		--with-ircg=/prefix/of/ircg

    $ make install

5.  Patch thttpd to add State Thread support

    $ cd thttpd-2.xx
    $ patch -p1 < ../IRCG-x.x/patch_thttpd

6.  Configure and install thttpd



RECOMMENDATIONS


You will also need some kind of IRC server. You can use a public server,
but that makes testing usually slow.

I prefer to use a local IRC server, like UnrealIRCD:

    http://www.unrealircd.com/

Or the Undernet IRC server:
    
    http://coder-com.undernet.org/

A highly customizable PHP framework can be found here:

    http://schumann.cx/ircg/ircg-php-scripts.tar.gz

Start with index.php.

