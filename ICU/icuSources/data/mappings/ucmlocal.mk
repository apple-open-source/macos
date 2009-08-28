#
# Local Apple addition for mapping resources
# Copyright (c) 2004-2007, 2008 Apple Inc. All rights reserved.
#

ifeq "$(APPLE_EMBEDDED)" "YES"
	UCM_SOURCE_LOCAL = iso-8859_10-1998.ucm\
		iso-8859_16-2001.ucm\
		gsm-03.38-2000.ucm\
		softbank-sjis.ucm
else
	UCM_SOURCE_LOCAL = iso-8859_10-1998.ucm\
		iso-8859_16-2001.ucm\
		gsm-03.38-2000.ucm
endif
