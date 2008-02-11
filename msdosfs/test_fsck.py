#
# Run a variety of tests against fsck_msdos
#
# Usage:
#	python test_fsck.py [<fsck_msdos> [<tmp_dir>]]
#
# where <tmp_dir> is a path to a directory where disk images will be
# temporarily created.  If <path_to_fsck> is specified, it is used instead
# of 'fsck_msdos' to invoke the fsck_msdos program (for example, to test
# a new build that has not been installed).
#

import sys
import os
import subprocess
from msdosfs import *
from HexDump import HexDump

class LaunchError(Exception):
	def __init__(self, returncode):
		self.returncode = returncode
		if returncode < 0:
			self.message = "Program exited with signal %d" % -returncode
		else:
			self.message = "Program exited with status %d" % returncode
	
	def __str__(self):
		return self.message

class FailureExpected(Exception):
	def __init__(self, s):
		self.s = s
	def __str__(self):
		return self.s

#
# launch -- A helper to run another process and collect the standard output
# and standard error streams.  If the process returns a non-zero exit
# status, then raise an exception.
#
def launch(args, **kwargs):
	print "launch:", args, kwargs
	p = subprocess.Popen(args, **kwargs)
	stdout, stderr = p.communicate()
	if p.returncode != 0:
		raise LaunchError(p.returncode)
	return stdout, stderr

#
# 1. Make a disk image file
# 2. Attach the image file, without mounting
# ---- Begin per-test stuff ----
# 3. newfs_msdos the image
# 4. Fill image with content
# 5. fsck_msdos -n the image
# 6. fsck_msdos -y the image
# 7. Run /sbin/fsck_msdos against image
# ---- End per-test stuff ----
# 8. Detach the image
# 9. Delete the image file
#

#
# Run tests on 20GiB FAT32 sparse disk image
#
def test_fat32(dir, fsck, newfs):	
	#
	# Create a 20GB disk image in @dir
	#
	dmg = os.path.join(dir, 'Test20GB.sparseimage')
	launch('hdiutil create -size 20g -type SPARSE -layout NONE'.split()+[dmg])
	newfs_opts = "-F 32 -b 4096 -v TEST20GB".split()
	
	#
	# Attach the image
	#
	disk = launch(['hdiutil', 'attach', '-nomount', dmg], stdout=subprocess.PIPE)[0].rstrip()
	rdisk = disk.replace('/dev/disk', '/dev/rdisk')
	
	#
	# Run tests
	#
	# TODO: Known good disk
	#	empty file
	#	one cluster file
	#	larger file
	#	one cluster directory
	#	larger directory
	#
	test_empty(rdisk, fsck, newfs, newfs_opts)
	orphan_clusters(rdisk, fsck, newfs, newfs_opts)
	file_excess_clusters(rdisk, fsck, newfs, newfs_opts)
	file_bad_clusters(rdisk, fsck, newfs, newfs_opts)
	dir_bad_start(rdisk, fsck, newfs, newfs_opts)
	root_bad_start(rdisk, fsck, newfs, newfs_opts)	# FAT32 only!
	root_bad_first_cluster(rdisk, fsck, newfs, newfs_opts)	# FAT32 only!
	dir_size_dots(rdisk, fsck, newfs, newfs_opts)
	long_name(rdisk, fsck, newfs, newfs_opts)
	past_end_of_dir(rdisk, fsck, newfs, newfs_opts)
	fat_bad_0_or_1(rdisk, fsck, newfs, newfs_opts)
	fat_mark_clean(rdisk, fsck, newfs, newfs_opts)
	file_4GB(rdisk, fsck, newfs, newfs_opts)
	file_4GB_excess_clusters(rdisk, fsck, newfs, newfs_opts)
	
	#
	# Detach the image
	#
	launch(['diskutil', 'eject', disk])
	
	#
	# Delete the image file
	#
	os.remove(dmg)

