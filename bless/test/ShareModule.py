#! /usr/bin/python3
#
# Copyright (c) 2020 Apple Inc. All rights reserved.
#
# Updated from Python 2 to Python 3 by Casey Hafley
#
# Common API shared between test_bless and test_bless2
#
# XXX This code is derived from test_asr, mainly to use SOURCE class
# for setting up dummy boot volumes for blessing. Still have target
# volume footprint that can be cleaned up.
#

import sys
import os
import logging
import plistlib
import uuid
import re
import collections
import time
import glob
from subprocess import Popen, PIPE, call
from datetime import datetime
from os import listdir
from os.path import expanduser
import ShareVars

BootPlist = 'com.apple.Boot.plist'

# LOG file for each test_bless2 program run on the system
tuuid = uuid.uuid4()
LOGDIR = '/Library/Logs/test_bless/'
TESTDIR = LOGDIR + str(tuuid) + '/'
LOGFILE = TESTDIR + 'test_bless.log'
BATSLOG = LOGDIR + 'bats_bless_presubmit.log'
VOLDIR = '/Volumes/'

# SRCDATA = '/usr/lib'
# SRCDATA = '/usr/local/bin'
# XXX change the folder to avoid issues caused by 64184194
SRCDATA = '/System/Applications/Utilities'
SRCIMG = TESTDIR + 'bless_source.dmg'
SRCVOL = 'test_src' + str(os.getpid())
SRCOSIMG = TESTDIR + 'bless_source_os.dmg'
SRCOSVOL = 'test_src_osvol' + str(os.getpid())
SRCROSVIMG = TESTDIR + 'bless_source_rosv.dmg'
SRCROSVDATAVOL = 'test_src_rosvdata' + str(os.getpid())
SRCROSVSYSVOL = 'test_src_rosvsys' + str(os.getpid())

HOLDMNTFILE = '/test_bless_holdmntptfile'
MTREEOUTFILE = TESTDIR + 'bless_mtree.out'

TYPE_SRC_IMG = 1  # Source is a dummy disk image
TYPE_SRC_VOL = 2  # Source is a dummy volume
TYPE_SRC_NAME = 3  # Source is the given name/path
TYPE_SRC_OSIMG = 4  # Source is a dummy OS image to be created
TYPE_SRC_OSVOL = 5  # Source is a dummy OS volume to be created
TYPE_SRC_ROSVIMG = 6  # Source is a dummy ROSV volume-group image to be created
TYPE_SRC_ROSV = 7  # Source is a dummy ROSV volume-group to be created
TYPE_SRC_SNAPVOL = 8  # Source is a volume with multiple snapshots
TYPE_SRC_SNAPIMG = 9  # Source is an image with multiple snapshots
TYPE_SRC_SNAPROSV = 10  # Source is an ROSV volume-group with multiple snapshots
TYPE_SRC_SNAPROSVIMG = 11  # Source is an ROSV diskimage with multiple snapshots
TYPE_SRC_HFSIMG = 12  # Source is a HFS image

SIZE_SRCDATA = '3g'

start_time4all = 0
start_time4each = 0
succeed_count = 0
fail_count = 0
skip_count = 0
known_fail_count = 0
unclean_count = 0
check_performance = 0

#
# Test name convention
#
TNAME_ENCRYPT = 'encrypt'

#
# /SWE macOS Images available in
#    /SWE/Teams/CoreOS/Images/prod/ASR/
# or /SWE/Teams/MSQ/macOS/asr/
# /SWE XCODE Images available in
#    /SWE/Xcode/Images/
#
OSDIR = '/SWE/Teams/CoreOS/Images/prod/ASR/'
OSDIR_GOLDENGATE = OSDIR + 'GoldenGate/'
OSDIR_JAZZ = OSDIR + 'macOSJazz/'
OSDIR_LIB = OSDIR + 'macOSLiberty/'
XCODEDIR = '/SWE/Xcode/Images/macOS_iOS_tvOS_watchOS_bridgeOS/macOSJazz_Yukon_Yager_Grace_bridgeOSJazz/standard-ui-sdk-internal/'

#
# Local Images location and names
#
DMGDIR = '/tmp/test_bless/'
SWEDMGname = {OSDIR_GOLDENGATE: '', OSDIR_JAZZ: '', OSDIR_LIB: '', XCODEDIR: ''}

#
# apfs_sealvolume location
#
APFS_SEALER = '/System/Library/Filesystems/apfs.fs/Contents/Resources/apfs_sealvolume'

#
# For bless2, Apple Silicon systems
#
RESTORE_DIR = '/restore/'


# ================================================
#    HELPER FUNCTIONS
# ================================================

#
# docmd_common without printing out the command line
#
def docmd_common_noprint(cmd):
    result = Popen(cmd, stdout=PIPE, stderr=PIPE)
    try:
        out, errmsg = result.communicate()

        out = out.strip(b'\n')
        errmsg = errmsg.strip(b'\n')

        out_decoded = bytes.decode(out)
        errmsg_decoded = bytes.decode(errmsg)

    except:
        print('Command interrupted...')
        exit(1)

    err = result.returncode

    # # Log command output messages
    if err and cmd[0] != '/sbin/umount' and cmd[0] != '/bin/rm' and cmd[0] != '/bin/ls':
        logging.error(out_decoded)
        logging.error(errmsg_decoded)
    else:

        logging.info(out_decoded)
        logging.info(errmsg_decoded)

    # Print result output messages
    if (
        cmd[0] == '/usr/sbin/asr'
        or cmd[0] == '/usr/local/bin/xia'
        or cmd[0] == '/usr/bin/time'
        or cmd[0] == 'time'
        or (
            err
            and cmd[0] != '/sbin/umount'
            and cmd[0] != '/bin/rm'
            and cmd[0] != '/bin/ls'
        )
    ):
        print(out_decoded)
        print(errmsg_decoded)

    return out, err


#
# Common routine for docmd*
#
def docmd_common(cmd):
    logging.info('call: {}'.format(cmd))
    print('call: {}'.format(cmd))
    out, err = docmd_common_noprint(cmd)
    return out, err


#
# Run cmd, return 0 for success, non-zero for failure
#
def docmd(cmd):
    out, err = docmd_common(cmd)
    return err


#
# Run cmd, return plist for success, error number for failure
#
def docmd_plist(cmd):
    out, err = docmd_common(cmd)
    if err == 0:
        # Success, return plist
        plist = plistlib.loads(out)
        return plist, 0
    else:
        return out, err


#
# Startup tasks before running all the tests
#
def Startup():
    global start_time4all

    cmd = ['/bin/mkdir', '-p', TESTDIR]
    call(cmd)
    cmd = ['/bin/mkdir', '-p', DMGDIR]
    call(cmd)

    logging.basicConfig(
        format='%(asctime)s %(levelname)s (%(funcName)s) %(message)s',
        datefmt='%m/%d/%Y %H:%M:%S',
        filename=LOGFILE,
        level=logging.DEBUG,
    )
    start_time4all = datetime.now()

    logging.info('\n\nTESTING BEGINS at {}'.format(start_time4all))
    print('\nTESTING Begins at {}'.format(start_time4all))
    docmd_common_noprint(['/usr/bin/sw_vers'])


#
# Teardown tasks after running all the tests
#
def Teardown():
    global succeed_count
    global fail_count
    global known_fail_count
    global skip_count
    global unclean_count
    global check_performance

    now = datetime.now()
    time_elapsed = now - start_time4all

    logging.info('TESTING ENDS at {}'.format(now))
    print('TESTING ENDS at {}'.format(now))

    logging.info('---------------------------------------------------------------')
    print('---------------------------------------------------------------')

    if known_fail_count:
        logging.info(
            'Total number of tests SUCCEED=%d, FAIL=%d (KNOWN=%d), SKIP=%d',
            succeed_count,
            fail_count,
            known_fail_count,
            skip_count,
        )
        print(
            'Total number of tests SUCCEED=%d, FAIL=%d (KNOWN=%d), SKIP=%d'
            % (succeed_count, fail_count, known_fail_count, skip_count)
        )
    else:
        logging.info(
            'Total number of tests SUCCEED=%d, FAIL=%d, SKIP=%d',
            succeed_count,
            fail_count,
            skip_count,
        )
        print(
            'Total number of tests SUCCEED=%d, FAIL=%d, SKIP=%d'
            % (succeed_count, fail_count, skip_count)
        )

    if unclean_count > 0:
        logging.info(
            'Total number of tests UNCLEAN=%d (Please clean up manually) ',
            unclean_count,
        )
        print(
            'Total number of tests UNCLEAN=%d (Please clean up manually)'
            % unclean_count
        )
    logging.info('Total running time {}  (h:m:s.ms)'.format(time_elapsed))
    print('Total running time {} (h:m:s.ms)'.format(time_elapsed))
    logging.info('Log information in %s\n', LOGFILE)
    print('Log information in %s\n' % LOGFILE)

    if check_performance == 1:
        logging.info('Check PERFORMANCE DATA in the Log file\n')
        print('Check PERFORMANCE DATA in the Log file')

    call(['/bin/cp', LOGFILE, BATSLOG])

    if fail_count > 0:
        exit(1)
    else:
        exit(0)


