#!/usr/bin/python3
"""Common routines to share."""
import os
import logging
import plistlib
import zlib
from uuid import uuid4
from datetime import datetime
from subprocess import run
from lib.constants import SUCCESS, SKIP
from lib.ntfs_dmg import DMG_DATA

Success_count = 0
Fail_count = 0
Skip_count = 0


# ============================================
#        General Routines
# ============================================

def docmd_common(cmd):
    """Run cmd, return output and error."""
    logging.info('cmd> {}'.format(cmd))
    result = run(cmd, capture_output=True)
    out = result.stdout
    errmsg = result.stderr
    err = result.returncode
    if err:
        logging.error(out.decode("utf-8"))
        logging.error(errmsg.decode("utf-8"))
    else:
        logging.debug(out.decode("utf-8"))
        logging.debug(errmsg.decode("utf-8"))
    return {'out': out, 'err': err}


def docmd_stdout(cmd):
    """Run a command, return stdout and error code.

    Arguments:
        cmd (str):        shell command to run

    Returns:
        result (dict):    {'out': stdout, 'err': error-code}
    """
    result = docmd_common(cmd)
    result['out'] = result['out'].decode("utf-8")
    return result


def docmd_plist(cmd):
    """Run cmd, return plist if succeed, empty string if fail.

    Arguments:
        cmd (str):        shell command to run

    Returns:
        plist (dict):     plist if succeed
                          '' (empty string) if fail
    """
    result = docmd_common(cmd)
    if result['err'] == 0:
        plist = plistlib.loads(result['out'])
        return plist
    else:
        return ''


def docmd(cmd, should_fail=False):
    """Run a command, return 0 if as expected, non-0 if not as expected.

    Arguments:
        cmd (str):        shell command to run
        should_fail (boolean): False - expect the command to succeed
                               True - expect the command to fail
    Returns:
        0  if result is as expected
        non-0 if result is not as expected
    """
    result = docmd_common(cmd)
    err = result['err']
    if err and should_fail:
        logging.info('This failure is expected.')
        err = 0
    return err


def print_to_terminal():
    """Set logging to print to the terminal."""
    terminal_output = logging.StreamHandler()
    logging.getLogger().addHandler(terminal_output)
    terminal_output.setLevel(logging.INFO)


def startup(testdir, logfile, debug=False):
    """Start up tasks before running all the tests.

    Arguments:
        testdir (str):     directory for putting files/dirs needed for testing
        logfile (str):     file for logging
        debug (boolean):   True if '-d' option is specified
    """
    cmd = ['/bin/mkdir', '-p', testdir]
    run(cmd)
    # Logging levels: debug, info, warning, error, critical
    logging.basicConfig(format='%(asctime)s %(levelname)s %(message)s',
                        datefmt='%m/%d/%Y %H:%M:%S', filename=logfile,
                        level=logging.DEBUG)
    if debug:
        print_to_terminal()
    start_time = datetime.now()
    logging.info('TESTING Starts at {}'.format(start_time))


def teardown(logfile, debug=False):
    """Teardown tasks after running all the tests.

    Arguments:
        logfile (str):     file for logging
        debug (boolean):   True if '-d' option is specified
    """
    now = datetime.now()
    logging.info('TESTING Finishes at {}'.format(now))
    summary = '------------------------------------'
    summary += f'\nTotal tests PASS={Success_count}, '\
               f'FAIL={Fail_count}, SKIP={Skip_count}'
    summary += f'\nLog file: {logfile}'
    logging.info(summary)
    if not debug:
        print(summary)
    if Fail_count > 0:
        exit(1)
    else:
        exit(0)


def test_begin(testname, description, debug=False):
    """For each test case, begin with this.

    Arguments:
        testname (str):    name of the test
        description (str): description of the test
        debug (boolean):   True if '-d' option is specified
    """
    logging.info('TEST Begins: ' + testname + ' (' + description + ')')
    if not debug:
        print(testname + ' ... ', end='')


def test_end(testname, err, debug=False):
    """For each test case, end with this.

    Arguments:
        testname (str):    name of the test
        err (int):         test result - SUCCESS, FAIL, or SKIP
        debug (boolean):   True if '-d' option is specified
    """
    global Success_count, Fail_count, Skip_count

    if err == SUCCESS or err == 0:
        result = ' PASS '
        Success_count = Success_count + 1
    elif err == SKIP:
        result = ' SKIP '
        Skip_count = Skip_count + 1
    else:
        result = ' FAIL '
        Fail_count = Fail_count + 1
    logging.info('TEST Ends: ' + testname + result + '\n')
    if not debug:
        print(result)


def is_macos():
    """Check if the machine processor architecture is for macOS."""
    result = docmd_stdout(["uname", "-p"])
    out = result['out'].strip('\n')
    if out == 'arm' or out == 'i386':
        return True
    else:
        logging.error("This is not a macOS.")
        return False


def is_userfs_enabled(fstype):
    """Check if userfs is enabled for this file system type.

    Arguments:
        fstype (str):       filesystem type, e.g. NTFS
    Return:
        True (boolean):     userfs is enabled for this fs type
        False (boolean):    userfs is not enabled for this fs type

    """
    fsdir = '/System/Library/Filesystems/' + fstype.lower() + '.fs'
    ilist = fsdir + '/Contents/Info.plist'
    if docmd(["grep", "UserFS", ilist]) == 0:
        return True
    logging.error(f"UserFS is not enabled for {fstype}.")
    return False