#
# Run tests on 160MiB FAT16 image
#
def test_fat16(dir, fsck, newfs):	
	#
	# Create a 160MB disk image in @dir
	#
	dmg = os.path.join(dir, 'Test160MB.dmg')
	f = file(dmg, "w")
	f.truncate(160*1024*1024)
	f.close
	newfs_opts = "-F 16 -b 4096 -v TEST160MB".split()
	
	#
	# Attach the image
	#
	disk = launch(['hdiutil', 'attach', '-nomount', dmg], stdout=subprocess.PIPE)[0].rstrip()
	rdisk = disk.replace('/dev/disk', '/dev/rdisk')
	
	#
	# Run tests
	#
	# TODO: Known good disk
	#	empty file
	#	one cluster file
	#	larger file
	#	one cluster directory
	#	larger directory
	#
	test_empty(rdisk, fsck, newfs, newfs_opts)
 	orphan_clusters(rdisk, fsck, newfs, newfs_opts)
	file_excess_clusters(rdisk, fsck, newfs, newfs_opts)
	file_bad_clusters(rdisk, fsck, newfs, newfs_opts)
	dir_bad_start(rdisk, fsck, newfs, newfs_opts)
	dir_size_dots(rdisk, fsck, newfs, newfs_opts)
	long_name(rdisk, fsck, newfs, newfs_opts)
	past_end_of_dir(rdisk, fsck, newfs, newfs_opts)
	fat_bad_0_or_1(rdisk, fsck, newfs, newfs_opts)
	fat_mark_clean(rdisk, fsck, newfs, newfs_opts)
	
	#
	# Detach the image
	#
	launch(['diskutil', 'eject', disk])
	
	#
	# Delete the image file
	#
	os.remove(dmg)

#
# Run tests on 15MiB FAT12 image
#
def test_fat12(dir, fsck, newfs):	
	#
	# Create a 15MB disk image in @dir
	#
	dmg = os.path.join(dir, 'Test15MB.dmg')
	f = file(dmg, "w")
	f.truncate(15*1024*1024)
	f.close
	newfs_opts = "-F 12 -b 4096 -v TEST15MB".split()
	
	#
	# Attach the image
	#
	disk = launch(['hdiutil', 'attach', '-nomount', dmg], stdout=subprocess.PIPE)[0].rstrip()
	rdisk = disk.replace('/dev/disk', '/dev/rdisk')
	
	#
	# Run tests
	#
	# TODO: Known good disk
	#	empty file
	#	one cluster file
	#	larger file
	#	one cluster directory
	#	larger directory
	#
	test_empty(rdisk, fsck, newfs, newfs_opts)
 	orphan_clusters(rdisk, fsck, newfs, newfs_opts)
	file_excess_clusters(rdisk, fsck, newfs, newfs_opts)
	file_bad_clusters(rdisk, fsck, newfs, newfs_opts)
	dir_bad_start(rdisk, fsck, newfs, newfs_opts)
	dir_size_dots(rdisk, fsck, newfs, newfs_opts)
	long_name(rdisk, fsck, newfs, newfs_opts)
	past_end_of_dir(rdisk, fsck, newfs, newfs_opts)
	fat_bad_0_or_1(rdisk, fsck, newfs, newfs_opts)
	
	#
	# Detach the image
	#
	launch(['diskutil', 'eject', disk])
	
	#
	# Delete the image file
	#
	os.remove(dmg)

#
# A minimal test -- make sure fsck_msdos runs on an empty image
#
def test_empty(disk, fsck, newfs, newfs_opts):
	#
	# newfs the disk
	#
	launch([newfs]+newfs_opts+[disk])
	
	#
	# fsck the disk
	#
	launch([fsck, '-n', disk])

#
# Make a volume with allocated but unreferenced cluster chains
#
def orphan_clusters(disk, fsck, newfs, newfs_opts):
	launch([newfs]+newfs_opts+[disk])
	
	#
	# Create some cluster chains not referenced by any file or directory
	#
	f = file(disk, "r+")
	v = msdosfs(f)
	v.allocate(7, 100)
	v.allocate(23, 150)
	v.allocate(1, 190)
	v.flush()
	del v
	f.close()
	del f
	
	try:
		launch([fsck, '-n', disk])
	except LaunchError:
		pass
	launch([fsck, '-p', disk])
	launch(['/sbin/fsck_msdos', '-n', disk])

