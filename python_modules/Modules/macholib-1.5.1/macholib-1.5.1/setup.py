"""
Shared setup file for simple python packages. Uses a setup.cfg that
is the same as the distutils2 project, unless noted otherwise.

It exists for two reasons:
1) This makes it easier to reuse setup.py code between my own
   projects

2) Easier migration to distutils2 when that catches on.

Additional functionality:

* Section 'x-setup-stub': 

    distribute-version: minimal distribute version
    setuptools-version: minimal setuptools version
        (for both: script will bail out when you have a too old
         version installed, and will install the version when
         it isn't installed)
    prefer-distribute = 1  
        If true install distribute when neither setuptools
        nor distribute is present, otherwise install setuptools

* Section metadata:
    requires-test:  Same as 'tests_require' option for setuptools.

"""

import sys
import os
import re
import platform
from fnmatch import fnmatch

if sys.version_info[0] == 2:
    from ConfigParser import RawConfigParser, NoOptionError, NoSectionError
else:
    from configparser import RawConfigParser, NoOptionError, NoSectionError

ROOTDIR = os.path.dirname(os.path.abspath(__file__))

def eval_marker(value):
    """
    Evaluate an distutils2 environment marker.

    This code is unsafe when used with hostile setup.cfg files,
    but that's not a problem for our own files.
    """
    value = value.strip()

    class M:
        def __init__(self, **kwds):
            for k, v in kwds.items():
                setattr(self, k, v)

    variables = {
        'python_version': '%d.%d'%(sys.version_info[0], sys.version_info[1]),
        'python_full_version': sys.version.split()[0],
        'os': M(
            name=os.name,
        ),
        'sys': M(
            platform=sys.platform,
        ),
        'platform': M(
            version=platform.version(),
            machine=platform.machine(),
        ),
    }

    return bool(eval(value, variables, variables))


    return True

def _opt_value(cfg, into, section, key, transform = None):
    try:
        v = cfg.get(section, key)
        if transform != _as_lines and ';' in v:
            v, marker = v.rsplit(';', 1)
            if not eval_marker(marker):
                return

            v = v.strip()

        if v:
            if transform:
                into[key] = transform(v.strip())
            else:
                into[key] = v.strip()

    except (NoOptionError, NoSectionError):
        pass

def _as_bool(value):
    if value.lower() in ('y', 'yes', 'on'):
        return True
    elif value.lower() in ('n', 'no', 'off'):
        return False
    elif value.isdigit():
        return bool(int(value))
    else:
        raise ValueError(value)

def _as_list(value):
    return value.split()

def _as_lines(value):
    result = []
    for v in value.splitlines():
        if ';' in v:
            v, marker = v.rsplit(';', 1)
            if not eval_marker(marker):
                continue

            v = v.strip()
            if v:
                result.append(v)
        else:
            result.append(v)
    return result
            
def _map_requirement(value):
    m = re.search(r'(\S+)\s*(?:\((.*)\))?', value)
    name = m.group(1)
    version = m.group(2)

    if version is None:
        return name

    else:
        mapped = []
        for v in version.split(','):
            v = v.strip()
            if v[0].isdigit():
                # Checks for a specific version prefix
                m = v.rsplit('.', 1)
                mapped.append('>=%s,<%s.%s'%(
                    v, m[0], int(m[1])+1))

            else:
                mapped.append(v)
        return '%s %s'%(name, ','.join(mapped),)

def _as_requires(value):
    requires = []
    for req in value.splitlines():
        if ';' in req:
            req, marker = v.rsplit(';', 1)
            if not eval_marker(marker):
                continue
            req = req.strip()

        if not req:
            continue
        requires.append(_map_requirement(req))
    return requires