#
# Check access permission
#
def checkaccess(dir):
    err = docmd(['/bin/ls', '-d', dir])
    if err:
        logging.error('Can not access ' + dir)
        print('Can not access ' + dir)
    return err


#
# For each test case, begin wth this
#
def testBegin(testname):
    global start_time4each
    logging.info('\n\nTEST ' + testname + ' Begin...')
    print('\n\nTEST ' + testname + ' Begin...')
    start_time4each = datetime.now()


#
# For each test case, end with this
#
def testEnd(testname, src, tgt, err, ErrorPause):
    global succeed_count
    global fail_count
    global known_fail_count
    global start_time4each

    if testname.startswith('neg_'):
        if err:
            err = 0
        else:
            err = 1

    # pause here on error and wait for user input before clean up
    if ErrorPause and err:
        input = raw_input(
            "Test paused on error. Enter 'x' to exit or any other key to continue:"
        )
        if input == 'x':
            exit()

    if src:
        src.release()
    if tgt:
        tgt.release()

    time_elapsed = datetime.now() - start_time4each

    exist = os.path.isfile(MTREEOUTFILE)
    if exist:
        docmd(['/bin/rm', '-f', MTREEOUTFILE])

    if err:
        logging.info(
            'TEST '
            + testname
            + ' FAIL  '
            + '(elapsed-time {})'.format(time_elapsed)
            + '\n'
        )
        print(
            'TEST '
            + testname
            + ' FAIL  '
            + '(elapsed-time {})'.format(time_elapsed)
            + '\n'
        )
        fail_count = fail_count + 1
        if 'bug-' in testname:
            known_fail_count = known_fail_count + 1
    else:
        logging.info(
            'TEST '
            + testname
            + ' SUCCEED '
            + '(elapsed-time {})'.format(time_elapsed)
            + '\n'
        )
        print(
            'TEST '
            + testname
            + ' SUCCEED '
            + '(elapsed-time {})'.format(time_elapsed)
            + '\n'
        )
        succeed_count = succeed_count + 1


#
# Check if this is an Apple Silicon system
#
def check_arm():
    result = Popen(['/usr/bin/uname', '-p'], stdout=PIPE, stderr=PIPE)
    out, errmsg = result.communicate()
    if out.startswith(b'arm'):
        return True
    return False


#
# Clean up all log directories in LOGDIR
#
def log_cleanup():
    cmd = ['/bin/rm', '-rf', LOGDIR]
    call(cmd)
    cmd = ['/bin/rm', '-rf', DMGDIR]
    call(cmd)


#
# XXX let the caller specify a specific image name: /path/to/image_name?
#
# Get the latest dmg name derived from the given dir
# (OS)
# % ls -lt <dir> | awk '/.AppleInternal.dmg/ {print $9}' | awk 'NR==1 {print $1}'
# (XCODE)
# % ls -lt <dir> | awk '/.dmg/ {print $9}' | awk 'NR==1 {print $1}'
#
def get_dmg_shortname(dir):
    err = checkaccess(dir)
    if err:
        return ''
    try:
        cmd = ['/bin/ls', '-lt', dir]
        ls = Popen(cmd, stdout=PIPE)
        if 'Xcode' in dir:
            cmd = "/usr/bin/awk '/.dmg/ {print $9}'"
        else:
            cmd = "/usr/bin/awk '/.AppleInternal.dmg/ {print $9}'"
        awk1 = Popen(cmd, stdin=ls.stdout, stdout=PIPE, shell=True)
        cmd = "/usr/bin/awk 'NR==1 {print $1}'"
        awk2 = Popen(cmd, stdin=awk1.stdout, stdout=PIPE, shell=True)
        out, err = awk2.communicate()
        if err:
            return ''
        out = out.strip(b'\n')
        out = bytes.decode(out)
        logging.info('get_dmg_shortname: ' + out)
        return out
    except:
        logging.error('get_dmg_shortname Failed')
        print('get_dmg_shortname Failed')
        return ''


#
# Copy and get remote OS dmg or XCODE dmg file from the given dir
# to a local directory
#
# If LocalMacOS is True, have macOS ASR images available in $HOME/Downloads/test_asr/,
# e.g.
#    % ls $HOME/Downloads/test_asr/*
#    /Users/smadmin/Downloads/test_asr/Golden:
#       Golden20A284UMIA_ASR_NFA.AppleInternal.dmg
#    /Users/smadmin/Downloads/test_asr/macOSJazz:
#       macOSJazz19A603.AppleInternal.dmg
#    /Users/smadmin/Downloads/test_asr/macOSLiberty:
#       macOSLiberty18A391.AppleInternal.dmg
#    /Users/smadmin/Downloads/test_asr/Xcode:
#       Xcode11M392v_m18A391011_m19A603_i17A878_t17J586_w17R605_b17P572_FastSim_Boost_43GB.dmg
#
def get_dmg_name(testname, dir):
    global SWEDMGname, skip_count
    global LocalMacOS

    # If a local dmg file is already copied for this test run, use it.
    if SWEDMGname[dir] != '':
        return SWEDMGname[dir]

    name = get_dmg_shortname(dir)
    if name == '':
        skip_count = skip_count + 1
        logging.info('SKIP test %s\n', testname)
        print('SKIP test ' + testname)
        return ''

    if LocalMacOS:
        local_dmg = dir + name
    else:
        remote_dmg = dir + name
        local_dmg = DMGDIR + name
        # If this file is not in local DMGDIR, copy it over
        err = docmd(['/bin/ls', local_dmg])
        if err:
            err = docmd(['/bin/cp', remote_dmg, local_dmg])
            if err:
                # in case it's partially copied, remove it
                docmd(['/bin/rm', local_dmg])
                local_dmg = ''
                skip_count = skip_count + 1
                logging.info('SKIP test %s\n', testname)
                print('SKIP test ' + testname)
    SWEDMGname[dir] = local_dmg
    return local_dmg


#
# Get Volume UUID for the given volume device name
#
def DeviceToUUID(dev):
    cmd = ['/usr/sbin/diskutil', 'info', '-plist', dev]
    plist, err = docmd_plist(cmd)
    try:
        uuid = plist['VolumeUUID']
    except:
        logging.info('Unrecognized VolumeUUID plist format')
        uuid = ''
    return uuid


#
# Get System Volume device in the Group Volume of the given container
# that can map to the given sysdevi.
# e.g. return /dev/disk2s1 for a given snapshot sysdev /dev/disk2s1s1
#
def getSystemVolumeDev(container, sysdev):
    cmd = ['/usr/sbin/diskutil', 'apfs', 'listgroups', '-plist', container]
    plist, err = docmd_plist(cmd)
    if err:
        logging.info('Can not listgroups for %s', container)
        return ''

    c_array = plist['Containers']
    for c_entry in c_array:
        vol_grps = c_entry['VolumeGroups']
        if vol_grps != '':
            for entry in vol_grps:
                for vol in entry['Volumes']:
                    devID = '/dev/' + vol['DeviceIdentifier']
                    if vol['Role'] == 'System' and sysdev.startswith(devID):
                        return devID
    logging.info('Can not get System Volume device for %s', sysdev)
    print('Can not get System Volume device for ' + sysdev)
    return ''


#
# Get Volume Group UUID for the System Volume in the given Container
#
def DeviceToGroupUUID(container, sysdev):
    cmd = ['/usr/sbin/diskutil', 'apfs', 'listgroups', '-plist', container]
    plist, err = docmd_plist(cmd)
    if err:
        logging.info('Can not listgroups for %s', container)
        return ''

    c_array = plist['Containers']
    for c_entry in c_array:
        vol_grps = c_entry['VolumeGroups']
        if vol_grps != '':
            for entry in vol_grps:
                for vol in entry['Volumes']:
                    devID = '/dev/' + vol['DeviceIdentifier']
                    if vol['Role'] == 'System' and sysdev.startswith(devID):
                        guuid = entry['APFSVolumeGroupUUID']
                        logging.info('GroupUUID for %s is %s', sysdev, guuid)
                        print('GroupUUID for ' + sysdev + ' is ' + guuid)
                        return guuid
    logging.info('Can not get GroupUUID for %s', sysdev)
    print('Can not get GroupUUID for ' + sysdev)
    return ''


