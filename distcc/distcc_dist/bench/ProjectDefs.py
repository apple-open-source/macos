# distcc/benchmark -- automated system for testing distcc correctness
# and performance on various source trees.

# Copyright (C) 2002, 2003, 2004 by Martin Pool

# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.

# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA

__doc__ = """distcc benchmark project definitions"""

from Project import Project

# Would like to test glibc, but it needs a separate source and build
# directory, and this tool doesn't support that yet.  

# disable-sanity-checks is needed to stop it wanting linuxthreads --
# the resulting library is useless, but this is only a test.

#Project(url = 'http://ftp.gnu.org/pub/gnu/glibc/glibc-2.3.2.tar.bz2',
#        configure_cmd = './configure --disable-sanity-checks'
#        ).register()

#Project(url='http://mirror.aarnet.edu.au/pub/gnu/libc/glibc-2.3.tar.bz2',
#        configure_cmd='./configure --disable-sanity-checks',
#        md5='fd20b4a9feeb2b2f0f589b1a9ae8a5e2  glibc-2.3.tar.bz2').register()

Project(url='http://apache.planetmirror.com.au/dist/httpd/old/httpd-2.0.43.tar.gz',
        md5='8051de5d160c43d4ed2cc47dc9be6fd3  httpd-2.0.43.tar.gz').register()

Project(url='ftp://ftp.gtk.org/pub/gtk/v2.0/glib-2.0.7.tar.bz2',
        md5='5882b1e729f57cb18af653a2f504197b  glib-2.0.7.tar.bz2').register()

Project(url='http://us1.samba.org/samba/ftp/old-versions/samba-2.2.7.tar.bz2',
        build_subdir="source",
        md5='9844529c047cd454fad25a0053994355  samba-2.2.7.tar.bz2').register()

Project(url='http://mirror.aarnet.edu.au/pub/gnu/make/make-3.80.tar.bz2',
        md5='0bbd1df101bc0294d440471e50feca71 *make-3.80.tar.bz2'
        ).register()

Project(url='http://public.ftp.planetmirror.com/pub/linux/kernel/v2.4/linux-2.4.20.tar.bz2',
        configure_cmd='make defconfig',
        build_cmd='make bzImage',
        ).register()

Project(url='http://www.kernel.org/pub/linux/kernel/v2.5/linux-2.5.51.tar.bz2',
        md5='2300b7b7d2ce4c017fe6dae49717fd9a *linux-2.5.51.tar.bz2',
        configure_cmd='make defconfig',
        build_cmd='make bzImage'
        ).register()

Project(url='http://mirror.aarnet.edu.au/pub/gnu/gdb/gdb-5.3.tar.gz'
        ).register()

Project(url='http://public.ftp.planetmirror.com/pub/gimp/gimp/v1.2/v1.2.3/gimp-1.2.3.tar.bz2',
        md5='b19235f19f524f772a4aef597a69b1da *gimp-1.2.3.tar.bz2',
        configure_cmd='./configure --disable-perl',
        ).register()

Project(url=r'http://www.ibiblio.org/pub/Linux/ALPHA/wine/development/Wine-20021219.tar.gz',
        md5='0091d9173290430a98f7d772b9deb06f *Wine-20021219.tar.gz',
        unpacked_subdir='wine-20021219'
        ).register()

Project(url='http://public.planetmirror.com.au/pub/gnu/hello/hello-2.1.1.tar.gz',
        md5='70c9ccf9fac07f762c24f2df2290784d *hello-2.1.1.tar.gz',
        ).register()


# XXX: Does not build on Debian at the moment, problem with libIDL-config

# Project(url='http://mirror.aarnet.edu.au/pub/mozilla/releases/mozilla1.4/src/mozilla-source-1.4.tar.bz2',
#         name='mozilla-1.4',
#         configure_cmd="LIBIDL_CONFIG=libIDL-config-2 ./configure",
#         unpacked_subdir='mozilla',
#         ).register()


Project(url='http://ftp.mozilla.org/pub/firebird/releases/0.6/MozillaFirebird-0.6-source.tar.bz2',
        name='MozillaFirebird',
        unpacked_subdir='mozilla',
        ).register()

Project(url='http://us1.samba.org/samba/ftp/samba-3.0.7.tar.gz',
        name='samba-3.0.7',
        build_subdir='source',
        configure_cmd='./configure',
        pre_build_cmd = 'make proto', 
        ).register()
