import os
from os.path import join as pjoin, abspath
import grp
import pwd
import sys
from distutils.util import spawn
from distutils.version import StrictVersion, LooseVersion
from distutils.dir_util import mkpath
from distutils.file_util import copy_file
import distutils.core
from tempfile import mkdtemp

from .tmpdirs import InTemporaryDirectory
from .py3k import unicode


def Version(s):
    try:
        return StrictVersion(s)
    except ValueError:
        return LooseVersion(s)

def run_setup(*args, **kwargs):
    """
    Re-entrant version of distutils.core.run_setup()
    """
    PRESERVE = '_setup_stop_after', '_setup_distribution'
    d = {}
    for k in PRESERVE:
        try:
            d[k] = getattr(distutils.core, k)
        except AttributeError:
            pass
    try:
        return distutils.core.run_setup(*args, **kwargs)
    finally:
        for k,v in d.items():
            setattr(distutils.core, k, v)

def adminperms(src, verbose=0, dry_run=0):
    try:
        spawn(['/usr/bin/chgrp', '-R', 'admin', src])
        spawn(['/bin/chmod', '-R', 'g+w', src])
    except:
        return False
    return True

def mkbom(src, pkgdir, verbose=0, dry_run=0, TOOL='/usr/bin/mkbom'):
    """
    Create a bill-of-materials (BOM) for the given src directory and store it
    to the given pkg directory
    """
    dest = os.path.join(pkgdir, 'Contents', 'Archive.bom')
    mkpath(os.path.dirname(dest), verbose=verbose, dry_run=dry_run)
    spawn([TOOL, src, dest], verbose=verbose, dry_run=dry_run)

def pax(src, pkgdir, verbose=0, dry_run=0, TOOL='/bin/pax'):
    """
    Create a pax gzipped cpio archive of the given src directory and store it
    to the given pkg directory

    returns size of archive
    """
    dest = os.path.realpath(os.path.join(pkgdir, 'Contents', 'Archive.pax.gz'))
    mkpath(os.path.dirname(dest), verbose=verbose, dry_run=dry_run)
    pwd = os.path.realpath(os.getcwd())
    os.chdir(src)
    try:
        spawn([TOOL, '-w', '-f', dest, '-x', 'cpio', '-z', '.'])
    finally:
        os.chdir(pwd)
    return os.stat(dest).st_size


def find_paxboms(base_path):
    """ Generator returning pax / bom pairs from root `base_path`

    Parameters
    ----------
    base_path : str
        directory from which to search recursively for pax / bom pairs

    Yields
    ------
    px_bom : length 2 tuple
        tuple of (pax file, bom file)
    """
    for root, dirs, files in os.walk(base_path):
        for file in files:
            if not file.endswith('.bom'):
                continue
            froot = file[:-3]
            for end in ('pax', 'pax.gz'):
                if froot + end in files:
                    yield (pjoin(root, froot + end),
                           pjoin(root, file))


def ugrp_path(path):
    """ Return user and group for a path `path`
    """
    stat_info = os.stat(path)
    uid = stat_info.st_uid
    gid = stat_info.st_gid
    user = pwd.getpwuid(uid)[0]
    group = grp.getgrgid(gid)[0]
    return user, group


def unpax(pax_file, out_dir=None, TOOL='/bin/pax'):
    """ Unpack a pax archive `pax_file` into `out_dir`

    Parameters
    ----------
    pax_file : str
        filename of pax file
    out_dir : None or str, optional
        directory into which to unpack files; if None, create temporary
        directory and unpack there.  The caller should delete the directory.
    TOOL : str, optional
        path to ``pax`` binary

    Returns
    -------
    out_dir : str
        directory containing unpacked files
    """
    pwd = os.path.realpath(os.getcwd())
    pax_path = os.path.abspath(pax_file)
    cmd = [TOOL, '-r', '-p', 'e', '-f', pax_path]
    if pax_file.endswith('.gz'):
        cmd += ['-z']
    elif pax_file.endswith('.bz2'):
        cmd += ['-j']
    if out_dir is None:
        out_dir = mkdtemp
    os.chdir(out_dir)
    try:
        spawn(cmd)
    finally:
        os.chdir(pwd)
    return out_dir


def reown_paxboms(base_path, user, group, TOOL='/usr/sbin/chown'):
    """ Change ownnerhsip files in pax/boms within `base_path`

    Parameters
    ----------
    base_path : str
        path to tree somewhere containing bom / pax pairs. We will change
        ownership of files within the pax archive, and record this in the bom.
    user : str
        user to which to change ownership
    group : str
        group to which to change ownership
    """
    for pxbom in find_paxboms(base_path):
        px, bm = [abspath(f) for f in pxbom]
        with InTemporaryDirectory() as tmpdir:
            arch_path = pjoin(tmpdir, 'archive')
            os.mkdir(arch_path)
            unpax(px, arch_path)
            spawn([TOOL, '-R', '%s:%s' % (user, group), arch_path])
            os.mkdir('Contents')
            pax(arch_path, tmpdir)
            mkbom(arch_path, tmpdir)
            rs1 = copy_file(pjoin('Contents', 'Archive.bom'), bm)
            rs2 = copy_file(pjoin('Contents', 'Archive.pax.gz'), px)
            assert rs1 == (bm, True)
            assert rs2 == (px, True)


def unicode_path(path, encoding=sys.getfilesystemencoding()):
    if isinstance(path, unicode):
        return path
    return unicode(path, encoding)

def walk_files(path):
    for root, dirs, files in os.walk(path):
        for fn in files:
            yield os.path.join(root, fn)

def get_gid(name):
    """ Return integer gid for group with name `name`
    """
    return grp.getgrnam(name).gr_gid

def find_root(path, base='/'):
    """
    Return the list of files, the archive directory, and the destination path
    """
    files = list(walk_files(path))
    common = os.path.dirname(os.path.commonprefix(files))
    prefix = os.path.join(base, common[len(os.path.join(path, '')):])
    #while not os.path.exists(prefix):
    #    common = os.path.dirname(common)
    #    prefix = os.path.dirname(prefix)
    prefix = os.path.realpath(prefix)
    return files, common, prefix

def admin_writable(path):
    gid = get_gid('admin')
    while not os.path.exists(path):
        path = os.path.dirname(path)
    s = os.stat(path)
    mode = s.st_mode
    return (mode & 0x2) or (s.st_gid == gid and mode & 0x10)

def reduce_size(files):
    return sum([os.stat(fn).st_size for fn in files])

def sw_vers(_cache=[]):
    if not _cache:
        info = os.popen('/usr/bin/sw_vers').read().splitlines()
        for line in info:
            key, value = line.split(None, 1)
            if key == 'ProductVersion:':
                _cache.append(Version(value.strip()))
                break
        else:
            raise ValueError("sw_vers not behaving correctly")
    return _cache[0]

def is_framework_python():
    return os.path.dirname(os.path.dirname(sys.prefix)).endswith('.framework')