def mntpt_to_device(mntpt):
    """Get the device name for the given mountpoint.

    Arguments:
        mntpt (str):   filesystem mount point, e.g. /Volumes/test

    Returns:
        device (str):  device of the mounted filesystem, e.g. /dev/disk2s3
    """
    cmd = ["/usr/sbin/diskutil", "info", "-plist", mntpt]
    plist = docmd_plist(cmd)
    if 'DeviceNode' in plist:
        dev = plist['DeviceNode']
    else:
        logging.error('Unrecognized DeviceNode plist format')
        dev = None
    return dev


def device_to_mntpt(device):
    """Get the mountpoint for the given device name.

    Arguments:
        device (str):  device of the mounted filesystem, e.g. /dev/disk2s3

    Returns:
        mntpt (str):   filesystem mount point, e.g. /Volumes/test
    """
    plist = docmd_plist(["/usr/sbin/diskutil", "info", "-plist", device])
    if 'MountPoint' in plist:
        mntpt = plist['MountPoint']
    else:
        mntpt = ''
    return mntpt


def hdiutil_attach(dmg, nomount=False, mntpt=None):
    """Run 'hdiutil attach -plist dmg', and return disk/fs device name.

    Arguments:
        dmg (str):  disk image to be attached
        nomount (boolean):  default is to mount
                            True is not to mount
        mntpt (str):  specify a different mount point than the default

    Returns:
        diskdev (str): disk device name, e.g. /dev/disk3
        fsdev (str):  file system device name, e.g. /dev/disk3s1
        fsmntpt (str):  file system mount point, e.g. /Volumes/test

    """
    if dmg is None:
        logging.error('Fail: no disk image to be attached')
        return None, None, None

    cmd = ["hdiutil", "attach"]
    if nomount:
        cmd.append("-nomount")
    if mntpt:
        cmd.extend(["-mountpoint", mntpt])
    cmd.extend(["-plist", dmg])
    plist = docmd_plist(cmd)
    if plist == '':
        logging.error('Fail: no plist from hdiutil attach')
        return None, None, None

    entity_array = plist['system-entities']
    diskdev = None
    fsdev = None
    fsmntpt = None
    for entity in entity_array:
        if entity['content-hint'] == 'GUID_partition_scheme' or\
           entity['content-hint'] == 'FDisk_partition_scheme':
            diskdev = entity['dev-entry']
        elif entity['content-hint'] == 'Apple_APFS' or\
                entity['content-hint'] == 'Apple_HFS' or\
                entity['content-hint'] == 'Windows_NTFS' or\
                entity['content-hint'] == 'DOS_FAT_32':
            fsdev = entity['dev-entry']
            if 'mount-point' in entity:
                fsmntpt = entity['mount-point']

    if None in [diskdev, fsdev]:
        logging.error('Fail: no disk device nor fs device is identified')
    return diskdev, fsdev, fsmntpt


def syscall_mount(fstype, fsdev):
    """Mount via mount(8)->mount(2) syscall.

    Arguments:
        fstype (str):  filesystem type, e.g. 'ntfs'
        fsdev (str):   filesystem device name, e.g. /dev/disk5s2

    Returns:
        0 if succeed
        non-o if fail

    """
    mntpt = '/Volumes/' + fstype.lower() + str(uuid4())
    if not os.path.exists(mntpt):
        err = docmd(["sudo", "mkdir", mntpt])
        if err:
            logging.error(f"Fail to mkdir {mntpt}")
            return err
    mountcmd = ["sudo", "mount", "-t", fstype, fsdev, mntpt]
    return docmd(mountcmd)


def syscall_unmount(dev=None, mntpt=None):
    """Unmount via umount(8)->umount(2) syscall.

    Arguments:
        dev (str):     filesystem device name, e.g. /dev/disk5s2
        mntpt (str):   filesystem mount point, e.g. /Volumes/ntfsvol

    Returns:
        0 if succeed
        non-0 if fail

    """
    if dev is not None:
        return docmd(["sudo", "umount", dev])
    elif mntpt is not None:
        return docmd(["sudo", "umount", mntpt])
    else:
        raise RuntimeError("Missing argument value")


def diskutil_mount(fsdev, read_only=False, no_browse=False, mount_opts=None,
                   mount_point=None):
    """Mount this File System using diskutil.

    Arguments:
        fsdev: filesystem device, e.g. /dev/disk2s1
        read_only (bool): whether or not to mount the file system
            as read only. Default is False
        no_browse (bool): whether or not to mount the file system
            as no browse. Default is False
        mount_opts (list): list of strings to pass to the correct
            mount call. Default is None, e.g. ['rdonly']
        mount_point (str): path to the directory to mount on.
            Default is None
    Return:
        0 for success
        non-0 for failure

    """
    cmd = ["diskutil", "mount"]
    if read_only:
        cmd.append("readOnly")
    if no_browse:
        cmd.append("nobrowse")
    if mount_opts:
        cmd.extend(["-mountOptions", mount_opts])
    if mount_point:
        cmd.extend(["-mountPoint", mount_point])
    cmd.append(fsdev)
    err = docmd(cmd)
    return err


def diskutil_unmount(fsdev):
    """Unmount this File System using diskutil.

    Arguments:
        fsdev: filesystem device, e.g. /dev/disk2s1

    Return:
        0 for success
        non-0 for failure

    """
    err = docmd(["diskutil", "unmount", fsdev])
    return err


def convert_text_to_dmg(dmg_file):
    """Convert a compressed text file to a dmg file.

    Arguments:
        dmg_file (file):  convert ntfs_dmg.py text file to this disk image name
    """
    with open(dmg_file, 'wb') as out_file:
        out_file.write(zlib.decompress(DMG_DATA))
