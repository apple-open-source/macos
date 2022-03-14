#!/usr/bin/python3
"""File System Class."""
import logging
from lib.constants import NTFS, IMAGE, EXTERNAL
from lib.general import (
    docmd, docmd_stdout, docmd_plist, is_userfs_enabled,
    device_to_mntpt, hdiutil_attach, diskutil_mount, diskutil_unmount,
    syscall_mount, syscall_unmount,
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
            logging.error('Setup ntfs dmg Fail: no plist from hdiutil attach')
            return 1

        for entity in plist['system-entities']:
            if entity['volume-kind'] == 'ntfs':
                self.diskdev = entity['dev-entry']
                self.fsdev = entity['dev-entry']
                self.fsmntpt = entity['mount-point']
                break

        if self.fsmntpt is None:
            logging.error('Setup ntfs dmg Fail: no ntfs image is mounted')
            return 1
        logging.info("setup_ntfs_dmg Succeeds")
        return 0

    def setup(self, kext=False):
        """Set up a File System object and its attributes values."""
        if self.disktype == IMAGE:
            if self.fstype == NTFS:
                if self.setup_ntfs_dmg() != 0:
                    return 1
            else:
                if self.dmg is None:
                    self.dmg = hdiutil_create(self.tmp_dir, self.fstype,
                                              self.dmgsize)
                    if self.dmg is None:
                        logging.error('Setup Fail to create a diskimage')
                        return 1
                self.diskdev, self.fsdev, self.fsmntpt = hdiutil_attach(
                    self.dmg, False, None)
                if None in [self.diskdev, self.fsdev, self.fsmntpt]:
                    logging.error('Setup Fail: no information from hdituil')
                    return 1
        elif self.disktype == EXTERNAL:
            if self.__get_external_device() != 0:
                logging.error('Setup Fail to get external device')
                return 1
        else:
            raise RuntimeError(f"Unexpected disktype data {self.disktype}")

        # diskimage or external disk may be mounted via userfs by now,
        # remount per requested setup via kext/mount(8) or userfs
        self.unmount()
        if self.mount(kext=kext) != 0:
            msg = f"Setup Fail to mount {'via mount(8)' if kext is True else ''}"
            logging.error(msg)
            return 1
        logging.info("Setup Succeeds")
        logging.info(self)
        return 0

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
        """Mount via UserFS or kext plugin."""
        if kext is True:
            err = syscall_mount(self.fstype, self.fsdev)
        else:
            err = diskutil_mount(self.fsdev)
        if err:
            return err
        if self.is_mounted_fs(kext=kext) is False:
            return 1

        self.fsmntpt = device_to_mntpt(self.fsdev)
        return 0

    def unmount(self, kext=False):
        """Unmount via UserFS or kext plugin."""
        if kext is True:
            err = syscall_unmount(dev=self.fsdev)
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
        expected_fs = self.fstype
        if is_userfs_enabled(self.fstype) and kext is False:
            expected_fs = 'lifs'

        for line in mountout.split('\n'):
            if dev in line and expected_fs in line and self.fstype in line:
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
            return 1
        external_disks = plist['AllDisksAndPartitions']
        for disk in external_disks:
            if disk['Content'] == 'FDisk_partition_scheme':
                for partition in disk['Partitions']:
                    if partition['Content'] == 'Windows_NTFS':
                        if found:
                            logging.error('More than 1 Windows_NTFS disk')
                            return 1
                        self.diskdev = disk['DeviceIdentifier']
                        self.fsdev = partition['DeviceIdentifier']
                        if not self.fsdev.startswith('/dev/'):
                            self.fsdev = f'/dev/{self.fsdev}'
                        found = True
        if not found:
            logging.error('Fail: external Windows disk not found')
            return 1
        return 0