#
# Check if the given UUID is a Volume Group UUID in this Container
#
def isGroupUUID(container, check_uuid):
    cmd = ['/usr/sbin/diskutil', 'apfs', 'listgroups', '-plist', container]
    plist, err = docmd_plist(cmd)
    if err:
        logging.info('Can not listgroups for %s', container)
        return ''

    c_array = plist['Containers']
    for c_entry in c_array:
        vol_grps = c_entry['VolumeGroups']
        if vol_grps != '':
            for entry in vol_grps:
                guuid = entry['APFSVolumeGroupUUID']
                if guuid == check_uuid:
                    return True
    return False


#
# Check if the given 'Vdev' has the given 'role'.
# Return True if it does, False if it does not.
#
def CheckVdevRole(Cdev, Vdev, role):
    plist, err = docmd_plist(['/usr/sbin/diskutil', 'apfs', 'list', '-plist', Cdev])
    if err:
        return ''
    C_array = plist['Containers']
    C_dict = C_array[0]
    V_array = C_dict['Volumes']
    for entry in V_array:
        roles_array = entry['Roles']
        if roles_array and roles_array[0] == role:
            roleVdev = '/dev/' + entry['DeviceIdentifier']
            if Vdev == roleVdev:
                return True
    return False


#
# Get the device with the matching 'role' in the given Container.
# Role can be 'Preboot' or 'Recovery' or 'System', etc.
#
def SysCntToRoleVdev(Cdev, role):
    # Find the device with the matching role
    plist, err = docmd_plist(['/usr/sbin/diskutil', 'apfs', 'list', '-plist', Cdev])
    if err:
        return ''
    C_array = plist['Containers']
    C_dict = C_array[0]
    V_array = C_dict['Volumes']
    for entry in V_array:
        roles_array = entry['Roles']
        if (len(roles_array) == 1 and roles_array[0] == role) or (
            len(roles_array) == 0 and role == ''
        ):
            return '/dev/' + entry['DeviceIdentifier']
    return ''


#
# Get device name for the given volume name
#
def MntptToDevice(volume):
    cmd = ['/usr/sbin/diskutil', 'info', '-plist', volume]
    plist, err = docmd_plist(cmd)
    try:
        dev = plist['DeviceNode']
    except:
        logging.info('Unrecognized DeviceNode plist format')
        dev = ''
    return dev


#
# Get the mountpoint for the given device name
#
def DeviceToMntpt(device):
    docmd(['/usr/sbin/diskutil', 'mount', device])
    cmd = ['/usr/sbin/diskutil', 'info', '-plist', device]
    plist, err = docmd_plist(cmd)
    try:
        mntpt = plist['MountPoint']
    except:
        logging.info('Unrecognized MountPoint plist format for %s', device)
        print('Unrecognized MountPoint plist format for ' + device)
        mntpt = ''
    return mntpt


#
# Get the APFS Container devname for the given dev
#
def DeviceToContainer(dev):
    cmd = ['/usr/sbin/diskutil', 'info', '-plist', dev]
    plist, err = docmd_plist(cmd)
    try:
        Cdev = plist['APFSContainerReference']
    except:
        logging.info('Unrecognized APFS Container plist format')
        Cdev = ''
    return '/dev/' + Cdev


#
# Get a set of System/Data device name in a given container,
# where it's not the current ROSV root.
#
# Assumption: only 1 non-root volume-group in the given container
#
def ContainerToSysData(Cdev):
    sysdev = ''
    datadev = ''

    if Cdev == '':
        return '', ''

    plist, err = docmd_plist(
        ['/usr/sbin/diskutil', 'apfs', 'listGroups', '-plist', Cdev]
    )
    if err:
        return sysdev, datadev

    rootsys = MntptToDevice('/')
    rootdata = MntptToDevice('/System/Volumes/Data')
    C_array = plist['Containers']
    C_dict = C_array[0]
    VG_array = C_dict['VolumeGroups']
    for entry in VG_array:
        Volumes = entry['Volumes']
        for vol in Volumes:
            dev = '/dev/' + vol['DeviceIdentifier']
            if dev == rootsys or dev == rootdata:
                continue
            if vol['Role'] == 'System':
                sysdev = dev
            elif vol['Role'] == 'Data':
                datadev = dev
        if sysdev != '' and datadev != '':
            break
    return sysdev, datadev


#
# Wait for encrypting a volume to finish
#
def wait_for_encrypt(Cdev, vol):
    start_time = time.time()
    while 1:
        elapse_time = time.time() - start_time
        # wait upto 300 seconds
        if elapse_time > 300:
            logging.error('Fail: Timeout waiting for encryption to finish')
            print('Fail: Timeout waiting for encryption to finish')
            return 1
        plist, err = docmd_plist(['/usr/sbin/diskutil', 'apfs', 'list', '-plist', Cdev])
        if err:
            logging.error('Fail at waiting for encryption to finish')
            print('Fail at waiting for encryption to finish')
            logging.error('Fail to wait for encryption to finish')
            print('Fail to wait for encryption to finish')
            return err
        C_array = plist['Containers']
        C_dict = C_array[0]
        V_array = C_dict['Volumes']
        for entry in V_array:
            Vdev = '/dev/' + entry['DeviceIdentifier']
            if Vdev == vol:
                inProgress = entry['CryptoMigrationOn']
                if inProgress == False:
                    # encryption is done
                    return 0
        time.sleep(5)


#
# Print the device information associated with Source or Target
#
def print_dev_info(obj):
    logging.info(obj.__class__.__name__ + ' Disk Image devname = ' + obj.diskimgdev)
    logging.info(
        obj.__class__.__name__ + ' APFS Container devname (apfsCdev) = ' + obj.apfsCdev
    )
    logging.info(
        obj.__class__.__name__ + ' APFS Volume devname (apfsVdev) = ' + obj.apfsVdev
    )
    logging.info(
        obj.__class__.__name__
        + ' APFS Volume mountpoint (apfsVmntpt) = '
        + obj.apfsVmntpt
    )
    logging.info(
        obj.__class__.__name__
        + ' APFS Data Volume devname (apfsDVdev) = '
        + obj.apfsDVdev
    )
    logging.info(
        obj.__class__.__name__
        + ' APFS Data Volume mountpoint (apfsDVmntpt) = '
        + obj.apfsDVmntpt
    )
    logging.info(
        obj.__class__.__name__ + ' HFS Volume devname (hfsdev) = ' + obj.hfsdev
    )
    logging.info('')
    print(' Disk Image devname = ' + obj.diskimgdev)
    print(' APFS Container devname (apfsCdev) = ' + obj.apfsCdev)
    print(' APFS Volume devname (apfsVdev) = ' + obj.apfsVdev)
    print(' APFS Volume mountpoint (apfsVmntpt) = ' + obj.apfsVmntpt)
    print(' APFS Data Volume devname (apfsDVdev) = ' + obj.apfsDVdev)
    print(' APFS Data Volume mountpoint (apfsDVmntpt) = ' + obj.apfsDVmntpt)
    print(' HFS Volume devname (hfsdev) = ' + obj.hfsdev)
    print('')


