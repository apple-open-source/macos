/*
 * normalize local -l* library conventions
 *
 * L [ P1 ... Pn ] :MAPLIB: T1.c ... Tn.c
 *
 * if Pi not specified then P1 == L
 * the first Ti.c that compiles/links with -lP1 ... -lPn
 * but does not compile/link with no libraries maps
 * -lL to to require -lP1 ... -lPn
 * otherwise -lL is not required and maps to "no library required"
 */

":MAPLIB:" : .MAKE .OPERATOR
	local L P
	L := $(<:B:O=1)
	if ! ( P = "$(<:B:O>1)" )
		P := $(L)
	end
	$(LIBDIR)/lib/$(L) :INSTALL: $(L).req
	eval
	$(L).req : (CC) $$(>)
		r='-'
		for i in $$(*)
		do	if	$$(CC) -o $$(<:B:S=.exe) $i $(P:/^/-l) > /dev/null
			then	$$(CC) -o $$(<:B:S=.exe) $i > /dev/null || {
					r='$(P:/^/-l)'
					break
				}
			fi
		done 2>/dev/null
		echo " $r" > $$(<)
		rm -f $$(<:B:S=.exe) $$(*:B:S=$$(CC.SUFFIX.OBJECT))
	end