#
# Make a file with excess clusters allocated
#	One file with EOF == 0
#	One file with EOF != 0
#	Files with excess clusters that are cross-linked
#		First excess cluster is cross-linked
#		Other excess cluster is cross-linked
#	Excess clusters end with free/bad/reserved cluster
#		First excess cluster is free/bad/reserved
#		Other excess cluster is free/bad/reserved
#
def file_excess_clusters(disk, fsck, newfs, newfs_opts):
	launch([newfs]+newfs_opts+[disk])

	#
	# Create files with too many clusters for their size
	#
	f = file(disk, "r+")
	v = msdosfs(f)
	head=v.allocate(7)
	v.root().mkfile('FOO', head=head, length=6*v.bytesPerCluster)
	head=v.allocate(1)
	v.root().mkfile('BAR', head=head, length=0)
	
	#
	# LINK1 is OK.
	# LINK2 contains excess clusters; the first is cross-linked with LINK1
	# LINK3 contains excess clusters; the second is cross-linked with LINK1
	#
	clusters = v.fat.find(9)
	head = v.fat.chain(clusters)
	v.root().mkfile('LINK1', head=head, length=8*v.bytesPerCluster+1)
	head = v.fat.allocate(3, last=clusters[7])
	v.root().mkfile('LINK2', head=head, length=2*v.bytesPerCluster+3)
	head = v.fat.allocate(5, last=clusters[8])
	v.root().mkfile('LINK3', head=head, length=3*v.bytesPerCluster+5)
	if v.fsinfo:
		v.fsinfo.allocate(9+3+5)

	#
	# FREE1 has its first excess cluster marked free
	# BAD3 has its third excess cluster marked bad
	#
	head = v.allocate(11, last=CLUST_BAD)
	v.root().mkfile('BAD3', head=head, length=8*v.bytesPerCluster+300)
	head = v.allocate(8, last=CLUST_FREE)
	v.root().mkfile('FREE1', head=head, length=6*v.bytesPerCluster+100)
	
	v.flush()
	del v
	f.close()
	del f
	
	try:
		launch([fsck, '-n', disk])
	except LaunchError:
		pass
	launch([fsck, '-y', disk])
	launch(['/sbin/fsck_msdos', '-n', disk])

#
# Make files with bad clusters in their chains
#	FILE1 file with middle cluster free
#	FILE2 file with middle cluster bad/reserved
#	FILE3 file with middle cluster points to out of range cluster
#	FILE4 file with middle cluster that is cross-linked (to same file)
#	FILE5 file whose head is "free"
#	FILE6 file whose head is "bad"
#	FILE7 file whose head is out of range
#	FILE8 file whose head is cross-linked
#
def file_bad_clusters(disk, fsck, newfs, newfs_opts):
	launch([newfs]+newfs_opts+[disk])

	f = file(disk, "r+")
	v = msdosfs(f)
	
	clusters = v.fat.find(5)
	to_free = clusters[2]
	head = v.fat.chain(clusters)
	v.root().mkfile('FILE1', head=head, length=6*v.bytesPerCluster+111)
	if v.fsinfo:
		v.fsinfo.allocate(5)
	
	clusters = v.fat.find(5)
	head = v.fat.chain(clusters)
	v.root().mkfile('FILE2', head=head, length=4*v.bytesPerCluster+222)
	v.fat[clusters[2]] = CLUST_RSRVD
	if v.fsinfo:
		v.fsinfo.allocate(5)
	
	clusters = v.fat.find(5)
	head = v.fat.chain(clusters)
	v.root().mkfile('FILE3', head=head, length=4*v.bytesPerCluster+333)
	v.fat[clusters[2]] = 1
	if v.fsinfo:
		v.fsinfo.allocate(5)
	
	clusters = v.fat.find(5)
	head = v.fat.chain(clusters)
	v.root().mkfile('FILE4', head=head, length=4*v.bytesPerCluster+44)
	v.fat[clusters[2]] = clusters[1]
	if v.fsinfo:
		v.fsinfo.allocate(5)
	
	v.root().mkfile('FILE5', head=CLUST_FREE, length=4*v.bytesPerCluster+55)

	v.root().mkfile('FILE6', head=CLUST_BAD, length=4*v.bytesPerCluster+66)

	v.root().mkfile('FILE7', head=CLUST_RSRVD-1, length=4*v.bytesPerCluster+77)

	head = v.allocate(5)
	v.root().mkfile('FOO', head=head, length=4*v.bytesPerCluster+99)
	v.root().mkfile('FILE8', head=head, length=4*v.bytesPerCluster+88)

	# Free the middle cluster of FILE1 now that we've finished allocating
	v.fat[to_free] = CLUST_FREE
	
	v.flush()
	del v
	f.close()
	del f
	
	try:
		launch([fsck, '-n', disk])
	except LaunchError:
		pass
	launch([fsck, '-y', disk])
	launch(['/sbin/fsck_msdos', '-n', disk])