#
# Attach a disk image, and store the following information in obj:
#    Disk Image devname, obj.diskimgdev (e.g. /dev/disk3)
#    APFS Container vdevname, obj.apfsCdev (/dev/disk3s2 v.s./dev/disk4)
#    APFS Volume devname, obj.apfsVdev (e.g. /dev/disk4s1)
#    APFS Volume mountpoint, obj.apfsVmntpt (e.g. /Volumes/apfsvolume)
#
def do_attach(obj, dmg):
    plist, err = docmd_plist(['/usr/bin/hdiutil', 'attach', '-plist', dmg])
    if err:
        return err

    # log some information after the attach
    docmd(['/usr/sbin/diskutil', 'list'])
    docmd(['/sbin/mount'])

    Vmntpt = ''
    Vdev = ''
    entity_array = plist['system-entities']
    for entity in entity_array:
        if entity['content-hint'] == 'Apple_HFS':
            obj.hfsdev = entity['dev-entry']
        elif entity['content-hint'] == 'GUID_partition_scheme':
            obj.diskimgdev = entity['dev-entry']
        elif entity['content-hint'] == 'Apple_APFS':
            obj.apfsCdev_phys = entity['dev-entry']
            obj.apfsCdev = DeviceToContainer(obj.apfsCdev_phys)
        elif 'mount-point' in entity and entity['volume-kind'] == 'apfs':
            Vmntpt = entity['mount-point']
            Vdev = entity['dev-entry']

    # Done if not APFS nor having APFS container
    if obj.hfsdev != '' or obj.apfsCdev == '':
        print_dev_info(obj)
        return 0

    # If dmg is APFS ROSV image, can get System and Data Volume device name
    obj.apfsVdev, obj.apfsDVdev = ContainerToSysData(obj.apfsCdev)
    if obj.apfsVdev != '' and obj.apfsDVdev != '':
        obj.apfsVmntpt = DeviceToMntpt(obj.apfsVdev)
        obj.apfsDVmntpt = DeviceToMntpt(obj.apfsDVdev)
    elif obj.apfsVdev == '' and obj.apfsDVdev == '':
        # If dmg is not an ROSV, use Vdev/Vmntpt from the attach plist
        obj.apfsVdev = Vdev
        obj.apfsVmntpt = Vmntpt
    else:
        logging.info('do_attach: something is not right')
        print('do_attach: something is not right')
        err = 1

    print_dev_info(obj)
    return err


#
# Check if the given source image has a GroupUUID dir in Preboot
# If so, flag it so that it can be verified on the target
#
def checkGroupUUID(src):
    err = do_attach(src, src.name)
    if err:
        logging.info('checkGroupUUID: Failed to attach')
        print('checkGroupUUID: Failed to attach')
        return

    preboot_dev = SysCntToRoleVdev(src.apfsCdev, 'Preboot')
    if preboot_dev == '':
        logging.error('No Preboot device in container %s', src.apfsCdev)
        print('No Preboot device in container %s' + src.apfsCdev)
    else:
        group_uuid = DeviceToGroupUUID(src.apfsCdev, src.apfsVdev)
        sys_uuid = DeviceToUUID(src.apfsVdev)
        if group_uuid == '':
            logging.info('Source image has NO GroupUUID')
            print('Source image has NO GroupUUID')
        elif group_uuid != sys_uuid:
            mntpt = DeviceToMntpt(preboot_dev)
            err = docmd(['/bin/ls', '-ld', mntpt + '/' + group_uuid])
            if err == 0:
                src.flag |= FLAG_SRC_PREBOOT_GroupUUID
                logging.info('Source image has GroupUUID in Preboot')
                print('Source image has GroupUUID in Preboot')
            else:
                logging.info('Source image does NOT have GroupUUID in Preboot')
                print('Source image does NOT have GroupUUID in Preboot')
            docmd(['/usr/sbin/diskutil', 'umount', preboot_dev])

    docmd(['hdiutil', 'detach', src.diskimgdev])
    src.apfsCdev = ''
    src.apfsVdev = ''


#
# Get nvram value for the given string name
#
def get_nvram_value(name):
    nvram = Popen(['/usr/sbin/nvram', name], stdout=PIPE)
    cmd = "/usr/bin/awk 'NR==1 {print $2}'"
    value = Popen(cmd, stdin=nvram.stdout, stdout=PIPE, shell=True)
    out, err = value.communicate()
    if err:
        return ''
    out = out.strip(b'\n')
    out = bytes.decode(out)
    logging.info('get_nvram_value: %s = %s', name, out)
    print('get_nvram_value: ' + name + ' = ' + out)
    return out


#
# check if a file or folder exits
# 'path' is the full path of the folder or file
# return True if exists, False otherwise
#
def checkFileDirExist(path):
    result = Popen(['ls', path], stdout=PIPE, stderr=PIPE)
    result.communicate()
    if result.returncode == 0:
        logging.info("File/Dir '" + path + "' found")
    else:
        logging.error("File/Dir '" + path + "' not found")

    return result.returncode == 0


#
# check if all the strings in 'strs' can be found in 'src
# print out log message
#
def verifyCmdOutputForStrngs(errorMessage, expectedStrings, inputDesc):
    # breakpoint()
    if errorMessage is None or len(errorMessage) <= 0:
        logging.error(f'{inputDesc} failed. Input string is empty')
        return False

    if type(errorMessage) is bytes:
        errorMessage = bytes.decode(errorMessage)

    logging.info(f'{inputDesc}: {errorMessage}')

    for expectedString in expectedStrings:
        if type(expectedString) is bytes:
            expectedString = bytes.decode(expectedString)

        logging.info(f'{inputDesc} for content "{expectedString}"')
        if errorMessage.find(expectedString) < 0:
            logging.error(
                f'Expected string "{expectedString}" not found in error message'
                f' {errorMessage}'
            )
            return False
    return True


#
# remove the special chars in 'chars' from 'src
#
def remSpecialChars(src, chars):
    for i in chars:
        src = src.replace(i, '')
    return src


#
# return a unique string for disk or volume or image name
# the name will start with 'BLS'
#
def getUniqName():
    cmd = ['mktemp', '-t', 'TEMPNAME']
    out, err = docmd_common(cmd)
    out = out.strip(b'\n')
    out = bytes.decode(out)
    ind = out.find('TEMPNAME')
    name = None
    if ind >= 0:
        name = "BLS" + out[(ind + 9) :]
    logging.info("UniqueName created: " + name)
    return name


#
# create a volume in the container with specific role
# return
# 1. returncode: 0=success, 1=fail
# 2. the volume's disk name
# 3. the volume's name
#
def addVolumeToContainer(containerDisk, role):
    containerVolName = getUniqName()

    # cmd = ['diskutil', 'ap', 'addvolume', containerDisk, 'apfs', containerVolName, '-role', role]
    # docmd(cmd)

    process = Popen(
        [
            'diskutil',
            'apfs',
            'addvolume',
            containerDisk,
            'apfs',
            containerVolName,
            '-role',
            role,
        ],
        stdout=PIPE,
        stderr=PIPE,
    )
    out, err = process.communicate()
    containerVolDisk = re.findall(b'Created new APFS Volume (disk\d+s\d+)', out)
    containerVolDisk = bytes.decode(containerVolDisk[0])
    return 0, containerVolDisk, containerVolName


#
# create a container on 'volDis' and return
# 1. returncode. 0=success, 1=fail
# 2. the container's disk name
# 3. the container's volume1's disk name
# 4. the container's volume1's name
#
def createContainer(imgDisk, volDisk):
    # volDisk is a volume identifier
    # imgDisk is a container identifier

    containerVol1Name = getUniqName()

    process = Popen(
        ['diskutil', 'apfs', 'create', volDisk, containerVol1Name],
        stdout=PIPE,
        stderr=PIPE,
    )
    out, err = process.communicate()

    containerDisk = re.findall(b'Created new APFS Container (disk\d+)', out)
    containerVol1Disk = re.findall(b'Created new APFS Volume (disk\d+s\d+)', out)
    containerDisk = bytes.decode(containerDisk[0])
    containerVol1Disk = bytes.decode(containerVol1Disk[0])
    return 0, containerDisk, containerVol1Disk, containerVol1Name


# ===========================================================
#   MAKE DISKIMAGE/CONTAINER/VOLUME for SOURCE
# ===========================================================

#
# Get a copy of com.apple.Boot.plist file from root's Recovery volume,
# and copy it to a new location.
#
def setupBootPlist(new_uuid):
    #
    # Find com.apple.Boot.plist in root's Recovery volume
    #
    root_dev = MntptToDevice('/')
    root_Cdev = DeviceToContainer(root_dev)
    root_recovdev = SysCntToRoleVdev(root_Cdev, 'Recovery')
    root_recovmntpt = DeviceToMntpt(root_recovdev)
    dir_entries = listdir(root_recovmntpt)
    root_uuid = ''
    for root_uuid in dir_entries:
        if len(root_uuid) == 36:
            break  # the first UUID entry name
    root_bpfile = root_recovmntpt + '/' + root_uuid + '/' + BootPlist
    err = docmd(['/bin/ls', '-l', root_bpfile])
    if err:
        docmd(['/usr/sbin/diskutil', 'unmount', root_recovdev])
        return ''

    new_bpfile = TESTDIR + BootPlist
    # Copy root's com.apple.Boot.plist to the new location
    err = docmd(['/bin/cp', root_bpfile, new_bpfile])
    if err == 0:
        # Edit the com.apple.Boot.plist file with the new uuid
        re = 's/' + root_uuid + '/' + new_uuid + '/g'
        err = docmd(['/usr/bin/sed', '-i', '-e', re, new_bpfile])

    docmd(['/usr/sbin/diskutil', 'unmount', root_recovdev])
    if err == 0:
        return new_bpfile
    else:
        return ''


