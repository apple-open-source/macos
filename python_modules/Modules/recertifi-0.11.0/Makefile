NAME = recertifi
VERSION = 0.11.0

SETUPARGS:= --install-purelib=/AppleInternal/Library/Python/2.7/site-packages

include ../Makefile.inc

postinstall:
	echo "import recertifi" >$(DSTROOT)//AppleInternal/Library/Python/2.7/site-packages/recertifi.pth
	chmod -R g-w $(DSTROOT)//AppleInternal/Library/Python/2.7/site-packages
	echo "# this file intentionally left blank" >>$(DSTROOT)/AppleInternal/Library/Python/2.7/site-packages/recertifi_cli/__init__.py
