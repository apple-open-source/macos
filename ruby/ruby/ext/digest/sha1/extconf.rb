# $RoughId: extconf.rb,v 1.3 2001/08/14 19:54:51 knu Exp $
# $Id: extconf.rb,v 1.5 2003/08/06 03:11:14 nobu Exp $

require "mkmf"

$CFLAGS << " -DHAVE_CONFIG_H -I#{File.dirname(__FILE__)}/.."
$CPPFLAGS << " -DHAVE_CONFIG_H -I#{File.dirname(__FILE__)}/.."

$objs = [ "sha1init.#{$OBJEXT}" ]

dir_config("openssl")

if !with_config("bundled-sha1") &&
    have_library("crypto") && have_header("openssl/sha.h")
  $objs << "sha1ossl.#{$OBJEXT}"
else
  $objs << "sha1.#{$OBJEXT}" << "sha1hl.#{$OBJEXT}"
end

have_header("sys/cdefs.h")

have_header("inttypes.h")

have_header("unistd.h")

create_makefile("digest/sha1")