#
# Setup LocalPolicy:
#    for external disk: in /System/Volumes/iSCPreboot/<uuid>
#    for internal disk: in Preboot
#
# This test program uses diskimage as the backing store which is
# treated as an external disk.
#
# Use 'personalize_macos' to generate a personalized
# apticket.<model>.<ECID>.im4m, e.g apticket.j313ap.1565260A00011E.im4m
# which will be stored in /Volumes/TestSystem/restore/ directory
#    % personalize_macos -v /Volumes/TestSystem
#
# With the apticket, use bpgen to generate a LocalPolicy for external disk:
# (the apticket should match the img4 files planned for stitching)
#     % bpgen localpolicy -i <apticket> -d /System/Volumes/iSCPreboot/<uuid>/LocalPolicy'
#
# Also, need bootpolicy entitlement set to true:
#     com.apple.private.security.bootpolicy
#
def setupLocalPolicy(src, prebmnt, uuid):
    docmd(
        [
            'defaults',
            'write',
            '/Library/Preferences/com.apple.security.bootpolicy',
            'Entitlements',
            '-string',
            'always',
        ]
    )

    err = docmd(
        [
            '/usr/local/bin/personalize_macos',
            '-v',
            src.apfsVmntpt,
            '--output',
            src.apfsVmntpt,
        ]
    )
    if err:
        return err

    fromdir = src.apfsVmntpt + RESTORE_DIR
    apticket = ''
    for file in glob.glob(fromdir + '*'):
        if 'apticket' in file:
            apticket = file
            break
    if apticket == '':
        logging.error('Fail to get apticket file in %s', fromdir)
        print('Fail to get apticket file in ' + fromdir)
        return 1
    restoredir = prebmnt + '/' + uuid + RESTORE_DIR
    docmd(['/bin/mkdir', '-p', restoredir])
    err = docmd(['/bin/cp', '-p', apticket, restoredir])
    if err:
        logging.error('Fail to copy %s to %s', apticket, restoredir)
        print('Fail to copy ' + apticket + ' to ' + restoredir)
        return err

    docmd(['/bin/mkdir', '-p', '/tmp/' + uuid + '/LocalPolicy'])
    docmd(['/usr/local/bin/atomicCopy', '/tmp/' + uuid, '/System/Volumes/iSCPreboot'])
    docmd(['/bin/rm', '-rf', '/tmp/' + uuid])
    lpdir = '/System/Volumes/iSCPreboot/' + uuid + '/LocalPolicy'
    err = docmd(['/usr/local/bin/bpgen', 'localpolicy', '-i', apticket, '-d', lpdir])
    if err:
        logging.error('Fail to generate LocalPolciy in %s', lpdir)
        print('Fail to generate LocalPolciy in ' + lpdir)
        return 1

    strlist = apticket.split('/')
    apticket_fname = strlist[len(strlist) - 1]
    ShareVars.bless2_osmanifestpath = '/' + uuid + RESTORE_DIR + apticket_fname
    return 0


#
# Setup 'raw' files needed to feed the libbless2 APIs for boot volume
# on Apple Silicon
#
def setupBless2Files(src, mntpt, the_uuid):
    #
    # Copy files from /System/Volumes/Preboot/<group_uuid> to the
    # new Preboot mount point as the "raw" files for the testing boot volume.
    #
    root_dev = MntptToDevice('/')
    root_Cdev = DeviceToContainer(root_dev)
    root_group_uuid = DeviceToGroupUUID(root_Cdev, root_dev)
    root_prebdir = '/System/Volumes/Preboot/' + root_group_uuid
    ShareVars.bless2_rootCdev = root_Cdev
    ShareVars.bless2_rootVdev = root_dev

    #
    # Copy /System/Volumes/Preboot/<root_guuid>/boot/<active-nsih#>/usr/standalone
    # to <preboot>/<uuid>/usr/
    # Files in ./standalone/firmware/:
    #    iBoot.img4, root_hash.img4, base_system_root_hash.img4, etc.
    #
    active_file = root_prebdir + '/boot/active'
    nsih, err = docmd_common(['/bin/cat', active_file])
    if err:
        return err
    nsih = nsih.strip(b'\n')
    nsih = bytes.decode(nsih)
    fromdir = root_prebdir + '/boot/' + nsih + '/usr/standalone'
    todir = mntpt + '/' + the_uuid + '/usr/'
    docmd(['/bin/mkdir', '-p', todir])
    err = docmd(['/bin/cp', '-R', fromdir, todir])
    if err:
        return err

    #
    # copy /System/Volumes/Preboot/<root_guuid>/restore
    # to <preboot>/<uuid>/restore,
    # but not copy apticket* files (will be generated thru setupLocalPoicy)
    # Files: BuildManifest.plist, Firmware/*.im4p (payload files), etc.
    #
    fromdir = root_prebdir + RESTORE_DIR
    todir = mntpt + '/' + the_uuid + RESTORE_DIR
    docmd(['/bin/mkdir', '-p', todir])
    for file in glob.glob(fromdir + '*'):
        if 'apticket' in file:
            continue
        elif os.path.isdir(file):
            err = docmd(['/bin/cp', '-R', file, todir])
        else:
            err = docmd(['/bin/cp', file, todir])
    if err:
        return err

    #
    # copy /System/Volumes/Preboot/<root_guuid>/boot/System/Library
    # to <preboot>/<uuid>/System/boot/System
    # Files: KernelCollections/*
    #
    fromdir = root_prebdir + '/boot/System/Library'
    todir = mntpt + '/' + the_uuid + '/boot/System/'
    docmd(['/bin/mkdir', '-p', todir])
    err = docmd(['/bin/cp', '-R', fromdir, todir])
    if err:
        return err

    #
    # Set up LocalPolicy with newly created personalized apticket
    #
    err = setupLocalPolicy(src, mntpt, the_uuid)
    if err:
        return err

    return 0


#
# Add Preboot and Recovery Volumes to a container using ERB
#
def add_preb_recov_volumes(src):
    err = docmd(['/usr/local/bin/dmtest', 'erb', src.apfsVdev])
    if err:
        return err

    #
    # For Preboot Volume:
    #
    # Check if erb creates a Volume Group UUID dir or System Volume UUID dir.
    # This is for a new source image, there should be only 1 UUID-dir
    #
    prebvdev = SysCntToRoleVdev(src.apfsCdev, 'Preboot')
    prebmntpt = DeviceToMntpt(prebvdev)
    dir_entries = listdir(prebmntpt)
    the_uuid = ''
    for the_uuid in dir_entries:
        break  # just need the first and only entry name
    if the_uuid == '':
        logging.error('No UUID entry, ERB error?')
        print('No UUID entry, ERB error?')
        return 1
    is_group = isGroupUUID(src.apfsCdev, the_uuid)
    if is_group:
        src.flag |= FLAG_SRC_PREBOOT_GroupUUID

    #
    # Copy /usr/standalone/i386/boot* (include *.im4m) to Preboot volume
    # ./Preboot/<the_uuid>/System/Library/CoreServices/
    #
    todir = prebmntpt + '/' + the_uuid + '/System/Library/CoreServices/'
    for file in glob.glob('/usr/standalone/i386/boot*'):
        err = docmd(['/bin/cp', file, todir])
        if err:
            logging.error('Fail to copy boot file: %s', file)
            print('Fail to copy boot file: ' + file)
            return err

    #
    # For bless2, Apple Silicon systems
    #
    isArm = check_arm()
    if isArm == True:
        err = setupBless2Files(src, prebmntpt, the_uuid)
        if err:
            logging.error('setupBless2Files Fail')
            print('setupBless2Files Fail')
            return err
        ShareVars.bless2_guuid = the_uuid
        ShareVars.bless2_prebVdev = prebvdev
        ShareVars.bless2_srcdirpath = '/' + the_uuid + RESTORE_DIR
    docmd(['/usr/sbin/diskutil', 'unmount', prebvdev])

    #
    # For Recovery Volume:
    #
    # Find com.apple.Boot.plist file in '/' root's Recovery volume,
    # if there is one, copy it to TESTDIR, and replace its uuid with
    # the uuid created by 'dmtest erb'.
    # If there is none, that's okay, no setup is needed.
    #
    bpfile = setupBootPlist(the_uuid)
    if bpfile == '':
        logging.error('setupBootPlist Fail')
        print('setupBootPlist Fail')
        return 0

    # Copy the new BootPlist file to the new Recovery/<the_uuid>/ directory
    recovdev = SysCntToRoleVdev(src.apfsCdev, 'Recovery')
    recovmntpt = DeviceToMntpt(recovdev)
    todir = recovmntpt + '/' + the_uuid
    err = docmd(['/bin/cp', bpfile, todir])
    docmd(['/usr/sbin/diskutil', 'unmount', recovdev])
    docmd(['/bin/rm', bpfile + '-e'])
    docmd(['/bin/rm', bpfile])
    return err


