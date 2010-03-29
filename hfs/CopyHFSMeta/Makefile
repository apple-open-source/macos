Project = CopyHFSMeta
ProductType = tool
Install_Dir = /System/Library/Filesystems/hfs.fs/Contents/Resources

CFILES= main.c misc.c ScanExtents.c dump.c SparseBundle.c \
	DeviceWrapper.c Gather.c \
	${OBJROOT}/${Project}/_version.c

${OBJROOT}/${Project}/_version.c:
	/Developer/Makefiles/bin/version.pl CopyHFSMeta > $@

Extra_LD_Flags= -framework CoreFoundation -lz

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make


