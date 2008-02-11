#
# msdosfs -- Helper module for dealing with FAT (MS-DOS) on-disk structures.
#

CLUST_FREE	= 0
CLUST_FIRST	= 2
CLUST_MAX12	= 0x00000FFF
CLUST_MAX16	= 0x0000FFFF
CLUST_MAX32	= 0x0FFFFFFF
CLUST_RSRVD	= 0xFFFFFFF6
CLUST_BAD	= 0xFFFFFFF7
CLUST_EOFS	= 0xFFFFFFF8
CLUST_EOF	= 0xFFFFFFFF

ATTR_READ_ONLY	= 0x01
ATTR_HIDDEN		= 0x02
ATTR_SYSTEM		= 0x04
ATTR_VOLUME_ID	= 0x08
ATTR_DIRECTORY	= 0x10
ATTR_ARCHIVE	= 0x20
ATTR_LONG_NAME	= ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID
ATTR_LONG_NAME_MASK = ATTR_LONG_NAME | ATTR_DIRECTORY | ATTR_ARCHIVE

LAST_LONG_ENTRY	= 0x40

import struct

def make_dirent(name, attr, head=0, length=0, ntres=0,
			create_time_tenth=0, create_time=0, create_date=0,
			access_date=0, mod_time=0, mod_date=0):
	"Create the raw bytes for a directory entry"
	assert len(name) == 11			
	entry = name + struct.pack("<3B7HI", attr, ntres, create_time_tenth,
		create_time, create_date, access_date, head>>16, mod_time,
		mod_date, head&0xFFFF, length)
	assert len(entry) == 32
	return entry

def parse_dirent(bytes):
	"""
	Parse the raw bytes of a directory entry, returning a dictionary of
	all of the fields.
	"""
	assert len(bytes) == 32
	fields = struct.unpack("<3B7HI", bytes[11:])
	return dict(name = bytes[0:11],
				attr = fields[0],
				ntres = fields[1],
				create_time_tenth = fields[2],
				create_time = fields[3],
				create_date = fields[4],
				access_date = fields[5],
				mod_time = fields[7],
				mod_date = fields[8],
				length = fields[10],
				head = (fields[6] << 16) + fields[9])

SHORTNAME_CHARS = "".join([chr(x) for x in xrange(33,128) if chr(x) not in '"*+,./:;<=>?[\]|'])

def mixed_case(s):
	return s.lower() != s and s.upper() != s

def fat_checksum(s):
	"Return the FAT checksum for a short name"
	assert len(s) == 11
	sum = 0
	for c in s:
		if sum & 1:
			sum = 0x80 + (sum >> 1)
		else:
			sum = sum >> 1
		sum = (sum + ord(c)) & 0xFF
	return sum

def make_ldir(name, order, checksum):
	"""
	Construct a FAT long name directory entry.
	"""
	assert 1 <= len(name) <= 13
	
	# Pad name with NUL and FFFF's
	name = (name + u'\u0000' + u'\uFFFF'*11)[0:13]

	entry = "".join([chr(order), name[0:5].encode("UTF-16LE"),
					chr(ATTR_LONG_NAME), chr(0), chr(checksum),
					name[5:11].encode("UTF-16LE"), "\x00\x00",
					name[11:13].encode("UTF-16LE")])
	assert len(entry) == 32
	return entry