#
# Seal the given source volume/device
# Encryption and Sealing do not co-exist on a System Volume
#
def seal_source(src):
    if src.flag & FLAG_SRC_Encrypt:
        logging.error('Encryption and Sealing do not co-exist on a System Volume')
        print('Encryption and Sealing do not co-exist on a System Volume')
        return 1

    if src.name != src.apfsVdev and src.name != src.apfsDVdev:
        docmd(['/usr/sbin/diskutil', 'mount', src.name])
        sealdev = MntptToDevice(src.name)
        src.name = sealdev
    if src.apfsVdev != '':
        docmd(['/usr/sbin/diskutil', 'unmount', src.apfsVdev])
    if src.apfsDVdev != '':
        docmd(['/usr/sbin/diskutil', 'unmount', src.apfsDVdev])

    if src.flag & FLAG_SRC_SSV:
        err = docmd([APFS_SEALER, '-T', src.name])
    elif src.flag & FLAG_SRC_SSV_SNAP:
        err = docmd([APFS_SEALER, '-s', src.snapsys[SNAP1][SNAME], src.name])
    else:
        err = 1
        logging.error('Something is not right')
        print('Something is not right')

    if err:
        logging.error('Fail to seal the source %s, err=%d', src.name, err)
        print('Fail to seal the source ' + src.name + ' err = %d\n' % err)
    return err


#
# Make a dummy single volume based on a diskimage
#
def make_dummy_vol(obj, size, content, vname, imgname, fstype):
    if content != '':
        err = docmd(
            [
                '/usr/bin/hdiutil',
                'create',
                '-size',
                size,
                '-srcfolder',
                content,
                '-fs',
                fstype,
                '-format',
                'UDRW',
                '-volname',
                vname,
                '-ov',
                imgname,
            ]
        )
    else:
        err = docmd(
            [
                '/usr/bin/hdiutil',
                'create',
                '-size',
                size,
                '-fs',
                fstype,
                '-volname',
                vname,
                '-ov',
                imgname,
            ]
        )
    if err:
        return err

    # attach the source image
    err = do_attach(obj, imgname)
    if err:
        return err

    if obj.flag & FLAG_SRC_Encrypt:
        # Enable encryption
        err = docmd(
            [
                '/usr/sbin/diskutil',
                'apfs',
                'enableFileVault',
                obj.apfsVdev,
                '-user',
                'disk',
                '-passphrase',
                'disk',
            ]
        )
        if err:
            return err

    if fstype == 'APFS':
        obj.name = obj.apfsVdev
    elif fstype == 'HFS+':
        obj.name = obj.hfsdev
    docmd(['/bin/mkdir', '-p', obj.apfsVmntpt + '/var/tmp/'])
    docmd(['/bin/chmod', '777', obj.apfsVmntpt + '/var/tmp/'])
    return err


#
# Make a dummy data image and scan it as the source
#
def make_dummy_img(src, fstype):
    err = docmd(
        [
            '/usr/bin/hdiutil',
            'create',
            '-size',
            SIZE_SRCDATA,
            '-srcfolder',
            SRCDATA,
            '-fs',
            fstype,
            '-format',
            'UDZO',
            '-volname',
            SRCVOL,
            '-ov',
            SRCIMG,
        ]
    )
    if err:
        return err

    # scan the source image
    err = docmd(['/usr/sbin/asr', 'imagescan', '-s', SRCIMG])
    if err:
        return err
    src.name = SRCIMG
    return err


#
# make a dummy hfs disk image
#
def make_dummy_hfsimg(src):
    return make_dummy_img(src, 'HFS+')


#
# Copy some files to Source object
#
def fill_os_data(src):
    dirusr = '/usr/'
    dirsl = '/System/Library/'
    dircs = '/System/Library/CoreServices/'
    bootfiles = dircs + 'boot*'
    svfile = dircs + 'SystemVersion.plist'
    dirtmp = '/var/tmp/'

    #
    # In /usr/standalone, there are bootcaches.plist, i386/, firmware/
    #
    todir = src.apfsVmntpt + '/usr'
    docmd(['/bin/mkdir', '-p', todir])
    docmd(['/bin/cp', '-R', dirusr + 'standalone', todir])

    # /System/Library/Caches
    todir = src.apfsVmntpt + dirsl
    docmd(['/bin/mkdir', '-p', todir])
    docmd(['/bin/cp', '-R', dirsl + 'Caches', todir])

    # /System/Library/CoreServices/boot*
    todir = src.apfsVmntpt + dircs
    docmd(['/bin/mkdir', '-p', todir])
    for file in glob.glob(bootfiles):
        docmd(['/bin/cp', file, todir])
    docmd(['/bin/cp', svfile, todir])

    docmd(['/bin/mkdir', '-p', src.apfsVmntpt + dirtmp])
    docmd(['/bin/chmod', '777', src.apfsVmntpt + dirtmp])


#
# Make a dummy OS Container/Volume filled with basic system files from the root Volume
#
def make_dummy_osvol(src):
    err = docmd(
        [
            '/usr/bin/hdiutil',
            'create',
            '-size',
            SIZE_SRCDATA,
            '-fs',
            'APFS',
            '-volname',
            SRCOSVOL,
            '-ov',
            SRCIMG,
        ]
    )
    if err:
        return err

    err = do_attach(src, SRCIMG)
    if err:
        return err

    if src.flag & FLAG_SRC_Encrypt:
        # Enable encryption
        err = docmd(
            [
                '/usr/sbin/diskutil',
                'apfs',
                'enableFileVault',
                src.apfsVdev,
                '-user',
                'disk',
                '-passphrase',
                'disk',
            ]
        )
        if err:
            return err

    fill_os_data(src)

    # Add Preboot and Recovery Volumes
    err = add_preb_recov_volumes(src)
    src.name = src.apfsVmntpt

    return err


#
# Make a dummy OS disk image as the Source
#
def make_dummy_osimg(src):
    err = make_dummy_osvol(src)
    if err:
        return err
    docmd(['/usr/sbin/diskutil', 'unmount', src.apfsVdev])
    src.name = SRCOSIMG
    err = docmd(
        ['/usr/bin/hdiutil', 'create', '-srcdevice', src.apfsCdev, '-ov', src.name]
    )
    if err:
        return err
    err = docmd(['/usr/sbin/asr', 'imagescan', '-s', src.name])
    return err


#
# Map snapshot name to its UUID for the given volume
#
def snap_nametouuid(vol, snapname):
    plist, err = docmd_plist(
        ['/usr/sbin/diskutil', 'apfs', 'listSnapshots', '-plist', vol]
    )
    if not err:
        snap_array = plist['Snapshots']
        for s_entry in snap_array:
            if s_entry['SnapshotName'] == snapname:
                return s_entry['SnapshotUUID']
    return ''


#
# Initialize snapshot values
#
SNAME = 0
SFILE = 1
SUUID = 2


def init_snap_values(snap, name, file, uuid):
    snap[SNAME] = name
    snap[SFILE] = file
    snap[SUUID] = uuid


#
# Create one snapshot for the given source mountpoint and store its UUID information
#
def make_one_snapshot(src, snapentry, mntpt):
    err = docmd(['/usr/bin/touch', mntpt + snapentry[SFILE]])
    if err == 0:
        err = docmd(['/usr/local/bin/apfs_snapshot', '-c', snapentry[SNAME], mntpt])
    if err == 0:
        snapuuid = snap_nametouuid(mntpt, snapentry[SNAME])
        if snapuuid != '':
            snapentry[SUUID] = snapuuid
        else:
            err = 1
    return err


