#	$Id: Solaris.m4,v 1.1.1.1 2000/06/10 00:40:47 wsanchez Exp $
depend: ${BEFORE} ${LINKS}
	@mv Makefile Makefile.old
	@sed -e '/^# Do not edit or remove this line or anything below it.$$/,$$d' < Makefile.old > Makefile
	@echo "# Do not edit or remove this line or anything below it." >> Makefile
	${CC} -xM ${COPTS} ${SRCS} >> Makefile

#	End of $RCSfile: Solaris.m4,v $
