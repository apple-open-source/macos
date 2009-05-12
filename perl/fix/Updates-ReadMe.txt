In the module search order (@INC), this directory comes before the system
perl directory, so modules installed here can be used to update perl-standard
modules.  The installprivlib Config parameter is now set to this directory,
so that (for instance) setting INSTALLDIRS=perl will cause modules to be
install here