def parse_setup_cfg():
    cfg = RawConfigParser()
    r = cfg.read([os.path.join(ROOTDIR, 'setup.cfg')])
    if len(r) != 1:
        print("Cannot read 'setup.cfg'")
        sys.exit(1)

    metadata = dict(
            name        = cfg.get('metadata', 'name'),
            version     = cfg.get('metadata', 'version'),
            description = cfg.get('metadata', 'description'),
    )

    _opt_value(cfg, metadata, 'metadata', 'license')
    _opt_value(cfg, metadata, 'metadata', 'maintainer')
    _opt_value(cfg, metadata, 'metadata', 'maintainer_email')
    _opt_value(cfg, metadata, 'metadata', 'author')
    _opt_value(cfg, metadata, 'metadata', 'author_email')
    _opt_value(cfg, metadata, 'metadata', 'url')
    _opt_value(cfg, metadata, 'metadata', 'download_url')
    _opt_value(cfg, metadata, 'metadata', 'classifiers', _as_lines)
    _opt_value(cfg, metadata, 'metadata', 'platforms', _as_list)
    _opt_value(cfg, metadata, 'metadata', 'packages', _as_list)

    try:
        v = cfg.get('metadata', 'requires-dist')

    except (NoOptionError, NoSectionError):
        pass

    else:
        requires = _as_requires(v)
        if requires:
            metadata['install_requires'] = requires

    try:
        v = cfg.get('metadata', 'requires-test')

    except (NoOptionError, NoSectionError):
        pass

    else:
        requires = _as_requires(v)
        if requires:
            metadata['tests_require'] = requires


    try:
        v = cfg.get('metadata', 'long_description_file')
    except (NoOptionError, NoSectionError):
        pass

    else:
        parts = []
        for nm in v.split():
            fp = open(nm, 'rU')
            parts.append(fp.read())
            fp.close()

        metadata['long_description'] = '\n\n'.join(parts)

        
    try:
        v = cfg.get('metadata', 'zip-safe')
    except (NoOptionError, NoSectionError):
        pass

    else:
        metadata['zip_safe'] = _as_bool(v)

    try:
        v = cfg.get('metadata', 'console_scripts')
    except (NoOptionError, NoSectionError):
        pass

    else:
        if 'entry_points' not in metadata:
            metadata['entry_points'] = {}

        metadata['entry_points']['console_scripts'] = v.splitlines()

    options = {}
    _opt_value(cfg, options, 'x-setup-stub', 'distribute-version')

    if sys.version_info[:2] <= (2,6):
        try:
            metadata['tests_require'] += ", unittest2"
        except KeyError:
            metadata['tests_require'] = "unittest2"

    return metadata, options


metadata, options = parse_setup_cfg()

# XXX: Use values from 'options' to ensure
# that we have the right version of setuptoosls/distribute
# (if any: we don't actually need those when there are
# no dependencies!)
try:
    import setuptools

except ImportError:
    import distribute_setup
    distribute_setup.use_setuptools()

from setuptools import setup

try:
    from distutils.core import PyPIRCCommand
except ImportError:
    PyPIRCCommand = None # Ancient python version

from distutils.core import Command
from distutils.errors  import DistutilsError
from distutils import log

if PyPIRCCommand is None:
    class upload_docs (Command):
        description = "upload sphinx documentation"
        user_options = []

        def initialize_options(self):
            pass

        def finalize_options(self):
            pass

        def run(self):
            raise DistutilsError("not supported on this version of python")

