# $RoughId: extconf.rb,v 1.3 2001/11/24 17:49:26 knu Exp $
# $Id: extconf.rb,v 1.1.1.1 2002/05/27 17:59:47 jkh Exp $

require 'mkmf'

have_header("syslog.h") &&
  have_func("openlog") &&
  have_func("setlogmask") &&
  create_makefile("syslog")

