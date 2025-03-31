
CFLAGS += -fno-omit-frame-pointer

all: sparseme netwrapd

run: sparseme netwrapd
	./generate-kyua
	-kyua test

count:
	ls *.test | wc -l

run-orsync2grsync2:
	rsync="openrsync --rsync-path=rsync2" make run

run-orsync2grsync:
	rsync="openrsync --rsync-path=rsync" make run

run-grsync22orsync:
	rsync="rsync2 --rsync-path=opensync" make run

run-grsync2orsync:
	rsync="rsync --rsync-path=opensync" make run

