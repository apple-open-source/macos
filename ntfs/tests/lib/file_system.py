#!/usr/bin/python3
"""File System Class."""
import os
import logging
from lib.constants import NTFS, IMAGE, EXTERNAL, SUCCESS, FAIL
from lib.general import (
    docmd, docmd_stdout, docmd_plist, is_userfs_enabled,
    device_to_mntpt, hdiutil_attach, diskutil_mount, diskutil_unmount,
)


# ============================================
#        File System Class
# ============================================


class FileSystem:
    """Local File System Class."""

    def __init__(self, fstype, disktype, dmg):
        """Instantiate a File System object.

        Arguments:
            fstype (str):         file system type, e.g. EXFAT, NTFS
            disktype (str):       disk image or external disk
            dmg (str):            disk image to use

        """
        self.fstype = fstype      # e.g. NTFS, EXFAT
        self.disktype = disktype  # disk image or external disk
        self.diskdev = None       # disk device, e.g. disk3
        self.fsdev = None         # file system device, e.g. disk3s1
        self.fsmntpt = None       # mount point, e.g. /Volumes/Untitled
        self.dmg = dmg            # disk image file

    def __str__(self):
        """Return human readable string for object."""
        return (f'FileSystem Information:'
                f'\n\tdisk device: {self.diskdev}'
                f'\n\tdisk type: {self.disktype}'
                f'\n\tfs type: {self.fstype}'
                f'\n\tfs device: {self.fsdev}'
                f'\n\tfs mountpoint: {self.fsmntpt}'
                f'\n\tdiskimage: {self.dmg}')

    def setup_ntfs_dmg(self):
        """NTFS diskimage is customized for testing."""
        plist = docmd_plist(["hdiutil", "attach", "-plist", self.dmg])
        if plist == '':
            logging.error('setup ntfs dmg Fail: no plist from hdiutil attach')
            return FAIL

        for entity in plist['system-entities']:
            if entity['volume-kind'] == 'ntfs':
                self.diskdev = entity['dev-entry']
                self.fsdev = entity['dev-entry']
                self.fsmntpt = entity['mount-point']
                break

        if self.fsmntpt is None:
            logging.error('setup ntfs dmg Fail: no ntfs image is mounted')
            return FAIL

        logging.info(self)
        return SUCCESS

    def setup(self):
        """Set up a File System object and its attributes values."""
        if self.disktype == IMAGE:
            if self.dmg is None:
                logging.error('setup Fail: no diskimage to setup')
                return FAIL

            if self.fstype == NTFS:
                return self.setup_ntfs_dmg()

            self.diskdev, self.fsdev, self.fsmntpt = hdiutil_attach(
                self.dmg, False, None)
            if None in [self.diskdev, self.fsdev, self.fsmntpt]:
                logging.error('setup Fail: no information from hdituil')
                return FAIL
        elif self.disktype == EXTERNAL:
            if self.__get_external_device() != 0:
                logging.error('setup Fail to get external device')
                return FAIL
            self.mount()
            if self.fsmntpt is None:
                logging.error('setup Fail to mount the external device')
                return FAIL
        else:
            logging.error('setup Fail: unsupported disk type')
            return FAIL

        logging.info(self)
        return SUCCESS

    def release(self, err):
        """Reverse setup() to release this object."""
        if self.diskdev:
            if docmd(["hdiutil", "eject", self.diskdev]) != 0:
                logging.error(f"release Fail to eject {self.diskdev}")

        self.dmg = None
        self.diskdev = None
        self.fsdev = None
        self.fsmntpt = None

    def mount(self, kext=False):
        """Mount via UserFS or Kext plugin."""
        if kext:
            mntpt = '/Volumes/' + self.fstype.lower() + str(os.getpid())
            if not os.path.exists(mntpt):
                err = docmd(["sudo", "mkdir", mntpt])
                if err:
                    logging.error(f"Fail to mkdir {mntpt}")
                    return err
            mountcmd = ["sudo", "mount", "-t", self.fstype, self.fsdev, mntpt]
            err = docmd(mountcmd)
        else:
            err = diskutil_mount(self.fsdev)
        if err == 0:
            self.fsmntpt = device_to_mntpt(self.fsdev)
        return err

    def unmount(self, kext=False):
        """Unmount via UserFS or Kext plugin."""
        if kext:
            err = docmd(["sudo", "umount", self.fsdev])
        else:
            err = diskutil_unmount(self.fsdev)
        if err == 0:
            self.fsmntpt = None
        return err

    def is_mounted_fs(self, kext=False):
        """Check if this filesystem is mounted with the intended fs type.

        Arguments:
            kext:     True if kext plugin is used
        Return:
            True:     mounted as fstype, and 'lifs' if UserFS enabled
            False:    not mounted as expected fstype

        """
        result = docmd_stdout(["mount"])
        mountout = result['out']
        dev = self.fsdev.replace('/dev/', '')
        if is_userfs_enabled(self.fstype) and kext is False:
            expected_fs = 'lifs'
        else:
            expected_fs = self.fstype
        for line in mountout.split('\n'):
            if dev in line and self.fstype in line:
                if expected_fs in line:
                    return True
        logging.error(f"Fail: file system is not mounted as {expected_fs}")
        return False

    def is_readonly(self):
        """Return True if the file system is mounted read-only.

        Returns:
            bool: True if the file system is mounted read-only.
                  False if the file system is mounted read-write.
        """
        if self.fsmntpt is None:
            logging.error("is_readonly Fail: file system is not mounted")
            return False

        result = docmd_stdout(["mount"])
        mountout = result['out']
        dev = self.fsdev.replace('/dev/', '')
        for line in mountout.split('\n'):
            if dev in line and self.fstype in line and 'read-only' in line:
                return True
        logging.error("is_readonly Fail: file system is not mounted read-only")
        return False

    def __get_external_device(self):
        """Get external Windows_NTFS (NTFS/EXFAT) drive information."""
        found = False
        plist = docmd_plist(["diskutil", "list", "-plist", "external"])
        if plist == '':
            logging.error("Fail: Can not get external drive plist")
            return FAIL
        external_disks = plist['AllDisksAndPartitions']
        for disk in external_disks:
            if disk['Content'] == 'FDisk_partition_scheme':
                for partition in disk['Partitions']:
                    if partition['Content'] == 'Windows_NTFS':
                        if found:
                            logging.error('More than 1 Windows_NTFS disk')
                            return FAIL
                        self.diskdev = disk['DeviceIdentifier']
                        self.fsdev = partition['DeviceIdentifier']
                        if not self.fsdev.startswith('/dev/'):
                            self.fsdev = f'/dev/{self.fsdev}'
                        found = True
        if not found:
            logging.error('Fail: external Windows disk not found')
            return FAIL
        return SUCCESS
