($Id: README.txt,v 1.1.1.1 2001/01/25 04:59:26 wsanchez Exp $)
BUILD INSTRUCTIONS

1.	Install SGI's State Threads Library

    http://oss.sgi.com/projects/state-threads/download/

	(Just copy st.h to /usr/include and libst.* to /usr/lib)
	
2.	Install the event-based IRC Gateway Library

	http://schumann.cx/ircg/

3.	Download and extract thttpd 2.20b or later

	http://www.acme.com/software/thttpd/

4.	Install PHP into thttpd

	$ ./configure [..] \
			--with-thttpd=../thttpd-2.xx \
			--with-ircg=/prefix/of/ircg

	$ make install

5.	Patch thttpd to add State Thread support

	$ cd thttpd-2.xx
	$ patch -p1 < ../IRCG-x.x/patch_thttpd

6.	Configure and install thttpd



RECOMMENDATIONS


You will also need some kind of IRC server. You can use a public server,
but that makes testing usually slow.

I prefer to use a local IRC server, like Undernet's ircu.

	http://coder-com.undernet.org/

A highly customizable PHP framework can be found here:

	http://schumann.cx/ircg/ircg-php-scripts.tar.gz

Start with index.php.