#
# Make directories whose starting cluster number is free/bad/reserved/out of range
#	DIR1 start cluster is free
#	DIR2 start cluster is reserved
#	DIR3 start cluster is bad
#	DIR4 start cluster is EOF
#	DIR5 start cluster is 1
#	DIR6 start cluster is one more than max valid cluster
#
def dir_bad_start(disk, fsck, newfs, newfs_opts):
	def mkdir(parent, name, head):
		bytes = make_long_dirent(name, ATTR_DIRECTORY, head=head)
		slots = len(bytes)/32
		slot = parent.find_slots(slots, grow=True)
		parent.write_slots(slot, bytes)

	launch([newfs]+newfs_opts+[disk])

	f = file(disk, "r+")
	v = msdosfs(f)
	root = v.root()
	
	mkdir(root, 'DIR1', CLUST_FREE)
	mkdir(root, 'DIR2', CLUST_RSRVD)
	mkdir(root, 'DIR3', CLUST_BAD)
	mkdir(root, 'DIR4', CLUST_EOF)
	mkdir(root, 'DIR5', 1)
	mkdir(root, 'DIR6', v.clusters+2)
	
	v.flush()
	del v
	f.close()
	del f
	
	try:
		launch([fsck, '-n', disk])
	except LaunchError:
		pass
	launch([fsck, '-y', disk])
	launch(['/sbin/fsck_msdos', '-n', disk])

#
# Root dir's starting cluster number is free/bad/reserved/out of range
#
# NOTE: This test is only applicable to FAT32!
#
def root_bad_start(disk, fsck, newfs, newfs_opts):
	def set_root_start(disk, head):
		dev = file(disk, "r+")
		dev.seek(0)
		bytes = dev.read(512)
		bytes = bytes[0:44] + struct.pack("<I", head) + bytes[48:]
		dev.seek(0)
		dev.write(bytes)
		dev.close()
		del dev

	launch([newfs]+newfs_opts+[disk])

	f = file(disk, "r+")
	v = msdosfs(f)
	clusters = v.clusters
	v.flush()
	del v
	f.close()
	del f
	
	for head in [CLUST_FREE, CLUST_RSRVD, CLUST_BAD, CLUST_EOF, 1, clusters+2]:
		set_root_start(disk, head)
		
		try:
			launch([fsck, '-n', disk])
		except LaunchError:
			pass
		try:
			launch([fsck, '-y', disk])
		except LaunchError:
			pass
		try:
			launch(['/sbin/fsck_msdos', '-n', disk])
		except LaunchError:
			pass

#
# Root dir's first cluster is free/bad/reserved
#
# NOTE: This test is only applicable to FAT32!
#
def root_bad_first_cluster(disk, fsck, newfs, newfs_opts):
	for link in [CLUST_FREE, CLUST_RSRVD, CLUST_BAD]:
		launch([newfs]+newfs_opts+[disk])
	
		f = file(disk, "r+")
		v = msdosfs(f)
		v.fat[v.rootCluster] = link
		v.flush()
		del v
		f.close()
		del f
		
		try:
			launch([fsck, '-n', disk])
		except LaunchError:
			pass
		launch([fsck, '-y', disk])
		launch(['/sbin/fsck_msdos', '-n', disk])

