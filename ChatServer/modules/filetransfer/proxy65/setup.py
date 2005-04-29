#!/usr/bin/env python

from distutils.core import setup

from distutils.command.install_data import install_data
class install_data_twisted(install_data):
    """make sure data files are installed in package.
    this is evil.
    copied from Twisted/setup.py.
    """
    def finalize_options(self):
        self.set_undefined_options('install',
            ('install_lib', 'install_dir')
        )
        install_data.finalize_options(self)

setup(name='Proxy65',
      version='1.0.0',
      description="JEP 65 Bytestream Proxy Component",
      author="Dave Smith",
      author_email="dizzyd@jabber.org",
      url="http://www.jabberstudio.org/projects/proxy65",
      packages=["proxy65"],
      data_files=[
    ("proxy65", ["proxy65/plugins.tml"])],
      license="GPL",
      platforms=["Linux"],
      # Don't ask, just paste.
      cmdclass={'install_data': install_data_twisted},
      long_description="""\      
      JEP 65 (http://www.jabber.org/jeps/jep-0065.html) proxy
      component. Read the JEP and you'll understand.      
      """
    )
