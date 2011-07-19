
sieve_subdirs = \
	lib-sieve \
	plugins \
	lib-sieve-tool \
	sieve-tools \
	testsuite

if BUILD_MANAGESIEVE
managesieve_subdirs = \
	lib-managesieve \
	lib-sievestorage \
	managesieve \
	managesieve-login
else
managesieve_subdirs =
endif

SUBDIRS = \
	$(sieve_subdirs) \
	$(managesieve_subdirs)
