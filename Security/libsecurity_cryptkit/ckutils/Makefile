#
# Top-level Makefile for ckutils. Allows build or clean
# of all directories in one swoop.  
#
SHELL := /bin/zsh

SUBDIRS= atomTime badsig blobtest cfileTest giantAsmBench giantBench giantDvt

first:
	@foreach i in $(SUBDIRS); \
		echo "=== Making $$i ==="; \
		cd $$i; \
		make || exit; \
		cd ..; \
	end

clean:
	@foreach i in $(SUBDIRS); \
		echo "=== Cleaning $$i ==="; \
		(cd $$i; make clean;) \
	end