#
# Setup snapshots on the Source for Snapshot Testing
#
def make_snapshots(src):
    err = make_one_snapshot(src, src.snapsys[SNAP1], src.apfsVmntpt)
    if err == 0:
        err = make_one_snapshot(src, src.snapsys[SNAP2], src.apfsVmntpt)
    if err == 0:
        err = make_one_snapshot(src, src.snapsys[SNAP3], src.apfsVmntpt)
    if err == 0:
        err = make_one_snapshot(src, src.snapsys[SNAPDIFF], src.apfsVmntpt)
    if err == 0:
        err = make_one_snapshot(src, src.snapsys[SNAPLIVE], src.apfsVmntpt)
    docmd(['/usr/local/bin/apfs_snapshot', '-l', src.apfsVmntpt])

    # If source is ROSV
    if err == 0 and src.apfsDVmntpt != '':
        err = make_one_snapshot(src, src.snapdata[SNAP1], src.apfsDVmntpt)
        if err == 0:
            err = make_one_snapshot(src, src.snapdata[SNAP2], src.apfsDVmntpt)
        if err == 0:
            err = make_one_snapshot(src, src.snapdata[SNAP3], src.apfsDVmntpt)
        if err == 0:
            err = make_one_snapshot(src, src.snapdata[SNAPDIFF], src.apfsDVmntpt)
        if err == 0:
            err = make_one_snapshot(src, src.snapdata[SNAPLIVE], src.apfsDVmntpt)
        docmd(['/usr/local/bin/apfs_snapshot', '-l', src.apfsDVmntpt])

    return err


#
# Make a dummy OS APFS volume with multiple snapshots
#
def make_dummysnap_osvol(src):
    err = make_dummy_osvol(src)
    if err:
        return err

    # setup snapshots
    err = make_snapshots(src)
    return err


#
# Make a dummy disk image contianing a APFS volume with multiple snapshots
#
def make_dummysnap_osimg(src):
    err = make_dummysnap_osvol(src)
    if err:
        return err

    # create dmg
    err = docmd(['/usr/sbin/diskutil', 'unmount', src.apfsVdev])
    if err:
        return err
    src.name = SRCOSIMG
    err = docmd(
        ['/usr/bin/hdiutil', 'create', '-srcdevice', src.apfsCdev, '-ov', src.name]
    )
    if err:
        return err
    err = docmd(['/usr/sbin/asr', 'imagescan', '-s', src.name])
    return err


#
# Make a dummy APFS Container with a Read-Only System Volume (Role: System)
# and a Data Volume (Role: Data)
#
def make_dummy_rosv(obj, size, sysvol, datavol, imgname):
    #
    # Create the Data Volume first because diskutil -groupWith works this way
    #
    err = docmd(
        [
            '/usr/bin/hdiutil',
            'create',
            '-size',
            size,
            '-fs',
            'APFS',
            '-volname',
            datavol,
            '-ov',
            imgname,
        ]
    )
    if err:
        return err

    err = do_attach(obj, imgname)
    if err:
        return err

    # Set 'Data' role for the 'Data' volume to be in a volume-group
    err = docmd(['/usr/sbin/diskutil', 'apfs', 'chrole', obj.apfsVdev, 'D'])
    if err:
        return err

    # Save the vdev and mountpoint information for the Data Volume
    obj.apfsDVdev = obj.apfsVdev
    obj.apfsDVmntpt = obj.apfsVmntpt

    #
    # Create a 'System' volume to be in the same volume-group as 'Data' volume
    #
    err = docmd(
        [
            '/usr/sbin/diskutil',
            'apfs',
            'addVolume',
            obj.apfsCdev,
            'APFS',
            sysvol,
            '-role',
            'S',
            '-groupWith',
            obj.apfsVdev,
        ]
    )
    if err:
        return err

    # Update the vdev and mountpoint information for the System Volume
    obj.apfsVmntpt = VOLDIR + sysvol
    obj.apfsVdev = MntptToDevice(obj.apfsVmntpt)
    obj.name = obj.apfsVdev

    #
    # Fill in some data in System-Volume
    #
    fill_os_data(obj)

    # Fill in some data in Data-Volume
    dirusers = '/Users'
    docmd(['/bin/mkdir', '-p', obj.apfsDVmntpt + dirusers])
    docmd(['/bin/cp', '/etc/passwd', obj.apfsDVmntpt + dirusers])
    docmd(['/bin/chmod', '600', obj.apfsDVmntpt + dirusers + '/passwd'])

    # Create the 'firmlinks' file in /usr/share on the System-Volume
    docmd(['/bin/mkdir', '-p', obj.apfsVmntpt + '/usr/share/'])
    flnk_name = obj.apfsVmntpt + '/usr/share/' + 'firmlinks'
    newf = open(flnk_name, 'w+')
    newf.write('/Users\t    Users\n')
    newf.close()

    #
    # Create firmlinks from System-Volume directory to Data-Volume directory
    # % sudo apfsctl firmlink -s 1 Users /Volumes/system-volume/Users
    #        (target_path 'Users' is relative to data-volume root, e.g.
    #         /Volumes/data-volume/ or /Volumes/system-volume/System/Volumes/Data/)
    #
    tgt_path = dirusers
    src_path = obj.apfsVmntpt + dirusers
    docmd(['/bin/mkdir', '-p', src_path])
    err = docmd(['/usr/local/bin/apfsctl', 'firmlink', '-s', '1', tgt_path, src_path])
    if err:
        return err

    # Add Preboot and Recovery Volumes
    err = add_preb_recov_volumes(obj)
    if err:
        return err
    err = docmd(
        [
            '/usr/sbin/bless',
            '--folder',
            obj.apfsVmntpt + '/System/Library/CoreServices',
            '--bootefi',
        ]
    )

    #
    # XXX EnableFileVault Not Supported on System volume nor Data volume
    # Enable encryption on Data volume
    #
    # if obj.flag & FLAG_SRC_Encrypt_DATA:
    #    err = docmd(['/usr/sbin/diskutil', 'apfs', 'enableFileVault', obj.apfsDVdev,\
    #        '-user', 'disk', '-passphrase', 'disk'])
    #    if err:
    #        return err
    #

    if err == 0:
        docmd(['/usr/sbin/diskutil', 'unmount', obj.apfsVdev])
        err = docmd(['/usr/sbin/diskutil', 'mount', 'readOnly', obj.apfsVdev])
    print_dev_info(obj)
    return err


#
# Make a dummy disk image contianing a APFS Container with a Read-Only System Volume
# (Role: System) and a Data Volume (Role: Data)
#
def make_dummy_rosvimg(src):
    err = make_dummy_rosv(src, SIZE_SRCDATA, SRCROSVSYSVOL, SRCROSVDATAVOL, SRCIMG)
    if err:
        return err
    docmd(['/usr/sbin/diskutil', 'unmount', src.apfsDVdev])
    docmd(['/usr/sbin/diskutil', 'unmount', src.apfsVdev])

    src.name = SRCROSVIMG
    err = docmd(
        ['/usr/bin/hdiutil', 'create', '-srcdevice', src.apfsCdev, '-ov', src.name]
    )
    if err:
        return err
    err = docmd(['/usr/sbin/asr', 'imagescan', '-s', src.name])
    return err


#
# Make a dummy ROSV volume with some snapshots as the source
#
def make_dummysnap_rosv(src):
    err = make_dummy_rosv(src, SIZE_SRCDATA, SRCROSVSYSVOL, SRCROSVDATAVOL, SRCIMG)
    if err == 0:
        # Remount System volume as read-write
        err = docmd(['/usr/sbin/diskutil', 'unmount', src.apfsVdev])
        if err == 0:
            err = docmd(['/usr/sbin/diskutil', 'mount', src.apfsVdev])
        if err == 0:
            err = make_snapshots(src)
    return err


#
# Make a dummy ROSV diskimage with some snapshots as the source
#
def make_dummysnap_rosvimg(src):
    err = make_dummysnap_rosv(src)
    if err:
        return err

    docmd(['/usr/sbin/diskutil', 'unmount', src.apfsDVdev])
    docmd(['/usr/sbin/diskutil', 'unmount', src.apfsVdev])
    src.name = SRCROSVIMG
    err = docmd(
        ['/usr/bin/hdiutil', 'create', '-srcdevice', src.apfsCdev, '-ov', src.name]
    )
    if err:
        return err
    err = docmd(['/usr/sbin/asr', 'imagescan', '-s', src.name])
    return err


#
# Make a dummy single volume as the source
#
def make_dummy_srcvol(src):
    err = make_dummy_vol(src, SIZE_SRCDATA, SRCDATA, SRCVOL, SRCIMG, 'APFS')
    return err


