Release history
===============

0.10.4
------

This is a bugfix release

* There were no 'classifiers' in the package metadata due to a bug
  in setup.py.

0.10.3
------

This is a bugfix release

Bugfixes
........

* ``modulegraph.find.modules.parse_mf_results`` failed when the main script of
  a py2app module didn't have a file name ending in '.py'.

0.10.2
------

This is a bugfix release

Bugfixes
........

* Issue #12: modulegraph would sometimes find the wrong package *__init__*
  module due to using the wrong search method. One easy way to reproduce the
  problem was to have a toplevel module named *__init__*.

  Reported by Kentzo.

0.10.1
------

This is a bugfix release

Bugfixes
........

* Issue #11: creating xrefs and dotty graphs from modulegraphs (the --xref 
  and --graph options of py2app) didn't work with python 3 due to use of
  APIs that aren't available in that version of python.

  Reported by Andrew Barnert.


0.10
----

This is a minor feature release

Features
........

* ``modulegraph.find_modules.find_needed_modules`` claimed to automaticly
  include subpackages for the "packages" argument as well, but that code
  didn't work at all.

* Issue #9: The modulegraph script is deprecated, use 
  "python -mmodulegraph" instead.

* Issue #10: Ensure that the result of "zipio.open" can be used
  in a with statement (that is, ``with zipio.open(...) as fp``.

* No longer use "2to3" to support Python 3. 

  Because of this modulegraph now supports Python 2.6
  and later.

* Slightly improved HTML output, which makes it easier
  to manipulate the generated HTML using JavaScript.

  Patch by anatoly techtonik.

* Ensure modulegraph works with changes introduced after
  Python 3.3b1.

* Implement support for PEP 420 ("Implicit namespace packages")
  in Python 3.3.

* ``modulegraph.util.imp_walk`` is deprecated and will be 
  removed in the next release of this package.

Bugfixes
........

* The module graph was incomplete, and generated incorrect warnings
  along the way, when a subpackage contained import statements for
  submodules.

  An example of this is ``sqlalchemy.util``, the ``__init__.py`` file
  for this package contains imports of modules in that modules using
  the classic relative import syntax (that is ``import compat`` to
  import ``sqlalchemy.util.compat``). Until this release modulegraph
  searched the wrong path to locate these modules (and hence failed
  to find them).


0.9.2
-----

This is a bugfix release

Bugfixes
........

* The 'packages' option to modulegraph.find_modules.find_modules ignored
  the search path argument but always used the default search path.

* The 'imp_find_modules' function in modulegraph.util has an argument 'path',
  this was a string in previous release and can now also be a sequence.

* Don't crash when a module on the 'includes' list doesn't exist, but warn
  just like for missing 'packages' (modulegraph.find_modules.find_modules)

0.9.1
-----

This is a bugfix release

Bug fixes
.........

- Fixed the name of nodes imports in packages where the first element of
  a dotted name can be found but the rest cannot. This used to create
  a MissingModule node for the dotted name in the global namespace instead
  of relative to the package.

  That is, given a package "pkg" with submodule "sub" if the "__init__.py"
  of "pkg" contains "import sub.nomod" we now create a MissingModule node
  for "pkg.sub.nomod" instead of "sub.nomod".

  This fixes an issue with including the crcmod package in application 
  bundles, first reported on the pythonmac-sig mailinglist by
  Brendan Simon.

0.9
---

This is a minor feature release


Features:

- Documentation is now generated using `sphinx <http://pypi.python.org/pypi/sphinx>`_
  and can be viewed at <http://packages.python.org/modulegraph>.

  The documention is very rough at this moment and in need of reorganisation and
  language cleanup. I've basiclly writting the current version by reading the code
  and documenting what it does, the order in which classes and methods are document
  is therefore not necessarily the most useful. 

- The repository has moved to bitbucket

- Renamed ``modulegraph.modulegraph.AddPackagePath`` to ``addPackagePath``,
  likewise ``ReplacePackage`` is now ``replacePackage``. The old name is still
  available, but is deprecated and will be removed before the 1.0 release.

- ``modulegraph.modulegraph`` contains two node types that are unused and
  have unclear semantics: ``FlatPackage`` and ``ArchiveModule``. These node
  types are deprecated and will be removed before 1.0 is released.

- Added a simple commandline tool (``modulegraph``) that will print information
  about the dependency graph of a script.

- Added a module (``zipio``) for dealing with paths that may refer to entries 
  inside zipfiles (such as source paths referring to modules in zipped eggfiles).

  With this addition ``modulegraph.modulegraph.os_listdir`` is deprecated and
  it will be removed before the 1.0 release.

Bug fixes:

- The ``__cmp__`` method of a Node no longer causes an exception
  when the compared-to object is not a Node. Patch by Ivan Kozik.

- Issue #1: The initialiser for ``modulegraph.ModuleGraph`` caused an exception
  when an entry on the path (``sys.path``) doesn't actually exist.

  Fix by "skurylo", testcase by Ronald.

- The code no longer worked with python 2.5, this release fixes that.

- Due to the switch to mercurial setuptools will no longer include
  all required files. Fixed by adding a MANIFEST.in file

- The method for printing a ``.dot`` representation of a ``ModuleGraph``
  works again.


0.8.1
-----

This is a minor feature release

Features:

- ``from __future__ import absolute_import`` is now supported

- Relative imports (``from . import module``) are now supported

- Add support for namespace packages when those are installed
  using option ``--single-version-externally-managed`` (part
  of setuptools/distribute)

0.8
---

This is a minor feature release

Features:

- Initial support for Python 3.x

- It is now possible to run the test suite
  using ``python setup.py test``.

  (The actual test suite is still fairly minimal though)