#
# Create subdirectories with the following problems:
#	Size (length) field is non-zero
#	"." entry has wrong starting cluster
#	".." entry start cluster is non-zero, and parent is root
#	".." entry start cluster is zero, and parent is not root
#	".." entry start cluster is incorrect
#
def dir_size_dots(disk, fsck, newfs, newfs_opts):
	launch([newfs]+newfs_opts+[disk])

	f = file(disk, "r+")
	v = msdosfs(f)
	root = v.root()
	
	# Make a couple of directories without any problems
	child = root.mkdir('CHILD')
	grand = child.mkdir('GRAND')

	# Directory has non-zero size
	dir = root.mkdir('BADSIZE', length=666)

	# "." entry has incorrect start cluster
	dir = root.mkdir('BADDOT')
	fields = parse_dirent(dir.read_slots(0))
	fields['head'] = fields['head'] + 30
	dir.write_slots(0, make_dirent(**fields))

	# ".." entry has non-zero start cluster, but parent is root
	dir = root.mkdir('DOTDOT.NZ')
	fields = parse_dirent(dir.read_slots(0))
	fields['head'] = 47
	dir.write_slots(0, make_dirent(**fields))

	# ".." entry has zero start cluster, but parent is not root
	dir = child.mkdir('DOTDOT.ZER')
	fields = parse_dirent(dir.read_slots(0))
	fields['head'] = 0
	dir.write_slots(0, make_dirent(**fields))

	# ".." entry start cluster is incorrect (parent is not root)
	dir = grand.mkdir('DOTDOT.BAD')
	fields = parse_dirent(dir.read_slots(0))
	fields['head'] = fields['head'] + 30
	dir.write_slots(0, make_dirent(**fields))
	
	v.flush()
	del v
	f.close()
	del f

	try:
		launch([fsck, '-n', disk])
	except LaunchError:
		pass
	launch([fsck, '-y', disk])
	launch(['/sbin/fsck_msdos', '-n', disk])

def long_name(disk, fsck, newfs, newfs_opts):
	launch([newfs]+newfs_opts+[disk])

	f = file(disk, "r+")
	v = msdosfs(f)
	root = v.root()
	
	# Long name entries (valid or not!) preceding volume label
	bytes = make_long_dirent('Test1GB', ATTR_VOLUME_ID)
	root.write_slots(0, bytes)
	
	# Create a file with a known good long name
	root.mkfile('The quick brown fox jumped over the lazy dog')
	
	# Create a file with a known good short name
	root.mkfile('foo.bar')
	
	# Create a file with invalid long name entries (bad checksums)
	bytes = make_long_dirent('Greetings and felicitations my friends', ATTR_ARCHIVE)
	bytes = bytes[0:-32] + 'HELLO      ' + bytes[-21:]
	assert len(bytes) % 32 == 0
	slots = len(bytes) / 32
	slot = root.find_slots(slots)
	root.write_slots(slot, bytes)
	
	subdir = root.mkdir('SubDir')
	
	# Create a file with incomplete long name entries
	#	Missing first (LONG_NAME_LAST) entry
	bytes = make_long_dirent('To be or not to be', ATTR_ARCHIVE)[32:]
	slots = len(bytes) / 32
	slot = subdir.find_slots(slots)
	subdir.write_slots(slot, bytes)
	
	#	Missing middle (second) long entry
	bytes = make_long_dirent('A Man a Plan a Canal Panama', ATTR_ARCHIVE)
	bytes = bytes[:32] + bytes[64:]
	slots = len(bytes) / 32
	slot = subdir.find_slots(slots)
	subdir.write_slots(slot, bytes)

	#	Missing last long entry
	bytes = make_long_dirent('We the People in order to form a more perfect union', ATTR_ARCHIVE)
	bytes = bytes[0:-64] + bytes[-32:]
	slots = len(bytes) / 32
	slot = subdir.find_slots(slots)
	subdir.write_slots(slot, bytes)

	subdir = root.mkdir('Bad Orders')
	
	#	Bad order value: first
	bytes = make_long_dirent('One is the loneliest number', ATTR_ARCHIVE)
	bytes = chr(ord(bytes[0])+7) + bytes[1:]
	slots = len(bytes) / 32
	slot = subdir.find_slots(slots)
	subdir.write_slots(slot, bytes)
	
	#	Bad order value: middle
	bytes = make_long_dirent('It takes two to tango or so they say', ATTR_ARCHIVE)
	bytes = bytes[:32] + chr(ord(bytes[32])+7) + bytes[33:]
	slots = len(bytes) / 32
	slot = subdir.find_slots(slots)
	subdir.write_slots(slot, bytes)
	
	#	Bad order value: last
	bytes = make_long_dirent('Threes Company becomes Threes A Crowd', ATTR_ARCHIVE)
	bytes = bytes[:-64] + chr(ord(bytes[-64])+7) + bytes[-63:]
	slots = len(bytes) / 32
	slot = subdir.find_slots(slots)
	subdir.write_slots(slot, bytes)
	
	# Long name entries (valid or not, with no short entry) at end of directory
	bytes = make_long_dirent('Four score and seven years ago', ATTR_ARCHIVE)
	bytes = bytes[0:-32]	# Remove the short name entry
	assert len(bytes) % 32 == 0
	slots = len(bytes) / 32
	slot = root.find_slots(slots)
	root.write_slots(slot, bytes)
	
	v.flush()
	del v
	f.close()
	del f
	
	try:
		launch([fsck, '-n', disk])
	except LaunchError:
		pass
	launch([fsck, '-y', disk])
	launch(['/sbin/fsck_msdos', '-n', disk])

