"""Generator for the Mailman on-line documentation.

Requires ht2html.py, available from http://ht2html.sourceforge.net
"""

import os
import time

from Skeleton import Skeleton
from Sidebar import Sidebar, BLANKCELL
from Banner import Banner
from HTParser import HTParser
from LinkFixer import LinkFixer



sitelinks = [
    # Row 1
    ('%(rootdir)s/index.html',    'Home'),
    ('%(rootdir)s/docs.html',     'Documentation'),
    ('%(rootdir)s/lists.html',    'Mailing lists'),
    ('%(rootdir)s/help.html',     'Help'),
    ('%(rootdir)s/download.html', 'Download'),
    ('%(rootdir)s/devs.html',     'Developers'),
    ]



class MMGenerator(Skeleton, Sidebar, Banner):
    def __init__(self, file, rootdir, relthis):
        self.__body = None
        root, ext = os.path.splitext(file)
        html = root + '.html'
        p = self.__parser = HTParser(file, 'mailman-users@python.org')
        f = self.__linkfixer = LinkFixer(html, rootdir, relthis)
        p.process_sidebar()
        p.sidebar.append(BLANKCELL)
        # massage our links
        self.__d = {'rootdir': rootdir}
        self.__linkfixer.massage(p.sidebar, self.__d)
        # tweak
        p.sidebar.append((None,
                          '''<a href="http://www.python.org/"><img border=0
                          src="%(rootdir)s/images/PythonPoweredSmall.png"
                          ></a>&nbsp;<a href="http://sourceforge.net"><img 
                          src="http://sourceforge.net/sflogo.php?group_id=103"
                          width="88" height="31" border="0"
                          alt="SourceForge Logo"></a>'''
                          % self.__d))
        p.sidebar.append(BLANKCELL)
        thisyear = time.localtime()[0]
        copyright = self.__parser.get('copyright', '1998-%s' % thisyear)
        p.sidebar.append((None, '&copy; ' + copyright + """
Free Software Foundation, Inc.  Verbatim copying and distribution of this
entire article is permitted in any medium, provided this notice is preserved.
"""))
        Sidebar.__init__(self, p.sidebar)
        #
        # fix up our site links, no relthis because the site links are
        # relative to the root of my web pages
        #
        sitelink_fixer = LinkFixer(f.myurl(), rootdir)
        sitelink_fixer.massage(sitelinks, self.__d, aboves=1)
        Banner.__init__(self, sitelinks, cols=3)
        # kludge!
##        for i in range(len(p.sidebar)-1, -1, -1):
##            if p.sidebar[i] == 'Email Us':
##                p.sidebar[i] = 'Email me'
##                break

    def get_corner(self):
        rootdir = self.__linkfixer.rootdir()
        return '''
<center>
    <a href="%(rootdir)s/index.html">
    <img border=0 src="%(rootdir)s/images/logo-70.jpg"></a></center>''' \
    % self.__d

    def get_corner_bgcolor(self):
        return 'white'

    def get_banner(self):
        return Banner.get_banner(self)

    def get_title(self):
        return self.__parser.get('title')

    def get_sidebar(self):
        return Sidebar.get_sidebar(self)

    def get_banner_attributes(self):
        return 'CELLSPACING=0 CELLPADDING=0'

    def get_body(self):
        if self.__body is None:
            self.__body = self.__parser.fp.read()
        return self.__body

    def get_lightshade(self):
        """Return lightest of 3 color scheme shade."""
        # The Mailman logo's foreground is approximately #da7074
        #return '#99997c'
        #return '#a39c82'
        #return '#caa08f'
        return '#eecfa1'

    def get_darkshade(self):
        """Return darkest of 3 color scheme shade."""
        #return '#545454'
        return '#36648b'