def make_long_dirent(name, attr, head=0, length=0,
			create_time_tenth=0, create_time=0, create_date=0,
			access_date=0, mod_time=0, mod_date=0, tail='1'):
	"""
	Create one or more directory entries -- a short name entry (like make_dirent)
	and preceding long name entries (if any are needed).  This routine handles
	names of arbitrary length (one to 255 Unicode characters), and will set
	the "ntres" field correctly for short names whose base and/or extension
	contains lower case letters.
	"""
	name = unicode(name)
	assert 1 <= len(name) <= 255
	
	#
	# Split the name into base and extension (if any)
	#
	if u'.' in name:
		base, ext = name.rsplit(u'.', 1)
	else:
		base = name
		ext = u''
	
	#
	# See if the name will fit the "8.3" form (possibly by using ntres for
	# base/ext containing lower case characters)
	#
	needs_long_name = True
	if (1 <= len(base) <= 8) and (len(ext) <= 3):
		needs_long_name = False
		for c in base+ext:
			if c not in SHORTNAME_CHARS:
				needs_long_name = True
				break
		if mixed_case(base) or mixed_case(ext):
			needs_long_name = True
	
	# "entries" is a list of raw directory entry structures
	entries = []
	ntres = 0
	
	#
	# If we need to generate a long name, do it; otherwise determine
	# the "ntres" value for a short name.
	#
	if needs_long_name:
		# Convert invalid ASCII chars to Services For Macintosh equivalents
		
		# Construct the associated short name
		lossy = False
		basis = ""
		for c in base.upper():
			if c in SHORTNAME_CHARS:
				basis = basis + c
			elif c in u' .':
				# Remove all spaces and periods
				lossy = True
			else:
				# Collapse multiple invalid characters to single '_'
				if basis[-1] != u'_':
					basis = basis + u'_'
				lossy = True
		if len(basis) > 8:
			lossy = True
		basis = (basis + u'       ')[0:8]
		for c in ext.upper():
			if c in SHORTNAME_CHARS:
				basis = basis + c
			else:
				# Should really collapse multiple '_' to one '_'
				basis = basis + u'_'
				lossy = True
		basis = (basis + u'   ')[0:11]
		assert len(basis) == 11
		basis = basis.encode('ASCII')
		if lossy:
			basis = basis[0:7-len(tail)] + '~' + tail + basis[8:11]
		basis = basis.encode('ASCII')
		
		# Make sure short name is unique in parent directory?
			# Try other numeric tails
		
		# Get the checksum of the short name
		checksum = fat_checksum(basis)
		
		# Generate the long name entries
		order = 1
		while len(name) > 0:
			if len(name) < 14:
				order += LAST_LONG_ENTRY
			entries.insert(0, make_ldir(name[:13], order, checksum))
			name = name[13:]
			order +=1
		
		# Add the short name entry
		entries.append(make_dirent(basis, attr, head, length, ntres,
					create_time_tenth, create_time, create_date, access_date,
					mod_time, mod_date))
	else:
		# Space pad the base and extension
		base = (str(base) + "       ")[0:8]
		ext = (str(ext) + "   ")[0:3]
		
		# Determine the value for "ntres"
		if base.islower():
			base = base.upper()
			ntres |= 0x08
		if ext.islower():
			ext = ext.upper()
			ntres |= 0x10
		
		entries = [make_dirent(base+ext, attr, head, length, ntres,
					create_time_tenth, create_time, create_date, access_date,
					mod_time, mod_date)]
	
	return "".join(entries)

