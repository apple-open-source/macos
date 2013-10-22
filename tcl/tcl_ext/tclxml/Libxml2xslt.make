##
# Makefile support for configuring libxml2/libxslt
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

Libxml2_Prefix          = $(SDKROOT)$(Install_Prefix)
Libxml2_Configure_Flags = --with-libxml2-include=$(Libxml2_Prefix)/include \
                          --with-libxml2-lib=$(Libxml2_Prefix)/lib
Libxslt_Prefix          = $(SDKROOT)$(Install_Prefix)
Libxslt_Configure_Flags = --with-libxslt-include=$(Libxslt_Prefix)/include \
                          --with-libxslt-lib=$(Libxslt_Prefix)/lib \
                          --with-libexslt-include=$(Libxslt_Prefix)/include \
                          --with-libexslt-lib=$(Libxslt_Prefix)/lib