def past_end_of_dir(disk, fsck, newfs, newfs_opts):
	launch([newfs]+newfs_opts+[disk])

	f = file(disk, "r+")
	v = msdosfs(f)
	root = v.root()
	
	subdir = root.mkdir('SubDir')
	subdir.mkfile('Good Sub File')
	root.mkfile('Good Root File')
	
	# Make an entry that will be replaced by end-of-directory
	slotEOF = root.find_slots(1)
	root.mkfile('EOF')
	
	# Make some valid file entries past end of directory
	root.mkfile('BADFILE')
	root.mkdir('Bad Dir')
	root.mkfile('Bad File 2')
	
	# Overwrite 'EOF' entry with end-of-directory marker
	root.write_slots(slotEOF, '\x00' * 32)
	
	# Make an entry that will be replaced by end-of-directory
	slotEOF = subdir.find_slots(1)
	subdir.mkfile('EOF')
	
	# Make some valid file entries past end of directory
	subdir.mkfile('BADFILE')
	subdir.mkdir('Bad Dir')
	subdir.mkfile('Bad File 2')
	
	# Overwrite 'EOF' entry with end-of-directory marker
	subdir.write_slots(slotEOF, '\x00' * 32)
	
	v.flush()
	del v
	f.close()
	del f
	
	try:
		launch([fsck, '-n', disk])
	except LaunchError:
		pass
	launch([fsck, '-y', disk])
	launch(['/sbin/fsck_msdos', '-n', disk])

#
# Stomp the first two FAT entries.
#
def fat_bad_0_or_1(disk, fsck, newfs, newfs_opts):
	launch([newfs]+newfs_opts+[disk])

	f = file(disk, "r+")
	v = msdosfs(f)

	v.fat[0] = 0
	v.fat[1] = 1
	
	v.flush()
	del v
	f.close()
	del f
	
	try:
		launch([fsck, '-n', disk])
	except LaunchError:
		pass
	launch([fsck, '-y', disk])
	launch(['/sbin/fsck_msdos', '-n', disk])