class msdosfs(object):
	def __init__(self, dev):
		self.dev = dev
		
		#
		# TODO: Set the sector size based on the device's info.
		#
		
		#
		# Read and parse the boot block
		#
		dev.seek(0)
		bs = dev.read(512)
		if ord(bs[0]) != 0xE9 and (ord(bs[0]) != 0xEB or ord(bs[2]) != 0x90):
			raise RuntimeError("Invalid boot jump signature")
		v = struct.unpack("<H", bs[11:13])[0]
		if v < 512 or v > 4096 or (v & (v-1)):
			raise RuntimeError("Invalid bytes per sector")
		self.bytesPerSector = v
		v = ord(bs[13])
		if (v & (v - 1)) or (self.bytesPerSector * v) > 65536:
			raise RuntimeError("Invalid sectors per cluster")
		sectorsPerCluster = v
		self.bytesPerCluster = v * self.bytesPerSector
		self.reservedSectors = struct.unpack("<H", bs[14:16])[0]
		self.numFATs = ord(bs[16])
		self.rootEntryCount = struct.unpack("<H", bs[17:19])[0]
		v = struct.unpack("<H", bs[19:21])[0]
		if v:
			self.totalSectors = v
		else:
			self.totalSectors = struct.unpack("<I", bs[32:36])[0]
		v = struct.unpack("<H", bs[22:24])[0]
		if v:
			self.fatSectors = v
		else:
			self.fatSectors = struct.unpack("<I", bs[36:40])[0]
		self.fsinfo = None
		
		#
		# Figure out the bits per FAT entry, and create an object for the FAT
		#
		rootSectors = ((self.rootEntryCount * 32) + self.bytesPerSector - 1) / self.bytesPerSector
		self.rootSectors = rootSectors
		clustersStart = self.reservedSectors + (self.numFATs * self.fatSectors) + rootSectors
		self.clusterStart = clustersStart
		dataSectors = self.totalSectors - clustersStart
		self.clusters = clusters = dataSectors / sectorsPerCluster
		if clusters < 4085:
			self.type = 12
			self.fat = self.FAT12(self)
		elif clusters < 65525:
			self.type = 16
			self.fat = self.FAT16(self)
		else:
			self.type = 32
			self.fat = self.FAT32(self)
			self.rootCluster = struct.unpack("<I", bs[44:48])[0]
			fsinfo = struct.unpack("<H", bs[48:50])[0]
			if fsinfo:
				self.fsinfo = self.FSInfo(self, fsinfo)
	
	def ReadCluster(self, cluster, count=1):
		"""Return the contents of cluster <cluster>"""
		assert cluster >= 2
		offset = (self.clusterStart * self.bytesPerSector) + ((cluster-2) * self.bytesPerCluster)
		self.dev.seek(offset)
		return self.dev.read(count * self.bytesPerCluster)
		
	def WriteCluster(self, cluster, bytes):
		"""Return the contents of cluster <cluster>"""
		assert cluster >= 2
		assert (len(bytes) % self.bytesPerSector) == 0
		offset = (self.clusterStart * self.bytesPerSector) + ((cluster-2) * self.bytesPerCluster)
		self.dev.seek(offset)
		return self.dev.write(bytes)
	
	def flush(self):
		self.fat.flush()
		if self.fsinfo:
			self.fsinfo.flush()
	
	def allocate(self, count, start=CLUST_FIRST, last=CLUST_EOF):
		"""
		Allocate and create a chain of count clusters.
		Searching begins at cluster #start.
		The last cluster of the chain will point at cluster #last.
		Returns the first cluster of the chain.
		
		Unlike FAT.allocate, this routine will adjust the free space in the
		FSInfo sector (applies to FAT32 only).
		"""
		cluster = self.fat.chain(self.fat.find(count, start), last)
		if cluster and self.fsinfo:
			self.fsinfo.allocate(count)
		return cluster

	class FAT(object):
		"""Base class to represent the File Allocation Table.  Do not
		instantiate this class; use FAT12, FAT16 or FAT32 instead."""

		def __init__(self, dev, bootSector, clusters):
			raise NotImplementedError("Do not instantiate class FAT directly")
		
		def __del__(self):
			self.flush()
			
		def find(self, count, start=CLUST_FIRST):
			"""Return a list of <count> free clusters"""
			
			if count < 1:
				raise ValueError("Invalid count of clusters")
			
			clusters = []
			for cluster in xrange(start, self.clusters):
				if self[cluster] == CLUST_FREE:
					clusters.append(cluster)
					count -= 1
					if count == 0:
						break
			
			if count:
				raise RuntimeError("Insufficient free space")
				
			return clusters
		
		def find_contig(self, count, start=CLUST_FIRST):
			"""Return an iterable of <count> contiguous free clusters"""
			raise NotImplementedError()
		
		def chain(self, clusters, last=CLUST_EOF):
			"""Create a cluster chain (allocate clusters).  The cluster numbers
			are in <clusters>.  The last cluster in the chain will point to
			cluster <last>.  The cluster number of the first cluster in the
			chain is returned."""
			it = iter(clusters)
			head = prev = it.next()
			for cluster in it:
				self[prev] = cluster
				prev = cluster
			self[prev] = last
			return head
			
		def allocate(self, count, start=CLUST_FIRST, last=CLUST_EOF):
			return self.chain(self.find(count, start), last)
		
		def allocate_contig(self, count, start=CLUST_FIRST, last=CLUST_EOF):
			return self.chain(self.find_contig(count, start), last)
	
	class FAT32(FAT):
		"A File Allocation Table with 32-bit entries."
	
		def __init__(self, vol):
			self.dev = vol.dev		# File object to use for I/O
			self.reservedSectors = vol.reservedSectors
			self.bytesPerSector = vol.bytesPerSector
			self.clusters = vol.clusters	# Number of clusters + 2
			self.sector = None	# Sector number of cached sector
			self.bytes = None	# Raw bytes from cached sector
			self.entries = None	# Array of entries from cached sector
			self.entriesPerSector = self.bytesPerSector / 4
			self.dirty = False
			
		def _cache(self, cluster):
			"""Make sure the FAT sector containing <cluster> is cached."""
			
			sector = cluster / self.entriesPerSector
			if sector != self.sector:
				self.flush()
				self.sector = sector
				self.dev.seek((sector + self.reservedSectors) * self.bytesPerSector)
				self.bytes = self.dev.read(self.bytesPerSector)
				self.entries = list(struct.unpack("<%dI" % self.entriesPerSector, self.bytes))

		def __getitem__(self, key):
			#
			# Make sure the requested cluster number is valid
			#
			if key < 0 or key >= self.clusters:
				raise IndexError("cluster number out of range")
			
			#
			# Make sure we have the correct sector cached
			#
			self._cache(key)
			
			#
			# Return the desired entry from the current sector.
			# For reserved/bad/EOF values, extend to full 32 bits.
			#
			value = self.entries[key % self.entriesPerSector] & 0x0FFFFFFF
			if value >= (CLUST_RSRVD & 0x0FFFFFFF):
				value |= 0xF0000000
			return value
			
		def __setitem__(self, key, value):
			#
			# Make sure the requested cluster number is valid
			#
			if key < 0 or key >= self.clusters:
				raise IndexError("cluster number out of range")
			
			#
			# Make sure the value is valid
			#
			if CLUST_RSRVD <= value <= CLUST_EOF:
				value = value & 0x0FFFFFFF
			if value < 0 or value > CLUST_MAX32:
				raise ValueError("cluster value out of range")
			
			#
			# Make sure we have the correct sector cached
			#
			self._cache(key)
			
			#
			# Set the desired entry to the given value.
			# Only updates the low 28 bits.
			#
			old = self.entries[key % self.entriesPerSector]
			value = (value & 0x0FFFFFFF) | (old & 0xF0000000)
			self.entries[key % self.entriesPerSector] = value
			self.dirty = True
			
		def flush(self):
			"Write any pending changes to disk"
			
			if not self.dirty:
				return
			
			if len(self.entries) != self.entriesPerSector:
				raise RuntimeError("FAT entries corrupt!")
			
			#
			# Only write to disk if the data has changed
			#
			bytes = struct.pack("<%dI" % self.entriesPerSector, *self.entries)
			if bytes != self.bytes:
				self.bytes = bytes
				self.dev.seek((self.sector + self.reservedSectors) * self.bytesPerSector)
				self.dev.write(bytes)

			self.dirty = False
			
	class FAT16(FAT):
		"A File Allocation Table with 16-bit entries."
	
		def __init__(self, vol):
			self.dev = vol.dev		# File object to use for I/O
			self.reservedSectors = vol.reservedSectors
			self.bytesPerSector = vol.bytesPerSector
			self.clusters = vol.clusters	# Number of clusters + 2
			self.sector = None	# Sector number of cached sector
			self.bytes = None	# Raw bytes from cached sector
			self.entries = None	# Array of entries from cached sector
			self.entriesPerSector = self.bytesPerSector / 2
			self.dirty = False
			
		def _cache(self, cluster):
			"""Make sure the FAT sector containing <cluster> is cached."""
			
			sector = cluster / self.entriesPerSector
			if sector != self.sector:
				self.flush()
				self.sector = sector
				self.dev.seek((sector + self.reservedSectors) * self.bytesPerSector)
				self.bytes = self.dev.read(self.bytesPerSector)
				self.entries = list(struct.unpack("<%dH" % self.entriesPerSector, self.bytes))

		def __getitem__(self, key):
			#
			# Make sure the requested cluster number is valid
			#
			if key < 0 or key >= self.clusters:
				raise IndexError("cluster number out of range")
			
			#
			# Make sure we have the correct sector cached
			#
			self._cache(key)
			
			#
			# Return the desired entry from the current sector.
			# For reserved/bad/EOF values, extend to full 32 bits.
			#
			value = self.entries[key % self.entriesPerSector]
			if value >= (CLUST_RSRVD & 0x0000FFFF):
				value |= 0xFFFF0000
			return value
			
		def __setitem__(self, key, value):
			#
			# Make sure the requested cluster number is valid
			#
			if key < 0 or key >= self.clusters:
				raise IndexError("cluster number out of range")
			
			#
			# Make sure the value is valid
			#
			if CLUST_RSRVD <= value <= CLUST_EOF:
				value = value & 0x0000FFFF
			if value < 0 or value > CLUST_MAX16:
				raise ValueError("cluster value out of range")
			
			#
			# Make sure we have the correct sector cached
			#
			self._cache(key)
			
			#
			# Set the desired entry to the given value.
			#
			self.entries[key % self.entriesPerSector] = value
			self.dirty = True
			
		def flush(self):
			"Write any pending changes to disk"
			
			if not self.dirty:
				return
			
			if len(self.entries) != self.entriesPerSector:
				raise RuntimeError("FAT entries corrupt!")
			
			#
			# Only write to disk if the data has changed
			#
			bytes = struct.pack("<%dH" % self.entriesPerSector, *self.entries)
			if bytes != self.bytes:
				self.bytes = bytes
				self.dev.seek((self.sector + self.reservedSectors) * self.bytesPerSector)
				self.dev.write(bytes)

			self.dirty = False
			
	class FAT12(FAT):
		"A File Allocation Table with 12-bit entries."
	
		def __init__(self, vol):
			self.vol = vol
			self.dev = vol.dev		# File object to use for I/O
			self.clusters = vol.clusters	# Number of clusters + 2
			self.dirty = False
			
			# Read in the entire FAT, converting it to the self.entries array.
			self.dev.seek(vol.reservedSectors * vol.bytesPerSector)
			bytes = self.dev.read(vol.fatSectors * vol.bytesPerSector)
			
			# We always unpack a multiple of two entries, for convenience.
			self.entries = [0] * (self.clusters + 1)
			for i in xrange(0, self.clusters, 2):
				index = i * 3 / 2
				self.entries[i]   = struct.unpack("<H", bytes[index:index+2])[0] & 0x0FFF
				self.entries[i+1] = struct.unpack("<H", bytes[index+1:index+3])[0] >> 4
			
		def __getitem__(self, key):
			#
			# Make sure the requested cluster number is valid
			#
			if key < 0 or key >= self.clusters:
				raise IndexError("cluster number out of range")
			
			#
			# Return the desired entry from the current sector.
			# For reserved/bad/EOF values, extend to full 32 bits.
			#
			value = self.entries[key]
			if value >= (CLUST_RSRVD & 0x00000FFF):
				value |= 0xFFFFF000
			return value
			
		def __setitem__(self, key, value):
			#
			# Make sure the requested cluster number is valid
			#
			if key < 0 or key >= self.clusters:
				raise IndexError("cluster number out of range")
			
			#
			# Make sure the value is valid
			#
			if CLUST_RSRVD <= value <= CLUST_EOF:
				value = value & 0x00000FFF
			if value < 0 or value > CLUST_MAX12:
				raise ValueError("cluster value out of range")
			
			#
			# Set the desired entry to the given value.
			#
			self.entries[key] = value
			self.dirty = True
			
		def flush(self):
			"Write any pending changes to disk"
			
			if not self.dirty:
				return
			
			if len(self.entries) != self.clusters + 1:
				raise RuntimeError("FAT entries corrupt!")
			
			vol = self.vol
			
			# Read the old data from disk
			self.dev.seek(vol.reservedSectors * vol.bytesPerSector)
			bytes = self.dev.read(vol.fatSectors * vol.bytesPerSector)

			# Update the bytes with values from self.entries
			for i in xrange(0, self.clusters, 2):
				index = i * 3 / 2
				pair = struct.pack("<I", self.entries[i] + (self.entries[i+1] << 12))
				bytes = bytes[:index] + pair[:3] + bytes[index+3:]
			assert len(bytes) == (vol.fatSectors * vol.bytesPerSector)
			
			# Write back to disk
			self.dev.seek(vol.reservedSectors * vol.bytesPerSector)
			self.dev.write(bytes)
			
			self.dirty = False
			
	class Chain(object):
		"""A chain of clusters (i.e. a file or directory)"""
		def __init__(self, volume, head):
			self.volume = volume
			self.head = head

		def cmap(self, offset):
			"""Return the cluster containing the chain's given byte <offset>.
			Returns <None> if the given offset is not part of the cluster
			chain."""
			
			bytesPerCluster = self.volume.bytesPerCluster
			cluster = self.head
			if cluster == CLUST_FREE:
				return None
			
			while offset >= bytesPerCluster:
				cluster = self.volume.fat[cluster]
				if cluster == CLUST_FREE or cluster >= CLUST_RSRVD:
					return None
				offset -= bytesPerCluster
			
			return cluster
			
		def pread(self, offset=0, count=None):
			"""Read up to <count> bytes at <offset> bytes from the start of
			the chain.  If <count> is <None>, then read all bytes until the
			end of the chain."""
			
			if count == 0:
				return ""
			
			result = []
			bytesPerCluster = self.volume.bytesPerCluster
			cluster = self.cmap(offset)
			if not cluster:
				return ""
			
			# Handle non-aligned start of read
			ofs = offset % bytesPerCluster
			if ofs:
				buf = self.volume.ReadCluster(cluster)
				length = bytesPerCluster - ofs
				if count is not None and count < length:
					length = count
				result.append(buf[ofs:ofs+length])
				if count is not None:
					count -= length
				cluster = self.volume.fat[cluster]
				if cluster == CLUST_FREE or cluster >= CLUST_RSRVD:
					return result[0]
			
			# Handle whole clusters
			while count != 0:
				if count is not None and count < bytesPerCluster:
					break
				result.append(self.volume.ReadCluster(cluster))
				cluster = self.volume.fat[cluster]
				if cluster == CLUST_FREE or cluster >= CLUST_RSRVD:
					return "".join(result)
				if count is not None:
					count -= bytesPerCluster
			
			# Handle non-aligned end of read
			if count is not None and count > 0:
				buf = self.volume.ReadCluster(cluster)
				result.append(buf[0:count])
			
			return "".join(result)
		
		def pwrite(self, offset, bytes):
			"""Write <bytes> at <offset> bytes from the start of the chain."""
			count = len(bytes)
			if count == 0:
				return
			
			bytesPerCluster = self.volume.bytesPerCluster
			cluster = self.cmap(offset)
			if not cluster:
				raise RuntimeError("Write beyond end of cluster chain")
			
			# Handle non-aligned start of write
			ofs = offset % bytesPerCluster
			if ofs:
				buf = self.volume.ReadCluster(cluster)
				length = bytesPerCluster - ofs
				if count < length:
					length = count
				buf = buf[0:ofs] + bytes[0:length] + buf[ofs+length:]
				assert len(buf) == bytesPerCluster
				self.volume.WriteCluster(cluster, buf)
				ofs = length
				count -= length
				cluster = self.volume.fat[cluster]
			
			# Handle whole clusters
			while count >= bytesPerCluster:
				if cluster == CLUST_FREE or cluster >= CLUST_RSRVD:
					raise RuntimeError("Write beyond end of cluster chain")
				self.volume.WriteCluster(cluster, bytes[ofs:ofs+bytesPerCluster])
				ofs += bytesPerCluster
				count -= bytesPerCluster
				cluster = self.volume.fat[cluster]

			# Handle non-aligned end of write
			if count > 0:
				if cluster == CLUST_FREE or cluster >= CLUST_RSRVD:
					raise RuntimeError("Write beyond end of cluster chain")
				buf = self.volume.ReadCluster(cluster)
				buf = bytes[ofs:ofs+count] + buf[count:]
				assert len(buf) == bytesPerCluster
				self.volume.WriteCluster(cluster, buf)
			
	class Directory(Chain):
		def __init__(self, volume, head):
			super(volume.Directory, self).__init__(volume, head)
			if volume.type == 32 and head == volume.rootCluster:
				self.is_root = True
			else:
				self.is_root = False
		
		def find_slots(self, count, grow=True):
			"""Find <count> contiguous free slots.  If <grow> is true then extend
			the directory to make enough space.  If there is insufficient free
			space, and the directory can't be grown, raise an error.  Returns
			the slot index of the found space."""
			assert count > 0
			bytesPerCluster = self.volume.bytesPerCluster
			slot = 0
			found = 0
			cluster = self.head
			while cluster >= CLUST_FIRST and cluster < CLUST_RSRVD:
				buf = self.volume.ReadCluster(cluster)
				offset = 0
				while offset < bytesPerCluster:
					if buf[offset] == '\x00' or buf[offset] == '\xE5':
						found += 1
						if found >= count:
							# <slot> is the index of the last of the slots
							return slot-count+1
					else:
						found = 0
					offset += 32
					slot += 1
				cluster = self.volume.fat[cluster]
			
			if grow:
				raise NotImplementedError("Growing directories not implemented")
		
			raise RuntimeError("Insufficient space in directory")
		
		def read_slots(self, slot, count=1):
			"""Read and return <count> consecutive directory slots, starting
			at slot number <slot>."""
			return self.pread(slot*32, count*32)
			
		def write_slots(self, slot, bytes):
			"""Write <data> to consecutive directory slots, starting at slot
			number <slot>."""
			assert len(bytes) > 0 and (len(bytes) % 32) == 0
			self.pwrite(slot*32, bytes)
		
		def mkfile(self, name, head=0, length=0):
			"Create a file entry in the directory."
			#
			# Construct the raw directory entry (entries if long name)
			#
			bytes = make_long_dirent(name, ATTR_ARCHIVE, head=head, length=length)
			
			#
			# Find enough free slots, growing the directory if needed
			#
			slots = len(bytes)/32
			slot = self.find_slots(slots)
			
			#
			# Write the raw entry/entries to the free slots
			#
			self.write_slots(slot, bytes)
		
		def mkdir(self, name, clusters=None, length=0, makeDot=True, makeDotDot=True, zeroFill=True):
			"""
			Create a subdirectory entry in the parent directory.
			By default, it also creates the "." and ".." entries in the first
			two slots of the first cluster of the subdirectory.  By default,
			the remaining slots and remaining clusters will be zero filled.
			
			The "clusters" parameter should be an iterable containing the
			clusters to be allocated to the subdirectory.  If the default,
			None, is used, then one cluster will be found and allocated.
			
			The "length" parameter overrides the length/size field in the
			subdirectory's entry.  This should normally be left at the
			default, 0.
			
			The "makeDot" and "makeDotDot" parameters may be set to False to
			avoid initializing the "." and ".." entries, respectively.
			
			The "zeroFill" parameter may be set to False to avoid filling the
			clusters with zeroes.
			
			If one or more clusters were successfully allocated and the
			subdirectory created, a Directory object for the new subdirectory
			is returned.
			"""
			
			if clusters is None:
				clusters = self.volume.fat.find(1)
				
			# See if there is a first cluster in the chain
			head = 0
			try:
				head = clusters[0]
			except IndexError:
				pass
			except TypeError:
				pass
			
			# Create the subdirectory's entry in the parent
			bytes = make_long_dirent(name, ATTR_DIRECTORY, head=head, length=length)
			slots = len(bytes) / 32
			self.write_slots(self.find_slots(slots), bytes)
			
			subdir = None
			# If there is at least one cluster, then zero fill and
			# create the "." and ".." entries
			if head:
				# Zero fill unless told not to
				if zeroFill:
					zeroes = "\x00" * self.volume.bytesPerCluster
					for cluster in clusters:
						self.volume.WriteCluster(cluster, zeroes)
				
				# Allocate the cluster(s)
				self.volume.fat.chain(clusters)
				
				# Create a Directory object for the new subdirectory
				# so that we can write to it conveniently
				subdir = self.volume.Directory(self.volume, head)

				# Create "."
				if makeDot:
					bytes = make_dirent('.          ', ATTR_DIRECTORY, head=head)
					subdir.write_slots(0, bytes)
				
				# Create ".."
				if makeDotDot:
					# If "self" is the root directory, then the first cluster
					# for ".." should be set to 0.
					if self.is_root:
						headDotDot = 0
					else:
						headDotDot = self.head
					bytes = make_dirent('..         ', ATTR_DIRECTORY, head=headDotDot)
					subdir.write_slots(1, bytes)
			
			# Return the subdirectory's Directory object (if any)
			return subdir
	
	class FixedRootDir(Directory):
		def __init__(self, volume):
			self.volume = volume
			self.head = 0
			self.is_root = True
			
		def pread(self, offset=0, count=None):
			"""Read up to <count> bytes at <offset> bytes from the start of
			the chain.  If <count> is <None>, then read all bytes until the
			end of the chain."""
			
			if count == 0:
				return ""
			
			volume = self.volume
			bytesPerSector = volume.bytesPerSector
			firstSector = volume.reservedSectors + (volume.numFATs * volume.fatSectors)
			
			if count is None:
				count = (volume.rootSectors * bytesPerSector) - offset
			
			if (offset + count) > (volume.rootSectors * bytesPerSector):
				raise RuntimeError("Read past end of fixed root directory")
			
			# Figure out the starting and ending sectors (relative to root dir)
			sector = offset / bytesPerSector
			endSector = (offset + count + bytesPerSector - 1) / bytesPerSector
			
			# Read whole sectors
			volume.dev.seek((sector + firstSector) * bytesPerSector)
			bytes = volume.dev.read((endSector - sector) * bytesPerSector)
			
			# Return just the portion the caller asked for
			offset = offset % bytesPerSector
			return bytes[offset:offset+count]
		
		def pwrite(self, offset, bytes):
			"""Write <bytes> at <offset> bytes from the start of the chain."""
			count = len(bytes)
			if count == 0:
				return
			
			volume = self.volume
			bytesPerSector = volume.bytesPerSector
			firstSector = volume.reservedSectors + (volume.numFATs * volume.fatSectors)
			
			if count is None:
				count = (volume.rootSectors * bytesPerSector) - offset
			
			if (offset + count) > (volume.rootSectors * bytesPerSector):
				raise RuntimeError("Write past end of fixed root directory")
			
			# Figure out the starting and ending sectors (relative to root dir)
			sector = offset / bytesPerSector
			endSector = (offset + count + bytesPerSector - 1) / bytesPerSector
			
			if offset % bytesPerSector or count % bytesPerSector:
				offset = offset % bytesPerSector

				# Read whole sectors
				volume.dev.seek((sector + firstSector) * bytesPerSector)
				original = volume.dev.read((endSector - sector) * bytesPerSector)
				
				# Overwrite with caller supplied data
				bytes = original[:offset] + bytes + original[offset+count:]
			
			# Write out the (updated) sectors
			volume.dev.seek((sector + firstSector) * bytesPerSector)
			volume.dev.write(bytes)

		def find_slots(self, count, grow=False):
			"""Find <count> contiguous free slots.  If there is insufficient
			free space, then raise an error.  Returns the slot index of the
			found space."""
			assert count > 0
			volume = self.volume
			bytesPerSector = volume.bytesPerSector
			firstSector = volume.reservedSectors + (volume.numFATs * volume.fatSectors)
			slot = 0
			found = 0
			for sector in xrange(firstSector, firstSector+volume.rootSectors):
				volume.dev.seek(sector * bytesPerSector)
				buf = volume.dev.read(bytesPerSector)
				offset = 0
				while offset < bytesPerSector:
					if buf[offset] == '\x00' or buf[offset] == '\xE5':
						found += 1
						if found >= count:
							# <slot> is the index of the last of the slots
							return slot-count+1
					else:
						found = 0
					offset += 32
					slot += 1
			
			raise RuntimeError("Insufficient space in directory")
	
	def root(self):
		"Return an object for the root directory."
		if self.type == 32:
			return self.Directory(self, self.rootCluster)
		else:
			return self.FixedRootDir(self)

	class FSInfo(object):
		def __init__(self, volume, sector):
			self.volume = volume
			self.sector = sector
			self.valid = False
			self.dirty = False
			self.free = 0xFFFFFFFF
			self.nextFree= 0
			
			volume.dev.seek(volume.bytesPerSector * sector)
			fsinfo = volume.dev.read(volume.bytesPerSector)
			if fsinfo[0:4] == 'RRaA' and fsinfo[484:488] == 'rrAa':
				self.valid = True
				self.free, self.nextFree = struct.unpack("<II", fsinfo[488:496])
		
		def allocate(self, clusters):
			if self.valid:
				self.free -= clusters
				self.dirty = True
		
		def flush(self):
			if self.valid and self.dirty:
				self.volume.dev.seek(self.volume.bytesPerSector * self.sector)
				fsinfo = self.volume.dev.read(self.volume.bytesPerSector)
				fsinfo = fsinfo[0:488] + struct.pack("<II", self.free, self.nextFree) + fsinfo[496:]
				self.volume.dev.seek(self.volume.bytesPerSector * self.sector)
				self.volume.dev.write(fsinfo)
				self.dirty = False

if __name__ == "__main__":
	import sys
	from HexDump import HexDump
	
	dev = file(sys.argv[1], "r+")
	v = msdosfs(dev)
	r = v.root()
	head = v.fat.allocate(7, 100)
	v.root().mkfile('FOO        ', head=head, length=6*v.bytesPerCluster)
	v.flush()
	del v
	dev.close()
	del dev
