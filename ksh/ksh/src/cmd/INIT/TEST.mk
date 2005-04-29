/*
 * regression test support
 *
 * @(#)TEST.mk (AT&T Labs Research) 2003-03-11
 *
 * test management is still in the design phase
 */

/*
 * three forms for :TEST:
 *
 *	:TEST: xxx yyy ...
 *
 *		$(REGRESS) $(REGRESSFLAGS) xxx.tst xxx
 *		$(REGRESS) $(REGRESSFLAGS) yyy.tst yyy
 *
 *	:TEST: xxx.tst yyy ...
 *
 *		$(REGRESS) $(REGRESSFLAGS) xxx.tst yyy ...
 *
 *	xxx :TEST: prereq ...
 *		[ action ]
 *
 *		$(*) if no action
 */

":TEST:" : .MAKE .OPERATOR
	local B P S T
	test : .INSERT .TESTINIT
	P := $(>:O=1)
	if "$(P:N=*.tst)" && ! "$(@:V)"
		B := $(P:B)
		if ! ( T = "$(<:V)" )
			T := $(B)
		end
		test : - test.$(T)
		eval
			test.$$(T) : $$(B).tst
				$$(REGRESS) $$(REGRESSFLAGS) $$(*) $(>:V:O>1)
		end
	elif "$(P:N=*@(.sh|$(.SUFFIX.c:/ /|/G)|$(.SUFFIX.C:/ /|/G)))"
		B := $(P:B)
		if ! ( T = "$(<:V)" )
			T := $(B)
		end
		:INSTALLDIR: $(B)
		$(B) :: $(P)
		if "$(P:N=*.sh)"
			TESTCC == $(CC)
			$(B) : (TESTCC)
		end
		test : - test.$(T)
		if "$(@:V)"
			eval
				test.$$(T) : $$(B) $(>:V:O>1)
					$(@:V)
			end
		else
			eval
				test.$$(T) : $$(B)
					$$(*) $(>:V:O>1)
			end
		end
	elif ! "$(<:V)"
		for B $(>)
			if ! "$(B:A=.TARGET)"
				for S .c .sh
					if "$(B:B:S=$(S):T=F)"
						:INSTALLDIR: $(B)
						$(B) :: $(B:B:S=$(S))
						break
					end
				end
			end
			test : - test.$(B)
			test.$(B) : $(B).tst $(B:A=.TARGET)
				$(REGRESS) $(REGRESSFLAGS) $(*)
		end
	else
		if "$(>:V)" || "$(@:V)"
			P := $(>)
			T := $(P:O=1)
			B := $(T:B)
			if "$(T)" != "$(B)" && "$(T:G=$(B))"
				:INSTALLDIR: $(B)
				$(B) :: $(T) $(P:O>1:N=-*)
				T := $(B)
				P := $(B) $(P:O>1:N!=-*)
			end
			if "$(<:V)"
				T := $(<:V)
			end
			test : - test.$(T)
			if "$(@:V)"
				eval
				test.$$(T) : $$(P) $(>:V:O>1)
					$(@:V)
				end
			else
				test.$(T) : $(P)
					$(*)
			end
		else
			test : - test.$(<)
			test.$(<) : $(<).tst $(<)
				$(REGRESS) $(REGRESSFLAGS) $(*)
		end
	end

.TESTINIT : .MAKE .VIRTUAL .FORCE .REPEAT
	if VARIANT == "DLL"
		error 1 :DLL: tests skipped
		exit 0
	end
	set keepgoing