#
# Make a dummy ROSV volume as the source
#
def make_dummy_srcrosv(src):
    err = make_dummy_rosv(src, SIZE_SRCDATA, SRCROSVSYSVOL, SRCROSVDATAVOL, SRCIMG)
    return err


#
# Get nvram value for the given string name
#
def get_nvram_value(name):
    nvram = Popen(['/usr/sbin/nvram', name], stdout=PIPE)
    cmd = "/usr/bin/awk 'NR==1 {print $2}'"
    value = Popen(cmd, stdin=nvram.stdout, stdout=PIPE, shell=True)
    out, err = value.communicate()
    if err:
        return ''
    out = out.strip(b'\n')
    logging.info('get_nvram_value: %s = %s', name, out)
    out = bytes.decode(out)
    print('get_nvram_value: ' + name + ' = ' + out)
    return out


# ================================================
#    CLASSES: SOURCE
# ================================================

#
# SOURCE CLASS
#    Source can be a disk image, /dev entry, or volume mountpoint.
#    opts = (name, type, mntopt)
#

FLAG_SRC_ROSVdata = 1 << 1
FLAG_SRC_ROSVsys = 1 << 2
FLAG_SRC_Encrypt = 1 << 3
# XXX EnableFV on Data volume via diskutil is not supported by APFS
FLAG_SRC_Encrypt_DATA = 1 << 4
FLAG_SRC_SSV = 1 << 5
FLAG_SRC_SSV_SNAP = 1 << 6
FLAG_SRC_PREBOOT_GroupUUID = 1 << 7

# Snapshot array index for snapsys and snapdata
SNAP1 = 0
SNAP2 = 1
SNAP3 = 2
SNAPDIFF = 3
SNAPLIVE = 4
SNAP_NONAME = 5


class Source:
    diskimgdev = ''  # disk image device
    apfsCdev = ''  # apfs container device
    apfsVdev = ''  # apfs (System) volume device
    apfsVmntpt = ''  # apfs (System) volume mountpoint
    apfsDVdev = ''  # apfs Data volume device
    apfsDVmntpt = ''  # apfs Data volume mountpoint
    hfsdev = ''  # hfs volume device
    holdfile = ''  # file object for HOLDMNTFILE in apfsVmntpt

    def __init__(self, opts):
        self.name = opts[0]
        self.type = opts[1]
        self.mntopt = opts[2]
        self.flag = opts[3]

        # Initialize snapshots values: array of (snapshot name, file, UUID)
        rows, cols = (6, 3)
        self.snapsys = [['' for i in range(cols)] for j in range(rows)]
        init_snap_values(self.snapsys[SNAP1], 'snap1', '/snapsys1-foo', '')
        init_snap_values(self.snapsys[SNAP2], 'snap2', '/snapsys2-foo', '')
        init_snap_values(self.snapsys[SNAP3], 'snap3', '/snapsys3-foo', '')
        init_snap_values(self.snapsys[SNAPDIFF], 'snapsys1', '/snapsys1-bar', '')
        init_snap_values(self.snapsys[SNAPLIVE], 'snaplive', '/livesys-bar', '')
        init_snap_values(self.snapsys[SNAP_NONAME], 'nosnap', '', '')

        self.snapdata = [['' for i in range(cols)] for j in range(rows)]
        init_snap_values(self.snapdata[SNAP1], 'snap1', '/snapdata1-foo', '')
        init_snap_values(self.snapdata[SNAP2], 'snap2', '/snapdata2-foo', '')
        init_snap_values(self.snapdata[SNAP3], 'snap3', '/snapdata3-foo', '')
        init_snap_values(self.snapdata[SNAPDIFF], 'snapdata1', '/snapdata1-bar', '')
        init_snap_values(self.snapdata[SNAPLIVE], 'snaplive', '/livedata-bar', '')
        init_snap_values(self.snapdata[SNAP_NONAME], 'nosnap', '', '')

    def setup(self):
        if self.type == TYPE_SRC_NAME:
            # Just use the given name, opts[0], as the source
            return 0
        elif self.type == TYPE_SRC_IMG:
            err = make_dummy_img(self, 'APFS')
        elif self.type == TYPE_SRC_VOL:
            err = make_dummy_srcvol(self)
        elif self.type == TYPE_SRC_OSIMG:
            err = make_dummy_osimg(self)
        elif self.type == TYPE_SRC_OSVOL:
            err = make_dummy_osvol(self)
        elif self.type == TYPE_SRC_ROSV:
            err = make_dummy_srcrosv(self)
        elif self.type == TYPE_SRC_ROSVIMG:
            err = make_dummy_rosvimg(self)
        elif self.type == TYPE_SRC_SNAPVOL:
            err = make_dummysnap_osvol(self)
        elif self.type == TYPE_SRC_SNAPIMG:
            err = make_dummysnap_osimg(self)
        elif self.type == TYPE_SRC_SNAPROSV:
            err = make_dummysnap_rosv(self)
        elif self.type == TYPE_SRC_SNAPROSVIMG:
            err = make_dummysnap_rosvimg(self)
        elif self.type == TYPE_SRC_HFSIMG:
            err = make_dummy_hfsimg(self)
        else:
            logging.error('Unknown Source Type')
            err = 1
        if self.flag & FLAG_SRC_ROSVdata:
            self.name = self.apfsDVdev
        if err:
            return err

        if err or self.mntopt == '':
            return err

        #
        # Now, take care of self.mntopt
        #
        if self.flag & FLAG_SRC_ROSVdata:
            vdev = self.apfsDVdev
            mntpt = self.apfsDVmntpt
        else:
            vdev = self.apfsVdev
            mntpt = self.apfsVmntpt

        # Create a hold file
        if self.type == TYPE_SRC_ROSV and ((self.flag & FLAG_SRC_ROSVdata) == 0):
            # remount ROSV System volume to write a hold file, remount back to ro
            docmd(['/usr/sbin/diskutil', 'unmount', self.apfsVdev])
            docmd(['/usr/sbin/diskutil', 'mount', self.apfsVdev])
            docmd(['/usr/bin/touch', self.apfsVmntpt + HOLDMNTFILE])
            err = docmd(['/usr/sbin/diskutil', 'mount', 'readOnly', self.apfsVdev])
        else:
            docmd(['/usr/bin/touch', mntpt + HOLDMNTFILE])

        if self.mntopt == 'umnt':
            err = docmd(['/usr/sbin/diskutil', 'unmount', vdev])
            if err:
                return err
            self.name = vdev
        elif self.mntopt == 'ro' or self.mntopt == 'robusy':
            err = docmd(['/usr/sbin/diskutil', 'unmount', vdev])
            if err:
                return err
            err = docmd(['/usr/sbin/diskutil', 'mount', 'readOnly', vdev])

        # Make the mount point busy and check if it's done correctly
        if err == 0 and (self.mntopt == 'robusy' or self.mntopt == 'rwbusy'):
            fname = mntpt + HOLDMNTFILE
            self.holdfile = open(fname, 'r')
            logging.info('Opening %s', fname)
            ret = docmd(['/sbin/umount', mntpt])
            if ret == 0:
                logging.error('Fail: umount ' + mntpt + ' should fail with busy')
                print('Fail: umount ' + mntpt + ' should fail with busy')
                err = 1
        return err

    def release(self):
        global unclean_count
        if self.type == TYPE_SRC_NAME:
            return
        if self.holdfile != '':
            self.holdfile.close()
            logging.info('Closing %s', self.apfsVmntpt + HOLDMNTFILE)
            self.holdfile = ''
        if self.apfsVdev != '':
            docmd(['/usr/sbin/diskutil', 'apfs', 'deleteVolume', self.apfsVdev])
        if self.apfsDVdev != '':
            docmd(['/usr/sbin/diskutil', 'apfs', 'deleteVolume', self.apfsDVdev])
        if self.diskimgdev != '':
            err = docmd(['hdiutil', 'detach', self.diskimgdev])
            for x in range(0, 3):
                if err:
                    print('Wait and retry ...')
                    time.sleep(5)
                    err = docmd(['hdiutil', 'detach', self.diskimgdev])
                else:
                    break

            if err:
                logging.info('hdiutil detach ' + self.diskimgdev + ' Fail UNCLEAN')
                print('hdiutil detach ' + self.diskimgdev + ' Fail UNCLEAN')
                unclean_count = unclean_count + 1
            self.diskimgdev = ''
        docmd(['/bin/rm', SRCOSIMG])
        docmd(['/bin/rm', SRCROSVIMG])
        docmd(['/bin/rm', SRCIMG])
        return
