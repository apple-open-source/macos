#
# this is used/needed by the APACHE2 build system
#

MOD_FASTCGI = mod_fastcgi fcgi_pm fcgi_util fcgi_protocol fcgi_buf fcgi_config

mod_fastcgi.la: ${MOD_FASTCGI:=.slo}
	$(SH_LINK) -rpath $(libexecdir) -module -avoid-version ${MOD_FASTCGI:=.lo}

DISTCLEAN_TARGETS = modules.mk

shared =  mod_fastcgi.la

