/*
 * regression test support
 *
 * @(#)TEST.mk (AT&T Labs Research) 2006-06-27
 *
 * test management is still in the design phase
 */

/*
 * three forms for :TEST:
 *
 *	:TEST: [--] xxx yyy ...
 *
 *		$(REGRESS) $(REGRESSFLAGS) xxx.tst xxx
 *		$(REGRESS) $(REGRESSFLAGS) yyy.tst yyy
 *
 *		-- disables .c .sh search
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
	local B G P S T
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
			:SAVE: $$(B).tst
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
					set +x; (ulimit -c 0) >/dev/null 2>&1 && ulimit -c 0; set -x
					$(@:V)
			end
		else
			eval
				test.$$(T) : $$(B)
					set +x; (ulimit -c 0) >/dev/null 2>&1 && ulimit -c 0; set -x
					$$(*) $(>:V:O>1)
			end
		end
	elif ! "$(<:V)"
		G = 1
		for B $(>)
			if B == "-|--"
				let G = !G
			else
				if ! G
					T =
				elif ! ( T = "$(B:A=.TARGET)" )
					for S .c .sh
						if "$(B:B:S=$(S):T=F)"
							:INSTALLDIR: $(B)
							$(B) :: $(B:B:S=$(S))
							T := $(B)
							break
						end
					end
				end
				test : - test.$(B)
				test.$(B) : $(B).tst $(T)
					$(REGRESS) $(REGRESSFLAGS) $(*)
				:SAVE: $(B).tst
			end
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
					set +x; (ulimit -c 0) >/dev/null 2>&1 && ulimit -c 0; set -x
					$(@:V)
				end
			else
				test.$(T) : $(P)
					set +x; (ulimit -c 0) >/dev/null 2>&1 && ulimit -c 0; set -x
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
	REGRESSFLAGS &= $(TESTS:@/ /|/G:/.*/--test=&/:@Q)

.SCAN.tst : .SCAN
	$(@.SCAN.sh)
	I| INCLUDE@ % |

.ATTRIBUTE.%.tst : .SCAN.tst

MKTEST = mktest
MKTESTFLAGS = --style=regress

/*
 * test scripts are only regenerated from *.rt when --force
 * is specified or the .rt file is newer than the script
 * otherwise the script is accepted if it exists
 *
 * this avoids the case where a fresh build with no state
 * would regenerate the test script and encode current
 * behavior instead of expected behavior
 */

%.tst : %.rt
	if	[[ "$(-force)" || "$(>)" -nt "$(^|<)" ]]
	then	$(MKTEST) $(MKTESTFLAGS) $(>) > $(<)
	fi

test%.sh test%.out : %.rt
	if	[[ "$(-force)" || "$(>)" -nt "$(^|<:O=1)" ]]
	then	$(MKTEST) --style=shell $(>) > $(<:N=*.sh)
		$(SHELL) $(<:N=*.sh) --accept > $(<:N=*.out)
	fi
