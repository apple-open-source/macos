<!-- $XFree86: xc/programs/Xserver/hw/xfree86/doc/sgml/mdefs.cpp,v 1.2 2003/03/19 01:49:28 dawes Exp $ -->

<!-- entity definitions for man pages -->

#ifdef HTML_MANPAGES
<!ENTITY % manpages 'INCLUDE'>
#else
<!ENTITY % manpages 'IGNORE'>
#endif

#ifdef HTML_SPECS
<!ENTITY % specdocs 'INCLUDE'>
#else
<!ENTITY % specdocs 'IGNORE'>
#endif

<!ENTITY drvsuffix CDATA __drivermansuffix__ >
<!ENTITY filesuffix CDATA __filemansuffix__ >
<!ENTITY miscsuffix CDATA __miscmansuffix__ >

