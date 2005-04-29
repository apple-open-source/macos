EXTRA_DIST = README Changes Credits srm.spec.in srm.spec
SUBDIRS = lib src doc

rpm: dist
	rpmbuild -tb $(distdir).tar.gz
	mv ~/src/rpm/RPMS/i386/srm* .