else:
    class upload_docs (PyPIRCCommand):
        description = "upload sphinx documentation"
        user_options = PyPIRCCommand.user_options

        def initialize_options(self):
            PyPIRCCommand.initialize_options(self)
            self.username = ''
            self.password = ''


        def finalize_options(self):
            PyPIRCCommand.finalize_options(self)
            config = self._read_pypirc()
            if config != {}:
                self.username = config['username']
                self.password = config['password']


        def run(self):
            import subprocess
            import shutil
            import zipfile
            import os
            import urllib
            import StringIO
            from base64 import standard_b64encode
            import httplib
            import urlparse

            # Extract the package name from distutils metadata
            meta = self.distribution.metadata
            name = meta.get_name()

            # Run sphinx
            if os.path.exists('doc/_build'):
                shutil.rmtree('doc/_build')
            os.mkdir('doc/_build')

            p = subprocess.Popen(['make', 'html'],
                cwd='doc')
            exit = p.wait()
            if exit != 0:
                raise DistutilsError("sphinx-build failed")

            # Collect sphinx output
            if not os.path.exists('dist'):
                os.mkdir('dist')
            zf = zipfile.ZipFile('dist/%s-docs.zip'%(name,), 'w', 
                    compression=zipfile.ZIP_DEFLATED)

            for toplevel, dirs, files in os.walk('doc/_build/html'):
                for fn in files:
                    fullname = os.path.join(toplevel, fn)
                    relname = os.path.relpath(fullname, 'doc/_build/html')

                    print ("%s -> %s"%(fullname, relname))

                    zf.write(fullname, relname)

            zf.close()

            # Upload the results, this code is based on the distutils
            # 'upload' command.
            content = open('dist/%s-docs.zip'%(name,), 'rb').read()
            
            data = {
                ':action': 'doc_upload',
                'name': name,
                'content': ('%s-docs.zip'%(name,), content),
            }
            auth = "Basic " + standard_b64encode(self.username + ":" +
                 self.password)


            boundary = '--------------GHSKFJDLGDS7543FJKLFHRE75642756743254'
            sep_boundary = '\n--' + boundary
            end_boundary = sep_boundary + '--'
            body = StringIO.StringIO()
            for key, value in data.items():
                if not isinstance(value, list):
                    value = [value]

                for value in value:
                    if isinstance(value, tuple):
                        fn = ';filename="%s"'%(value[0])
                        value = value[1]
                    else:
                        fn = ''

                    body.write(sep_boundary)
                    body.write('\nContent-Disposition: form-data; name="%s"'%key)
                    body.write(fn)
                    body.write("\n\n")
                    body.write(value)

            body.write(end_boundary)
            body.write('\n')
            body = body.getvalue()

            self.announce("Uploading documentation to %s"%(self.repository,), log.INFO)

            schema, netloc, url, params, query, fragments = \
                    urlparse.urlparse(self.repository)


            if schema == 'http':
                http = httplib.HTTPConnection(netloc)
            elif schema == 'https':
                http = httplib.HTTPSConnection(netloc)
            else:
                raise AssertionError("unsupported schema "+schema)

            data = ''
            loglevel = log.INFO
            try:
                http.connect()
                http.putrequest("POST", url)
                http.putheader('Content-type',
                    'multipart/form-data; boundary=%s'%boundary)
                http.putheader('Content-length', str(len(body)))
                http.putheader('Authorization', auth)
                http.endheaders()
                http.send(body)
            except socket.error:
                e = socket.exc_info()[1]
                self.announce(str(e), log.ERROR)
                return

            r = http.getresponse()
            if r.status in (200, 301):
                self.announce('Upload succeeded (%s): %s' % (r.status, r.reason),
                    log.INFO)
            else:
                self.announce('Upload failed (%s): %s' % (r.status, r.reason),
                    log.ERROR)

                print ('-'*75) 
                print (r.read())
                print ('-'*75)


def recursiveGlob(root, pathPattern):
    """
    Recursively look for files matching 'pathPattern'. Return a list
    of matching files/directories.
    """
    result = []

    for rootpath, dirnames, filenames in os.walk(root):
        for fn in filenames:
            if fnmatch(fn, pathPattern):
                result.append(os.path.join(rootpath, fn))
    return result
        

