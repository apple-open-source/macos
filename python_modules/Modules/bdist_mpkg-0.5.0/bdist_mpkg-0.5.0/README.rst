==========
bdist_mpkg
==========

bdist_mpkg is a distutils plugin that implements the ``bdist_mpkg`` command.
The command builds a Mac OS X metapackage for use by Installer.app for easy GUI
installation of Python modules, much like ``bdist_wininst``.

It also comes with a ``bdist_mpkg`` script, which is a setup.py front-end that
will allow you to easy build an installer metapackage from nearly any existing
package that uses distutils.

Please email the `Python-Mag SIG mailing list
<http://www.python.org/community/sigs/current/pythonmac-sig/>`_ with questions,
and let us know of bugs via `github issues
<https://github.com/matthew-brett/bdist_mpkg/issues>`_

Code
====

The code started life at:

http://undefined.org/python/#bdist_mpkg

Bob Ippolito wrote most of the code.

The `current repository`_ is on Github.

.. _current repository: http://github.com/matthew-brett/bdist_mpkg

License
=======

MIT license.  See the ``LICENSE`` file in the source archive.
