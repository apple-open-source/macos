

*** IMPORTANT WARNING ***

IRCG WILL NOT WORK WITH APACHE OR ANY OTHER WEB SERVERS
OTHER THAN THOSE DESCRIBED BELOW.

PLEASE DO NOT SEND ENQUIRIES TO THE AUTHOR BY ANY MEANS
CONCERNING UNSUPPORTED CONFIGURATIONS OR ANY OTHER
SUPPORT QUESTIONS.  WHEN AND IF APPROPIATE, CONTACT THE
REGULAR PHP MAILING LISTS OR, AT YOUR DISCRETION, ASK ON
THE IRCG MAILING LIST.

*** IMPORTANT WARNING ***



($Id: README.txt,v 1.1.1.5 2003/03/11 01:09:22 zarzycki Exp $)

STATUS

    Mailing lists and other information can be found on IRCG's homepage:

    http://schumann.cx/ircg/


	
INSTALLATION: THE QUICK AND EASY WAY


1.  Download

    http://schumann.cx/ircg/install_ircg.sh

2.  Open the script in your favorite editor and set up the three
    variables for installation prefix, PHP 4 source directory and
    Operating System.  Please refer to the comments inside the script.

3.  Run

        chmod +x install_ircg.sh
        ./install_ircg.sh

    If you are installing on a common system (e.g. Linux or FreeBSD), the 
    process should finish without any problem.  

    Note 1:  Currently, the shipped patches are in the unified diff
    format.  Some esoteric patch tools cannot handle that format.  On
    those systems, installing GNU patch is recommended.

        ftp://ftp.gnu.org/pub/gnu/patch/
	
    Note 2:  The State Threads build system is quite simple.  On some
    esoteric systems, State Threads handle only the vendor compiler,
    not GCC.  In that case, you might need to fiddle with the build
    system yourself.. Good luck.
    



THE LONG AND BORING WAY

Unless there is a specific reason (like you are bored or generally
have too much time on your hands), you are advised to use the
installation script as outlined above.


1.  Install SGI's State Threads Library

    http://state-threads.sourceforge.net/

    (Just copy st.h to /usr/include and libst.* to /usr/lib)
    
2.  Install the event-based IRC Gateway Library

    http://schumann.cx/ircg/

3.  Download and extract thttpd 2.21b

    http://www.acme.com/software/thttpd/thttpd-2.21b.tar.gz

4.  Install PHP into thttpd

    $ ./configure [..] \
    		--with-thttpd=../thttpd-2.xx \
    		--with-ircg=/prefix/of/ircg

    $ make install

5.  Patch thttpd to add State Thread support

    $ cd thttpd-2.xx
    $ patch -p1 < ../IRCG-x.x/patch_thttpd

    IMPORTANT: The process will throw a SIGSEGV or SIGBUS if you
               forget to apply this patch.
	
6.  Configure and install thttpd



RECOMMENDATIONS


You will also need some kind of IRC server. You can use a public server,
but that makes testing usually slow.

Use the Undernet IRC server (it's fast, it scales, and it has
lots of hidden options to tune it):
    
    http://coder-com.undernet.org/

A highly customizable PHP framework can be found here:

    http://schumann.cx/ircg/ircg-php-scripts.tar.gz

Start by reading index.php.