def importExternalTestCases(unittest, 
        pathPattern="test_*.py", root=".", package=None):
    """
    Import all unittests in the PyObjC tree starting at 'root'
    """

    testFiles = recursiveGlob(root, pathPattern)
    testModules = map(lambda x:x[len(root)+1:-3].replace('/', '.'), testFiles)
    if package is not None:
        testModules = [(package + '.' + m) for m in testModules]

    suites = []
   
    for modName in testModules:
        try:
            module = __import__(modName)
        except ImportError:
            print("SKIP %s: %s"%(modName, sys.exc_info()[1]))
            continue

        if '.' in modName:
            for elem in modName.split('.')[1:]:
                module = getattr(module, elem)

        s = unittest.defaultTestLoader.loadTestsFromModule(module)
        suites.append(s)

    return unittest.TestSuite(suites)



class test (Command):
    description = "run test suite"
    user_options = [
        ('verbosity=', None, "print what tests are run"),
    ]

    def initialize_options(self):
        self.verbosity='1'

    def finalize_options(self):
        if isinstance(self.verbosity, str):
            self.verbosity = int(self.verbosity)


    def cleanup_environment(self):
        ei_cmd = self.get_finalized_command('egg_info')
        egg_name = ei_cmd.egg_name.replace('-', '_')

        to_remove =  []
        for dirname in sys.path:
            bn = os.path.basename(dirname)
            if bn.startswith(egg_name + "-"):
                to_remove.append(dirname)

        for dirname in to_remove:
            log.info("removing installed %r from sys.path before testing"%(
                dirname,))
            sys.path.remove(dirname)

    def add_project_to_sys_path(self):
        from pkg_resources import normalize_path, add_activation_listener
        from pkg_resources import working_set, require

        self.reinitialize_command('egg_info')
        self.run_command('egg_info')
        self.reinitialize_command('build_ext', inplace=1)
        self.run_command('build_ext')


        # Check if this distribution is already on sys.path
        # and remove that version, this ensures that the right
        # copy of the package gets tested.

        self.__old_path = sys.path[:]
        self.__old_modules = sys.modules.copy()


        ei_cmd = self.get_finalized_command('egg_info')
        sys.path.insert(0, normalize_path(ei_cmd.egg_base))
        sys.path.insert(1, os.path.dirname(__file__))

        # Strip the namespace packages defined in this distribution
        # from sys.modules, needed to reset the search path for
        # those modules.

        nspkgs = getattr(self.distribution, 'namespace_packages')
        if nspkgs is not None:
            for nm in nspkgs:
                del sys.modules[nm]
        
        # Reset pkg_resources state:
        add_activation_listener(lambda dist: dist.activate())
        working_set.__init__()
        require('%s==%s'%(ei_cmd.egg_name, ei_cmd.egg_version))

    def remove_from_sys_path(self):
        from pkg_resources import working_set
        sys.path[:] = self.__old_path
        sys.modules.clear()
        sys.modules.update(self.__old_modules)
        working_set.__init__()


    def run(self):
        import unittest

        # Ensure that build directory is on sys.path (py3k)

        self.cleanup_environment()
        self.add_project_to_sys_path()

        try:
            meta = self.distribution.metadata
            name = meta.get_name()
            test_pkg = name + "_tests"
            suite = importExternalTestCases(unittest, 
                    "test_*.py", test_pkg, test_pkg)

            runner = unittest.TextTestRunner(verbosity=self.verbosity)
            result = runner.run(suite)

            # Print out summary. This is a structured format that
            # should make it easy to use this information in scripts.
            summary = dict(
                count=result.testsRun,
                fails=len(result.failures),
                errors=len(result.errors),
                xfails=len(getattr(result, 'expectedFailures', [])),
                xpass=len(getattr(result, 'expectedSuccesses', [])),
                skip=len(getattr(result, 'skipped', [])),
            )
            print("SUMMARY: %s"%(summary,))

        finally:
            self.remove_from_sys_path()

setup(
    cmdclass=dict(
        upload_docs=upload_docs,
        test=test,
    ),
    **metadata
)