#
# Mark the volume dirty, and cause some minor damage (orphan clusters).
# Make sure the volume gets marked clean afterwards.
#
def fat_mark_clean(disk, fsck, newfs, newfs_opts):
	launch([newfs]+newfs_opts+[disk])

	f = file(disk, "r+")
	v = msdosfs(f)

	# Mark the volume "dirty" by clearing the "clean" bit.
	if v.type == 32:
		v.fat[1] = v.fat[1] & 0x07FFFFFF
	else:
		v.fat[1] = v.fat[1] & 0x7FFF
	
	# Allocate some clusters, so there is something to repair.
	v.allocate(3)
	
	v.flush()
	del v
	f.close()
	del f
	
	try:
		launch([fsck, '-n', disk])
	except LaunchError:
		pass
	launch([fsck, '-y', disk])
	
	f = file(disk, "r")
	v = msdosfs(f)

	# Make sure the "clean" bit is now set.
	if v.type == 32:
		clean = v.fat[1] & 0x08000000
	else:
		clean = v.fat[1] & 0x8000
	if not clean:
		raise RuntimeError("Volume still dirty!")
		
	v.flush()
	del v
	f.close()
	del f

	launch(['/sbin/fsck_msdos', '-n', disk])

#
# Make a file whose physical size is 4GB.  The logical size is 4GB-100.
# This is actually NOT corrupt; it's here to verify that fsck_hfs does not
# try to truncate the file due to overflow of the physical size.  [4988133]
#
def file_4GB(disk, fsck, newfs, newfs_opts):
	launch([newfs]+newfs_opts+[disk])

	#
	# Create a file whose size is 4GB-100.  That means its physical size will
	# be rounded up to the next multiple of the cluster size, meaning the
	# physical size will be 4GB.
	#
	f = file(disk, "r+")
	v = msdosfs(f)
	four_GB = 4*1024*1024*1024
	clusters = four_GB / v.bytesPerCluster
	head = v.allocate(clusters)
	v.root().mkfile('4GB', head=head, length=four_GB-100)
		
	v.flush()
	del v
	f.close()
	del f
	
	launch([fsck, '-n', disk])

#
# Make a file with excess clusters allocated: over 4GB worth of clusters
#
# TODO: What combination of files do we want to test with?
# TODO: 	A smallish logical size
# TODO: 	A logical size just under 4GB
# TODO: 	Cross-linked files?
# TODO: 		Cross linked beyond 4GB?
# TODO: 		Cross linked before 4GB?
#
def file_4GB_excess_clusters(disk, fsck, newfs, newfs_opts):
	launch([newfs]+newfs_opts+[disk])

	#
	# Create files with too many clusters for their size
	#
	f = file(disk, "r+")
	v = msdosfs(f)
	four_GB = 4*1024*1024*1024
	clusters = four_GB / v.bytesPerCluster
	head=v.allocate(clusters+7)
	v.root().mkfile('FOO', head=head, length=5*v.bytesPerCluster-100)
	head=v.allocate(clusters+3)
	v.root().mkfile('BAR', head=head, length=four_GB-30)
		
	v.flush()
	del v
	f.close()
	del f
	
	# TODO: Need a better way to assert that the disk is corrupt to start with
	try:
		launch([fsck, '-n', disk])
	except LaunchError:
		pass
	launch([fsck, '-y', disk])
	launch([fsck, '-n', disk])

#
# When run as a script, run the test suite.
#
# Usage:
#	python test_fsck.py [<fsck_msdos> [<tmp_dir>]]
#
if __name__ == '__main__':
	#
	# Set up defaults
	#
	dir = '/tmp'
	fsck = 'fsck_msdos'
	newfs = 'newfs_msdos'

	if len(sys.argv) > 1:
		fsck = sys.argv[1]
	if len(sys.argv) > 2:
		dir = sys.argv[2]
	if len(sys.argv) > 3:
		print "%s: Too many arguments!" % sys.argv[0]
		print "Usage: %s [<fsck_msdos> [<tmp_dir>]]"
		sys.exit(1)
	
	#
	# Run the test suite
	#
	test_fat32(dir, fsck, newfs)
	test_fat16(dir, fsck, newfs)
	test_fat12(dir, fsck, newfs)
	
	print "\nSuccess!"
