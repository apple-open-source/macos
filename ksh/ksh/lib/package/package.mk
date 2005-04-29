/*
 * source and binary package support
 *
 * @(#)package.mk (AT&T Labs Research) 2004-02-29
 *
 * usage:
 *
 *	cd $INSTALLROOT/lib/package
 *	nmake -f name [closure] [cyg|exp|lcl|pkg|rpm|tgz] [base|delta] type
 *
 * where:
 *
 *	name	package description file or component
 *
 *	type	source	build source archive, generates
 *			$(PACKAGEDIR)/name.version.release.suffix
 *		binary	build binary archive, generates
 *			$(PACKAGEDIR)/name.version.hosttype.release.suffix
 *
 * NOTE: $(PACKAGEDIR) is in the lowest view and is shared among all views
 *
 * generated archive member files are $(PACKAGEROOT) relative
 *
 * main assertions:
 *
 *	NAME [ name=value ] :PACKAGE: component ...
 *	:CATEGORY: category-id ...
 *	:COVERS: package ...
 *	:REQURES: package ...
 *	:INDEX: index description line
 *	:DESCRIPTION:
 *		[ verbose description ]
 *	:DETAILS: style
 *		:README:
 *			readme lines
 *		:EXPORT:
 *			name=value
 *		target :INSTALL: [ source ]
 *
 * option variables, shown with default values
 *
 *	format=tgz
 *		archive format
 *
 *	version=YYYY-MM-DD
 *		package base version (overrides current date)
 *
 *	release=YYYY-MM-DD
 *		package delta release (overrides current date)
 *
 *	strip=0
 *		don't strip non-lcl binary package members
 *
 *	variants=pattern
 *		include variants matching pattern in binary packages
 *
 *	incremental=[source:1 binary:0]
 *		if a base archive is generated then also generate an
 *		incremental delta archive from the previous base
 *
 * NOTE: the Makerules.mk :PACKAGE: operator defers to :package: when
 *	 a target is specified
 */

/* these are ast centric -- we'll parameterize another day */

org = ast
url = http://www.research.att.com/sw/download

/* generic defaults */

base =
category = utils
closure =
delta =
format = tgz
incremental =
index =
init = INIT
licenses = $(org)
opt =
name =
release =
strip = 0
style = tgz
suffix = tgz
type =
variants = !(cc-g)
vendor =
version = $("":T=R%Y-%m-%d)

package.notice = ------------ NOTICE -- LICENSED SOFTWARE -- SEE README FOR DETAILS ------------

package.readme = $(@.package.readme.)

.package.readme. :
	This is a package root directory $PACKAGEROOT. Source and binary
	packages in this directory tree are controlled by the command
	$()
		bin/package
	$()
	Binary package files are in the install root directory
	$()
		INSTALLROOT=$PACKAGEROOT/arch/`bin/package`
	$()
	For more information run
	$()
		bin/package help
	$()
	Each package has its own license file
	$()
		lib/package/LICENSES/<prefix>
	$()
	where <prefix> is the longest matching prefix of the package name.
	At the top of each license file is a URL; the license covers all
	software referring to this URL. For details run
	$()
		bin/package license [<package>]
	$()
	A component within a package may have its own license file
	$()
		lib/package/LICENSES/<prefix>-<component>
	$()
	or it may have a separate license detailed in the component
	source directory.
	$()
	Any archives, distributions or packages made from source or
	binaries covered by license(s) must contain the corresponding
	license file(s), this README file, and the empty file
	$()
	$(package.notice)

.package.licenses. : .FUNCTION
	local I L R
	L := $(%)
	if "$(%)" == "*-*"
		L += $(%:/[^-]*-//) $(%:/-.*//)
	end
	L += $(licenses)
	for I $(L:U)
		if R = "$(I:D=$(PACKAGESRC):B:S=.lic:T=F)"
			R += $(I:D=$(PACKAGESRC)/LICENSES:B)
			break
		end
	end
	return $(R)

PACKAGEROOT = $(VROOT:T=F:P=L*:N!=*/arch/+([!/]):O=1)
PACKAGESRC = $(PACKAGEROOT)/lib/package
PACKAGEBIN = $(INSTALLROOT)/lib/package
PACKAGEDIR = $(PACKAGESRC)/$(style)
INSTALLOFFSET = $(INSTALLROOT:C%$(PACKAGEROOT)/%%)

package.omit = -|*/$(init)
package.glob.all = $(INSTALLROOT)/src/*/*/($(MAKEFILES:/:/|/G))
package.all = $(package.glob.all:P=G:W=O=$(?$(name):A=.VIRTUAL):N!=$(package.omit):T=F:$(VROOT:T=F:P=L*:C,.*,C;^&/;;,:/ /:/G):U)
package.glob.pkg = $(INSTALLROOT)/src/*/($(~$(name):/ /|/G))/($(MAKEFILES:/:/|/G))
package.pkg = $(package.glob.pkg:P=G:D:N!=$(package.omit):T=F:$(VROOT:T=F:P=L*:C,.*,C;^&/;;,:/ /:/G):U)
package.closure = $(closure:?$(package.all)?$(package.pkg)?)

package.ini = ignore mamprobe manmake package silent
package.src.pat = $(PACKAGESRC)/($(name).(ini|lic|pkg))
package.src = $(package.src.pat:P=G) $(.package.licenses. $(name))
package.bin = $(PACKAGEBIN)/$(name).ini

op = current
stamp = [0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]
source = $(PACKAGEDIR)/$(name).$(version)$(release:?.$(release)??).$(suffix)
binary = $(PACKAGEDIR)/$(name).$(version)$(release:?.$(release)??).$(CC.HOSTTYPE).$(suffix)
old.new.source = $(PACKAGEDIR)/$(name).$(version).$(old.version).$(suffix)
old.new.binary = $(PACKAGEDIR)/$(name).$(version).$(old.version).$(CC.HOSTTYPE).$(suffix)

source.list = $("$(PACKAGEDIR)/$(name).*$(stamp).$(suffix)":P=G:H>)
binary.list = $("$(PACKAGEDIR)/$(name).*$(stamp).$(CC.HOSTTYPE).$(suffix)":P=G:H>)

source.ratz = $("$(INSTALLROOT)/src/cmd/$(init)/ratz.c":T=F)
binary.ratz = $("$(INSTALLROOT)/src/cmd/$(init)/ratz":T=F)

$(init) : .VIRTUAL $(init)

":package:" : .MAKE .OPERATOR
	local P I J R V
	P := $(<:O=1)
	$(P) : $(>:V)
	if ! name
		name := $(P)
		.PACKAGE. := $(P)
		if name == "$(init)"
			package.omit = -
			package.src += $(package.ini:C,^,$(PACKAGEROOT)/bin/,) $(PACKAGESRC)/package.mk
		end
		for I $(<:O>1)
			if I == "*=*"
				eval
				$(I)
				end
			else
				version := $(I)
			end
		end
		if name == "*-*"
			J := $(name:/[^-]*-//) $(name) $(name:/-[^-]*$//)
		else
			J := $(name)
		end
		for I $(J)
			while 1
				LICENSEFILE := $(LICENSEFILE):$(I:D=${PACKAGEROOT}/lib/package:B:S=.lic)
				if I != "*-*"
					break
				end
				I := $(I:/-[^-]*$//)
			end
		end
		export LICENSEFILE
	end
	if "$(>)"
		for I $(>:V)
			$(I) : .VIRTUAL
			if I == "/*"
				package.dir += $(I:V)
			end
		end
	end
	if "$(@)"
		$(P).txt := $(@)
	else
		$(P).txt := This is the $(P) package.
	end

":CATEGORY:" : .MAKE .OPERATOR
	category := $(>)

.covers. : .FUNCTION
	local I C D F K=0 L
	for I $(%)
		if ! "$(~covers:N=$(I:B))"
			if F = "$(I:D:B:S=.pkg:T=F)"
				if D = "$(F:T=I)"
					covers : $(I:B)
					for L $(D)
						if L == ":COVERS:"
							K = 1
						elif L == ":*:"
							if K
								break
							end
						elif K
							: $(.covers. $(L))
						end
					end
				end
			else
				error $(--exec:?3?1?) $(I): unknown package $(I)
			end
		end
	end

":COVERS:" : .MAKE .OPERATOR
	: $(.covers. $(>))

":DESCRIPTION:" : .MAKE .OPERATOR
	$(name).txt := $(@:V)

":INDEX:" : .MAKE .OPERATOR
	index := $(>)

":DETAILS:" : .MAKE .OPERATOR
	details.$(>:O=1) := $(@:V)

":README:" : .MAKE .OPERATOR
	readme.$(style) := $(@:V)

":EXPORT:" : .MAKE .OPERATOR
	export.$(style) := $(@:/$$("\n")/ /G)

":INSTALL:" : .MAKE .OPERATOR
	local T S F X
	S := $(>)
	T := $(<)
	if "$(exe.$(style))" && "$(T)" == "bin/*([!./])"
		T := $(T).exe
	end
	if ! "$(S)"
		S := $(T)
	elif "$(exe.$(style))" && "$(S)" == "bin/*([!./])"
		S := $(S).exe
	end
	install.$(style) := $(install.$(style):V)$("\n")install : $$(ROOT)/$(T)$("\n")$$(ROOT)/$(T) : $$(ARCH)/$(S)$("\n\t")cp $< $@
	if strip && "$(T:N=*.exe)"
		install.$(style) := $(install.$(style):V)$("\n\t")strip $@ 2>/dev/null
	end
	X := $(PACKAGEROOT)/arch/$(CC.HOSTTYPE)/$(S)
	if strip && "$(X:T=Y)" == "*/?(x-)(dll|exe)"
		F := filter $(STRIP) $(STRIPFLAGS) $(X)
	end
	if "$(filter.$(style):V)"
		filter.$(style) := $(filter.$(style):V)$$("\n")
	end
	filter.$(style) := $(filter.$(style):V);;$(F);$(X);usr/$(T)

.requires. : .FUNCTION
	local I C D F K=0 L V
	for I $(%)
		if ! "$(~requires:N=$(I:B))"
			if F = "$(I:D:B:S=.pkg:T=F)"
				if D = "$(F:T=I)"
					if I == "$(init)"
						package.omit = -
					else
						requires : $(I:B)
					end
					if V = "$(I:D:B=gen/$(I:B):S=.ver:T=F)"
						req : $(V)
					else
						error $(--exec:?3?1?) package $(I) must be written before $(P)
					end
					for L $(D)
						if L == ":REQUIRES:"
							K = 1
						elif L == ":*:"
							if K
								break
							end
						elif K
							: $(.requires. $(L))
						end
					end
				end
			else
				error $(--exec:?3?1?) $(I): unknown package $(I)
			end
		end
	end

":REQUIRES:" : .MAKE .OPERATOR
	: $(.requires. $(>))

":TEST:" : .MAKE .OPERATOR
	local T
	T := $(>)
	if "$(T)" == "bin/*([!./])"
		if "$(exe.$(style))"
			T := $(T).exe
		end
		T := $$(PWD)/$$(ARCH)/$(T)
	end
	test.$(style) := $(test.$(style):V)$("\n")test : $(T:V)$("\n\t")$(@)

":POSTINSTALL:" : .MAKE .OPERATOR
	postinstall.$(style) := $(@:V)

base delta : .MAKE .VIRTUAL .FORCE
	op := $(<)

closure : .MAKE .VIRTUAL .FORCE
	$(<) := 1

cyg exp lcl pkg rpm tgz : .MAKE .VIRTUAL .FORCE
	style := $(<)

source : .source.init .source.gen .source.$$(style)

.source.init : .MAKE
	local A B D P V I
	type := source
	if ! "$(incremental)"
		incremental = 1
	end
	if "$(source.$(name))"
		suffix = c
	end
	: $(.init.$(style))
	: $(details.$(style):V:R) :
	A := $(source.list)
	B := $(A:N=*.$(stamp).$(suffix):N!=*.$(stamp).$(stamp).*:O=1:T=F)
	P := $(A:N=*.$(stamp).$(suffix):N!=*.$(stamp).$(stamp).*:O=2:T=F)
	D := $(A:N=*.$(stamp).$(stamp).$(suffix):O=1:T=F)
	if op == "delta"
		if ! B
			error 3 delta requires a base archive
		end
		base := -z $(B)
		deltaversion := $(B:B:B:/$(name).//)
		let deltasince = $(deltaversion:/.*-//) + 1
		deltasince := $(deltaversion:/[^-]*$/$(deltasince:F=%02d)/)
		if "$(release)" != "$(stamp)"
			release := $("":T=R%Y-%m-%d)
		end
		source := $(B:D:B:S=.$(release).$(suffix))
		version := $(source:B:B:/$(name).//)
	elif B || op == "base"
		if op == "base"
			for I $(B) $(P)
				V := $(I:B:/$(name)\.\([^.]*\).*/\1/)
				if V == "$(stamp)" && V != "$(version)"
					old.version := $(V)
					old.source := $(I)
					if "$(old.version)" >= "$(version)"
						error 3 $(name): previous base $(old.version) is newer than $(version)
					end
					break
				end
			end
		else
			source := $(B)
		end
		if B == "$(source)"
			if "$(B:D:B:B)" == "$(D:D:B:B)" && "$(B:B::S)" != "$(D:B::S)"
				error 3 $(B:B:S): base overwrite would invalidate delta $(D:B:S)
			end
			error 1 $(B:B:S): replacing current base
		end
		version := $(source:B:S:/^$(name).\(.*\).$(suffix)$/\1/)
	end
	PACKAGEGEN := $(PACKAGESRC)/gen

.source.gen : $$(PACKAGEDIR) $$(PACKAGEGEN) $$(PACKAGEGEN)/SOURCE.html $$(PACKAGEGEN)/BINARY.html $$(PACKAGEGEN)/DETAILS.html

BINPACKAGE := $(PATH:/:/ /G:X=package:T=F:O=1)

$$(PACKAGEDIR) $$(PACKAGEGEN) : .IGNORE
	test -d $(<) || mkdir $(<)

$$(PACKAGEGEN)/SOURCE.html : $(BINPACKAGE)
	$(*) html source > $(<)

$$(PACKAGEGEN)/BINARY.html : $(BINPACKAGE)
	$(*) html binary > $(<)

$$(PACKAGEGEN)/DETAILS.html : $(BINPACKAGE)
	$(*) html intro > $(<)

.source.exp .source.pkg .source.rpm : .MAKE
	error 3 $(style): source package style not supported yet

exe.cyg = .exe
vendor.cyg = gnu

.name.cyg : .FUNCTION
	local N
	N := $(%)
	if N == "*-*"
		vendor := $(N:/-.*//)
		if vendor == "$(vendor.cyg)"
			vendor :=
			N := $(N:/[^-]*-//)
		end
		N := $(N:/-//G)
	end
	return $(N)

.init.cyg : .FUNCTION
	local N O
	closure = 1
	init = .
	strip = 1
	suffix = tar.bz2
	format = tbz
	vendor := $(licenses:N!=$(vendor.cyg):O=1)
	package.ini := $(package.ini)
	package.src.pat := $(package.src.pat)
	package.src := $(package.src)
	package.bin := $(package.bin)
	.source.gen : .CLEAR $(*.source.gen:V:N!=*.html)
	name.original := $(name)
	name := $(.name.cyg $(name))
	if name != "$(name.original)"
		$(name) : $(~$(name.original))
		O := $(~covers)
		covers : .CLEAR
		for N $(O)
			covers : $(.name.cyg $(N))
		end
	end
	stamp = [0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]-[0-9]
	version.original := $(version)
	version := $(version:/-//G)-1
	if opt
		opt := $(opt)/$(vendor)/
	else
		opt := $(name)-$(version)/
	end
	if type == "source"
		version := $(version)-src
		source = $(PACKAGEDIR)/$(name)-$(version)$(release:?.$(release)??).$(suffix)
	else
		binary = $(PACKAGEDIR)/$(name)-$(version)$(release:?.$(release)??).$(suffix)
	end

.source.cyg :
	if	test '' != '$(~$(name))'
	then	tmp=/tmp/pkg$(tmp)
		mkdir $tmp
		{
			integer m
			: > $tmp/HEAD
			echo ";;;$tmp/HEAD;$(package.notice)"
			cat > $tmp/README <<'!'
	$(package.readme)
	!
			echo ";;;$tmp/README;README"
			cat > $tmp/configure <<'!'
	echo "you didn't have to do that"
	!
			chmod +x $tmp/configure
			echo ";;;$tmp/configure;configure"
			cat > $tmp/Makefile0 <<'!'
	HOSTTYPE := $$(shell bin/package)
	ROOT = ../..
	ARCH = arch/$$(HOSTTYPE)
	all :
		PACKAGEROOT= CYGWIN="$$CYGWIN ntsec binmode" bin/package make $(export.$(style))
	install : all
	$(install.$(style):V)
	$(test.$(style):V)
	!
			echo ";;;$tmp/Makefile0;Makefile"
			cat > $tmp/CYGWIN-README <<'!'
	$(readme.$(style):@?$$(readme.$$(style))$$("\n\n")??)To build binaries from source into the ./arch/`bin/package` tree run:
	$()
		make
	$()
	$(test.$(style):@?To test the binaries after building/installing run:$$("\n\n\t")make test$$("\n\n")??)To build and/or install the binaries run:
	$()
		make install
	$()
	The bin/package command provides a command line interface for all package
	operations. The $(opt:/.$//) source and binary packages were generated by:
	$()
		package write cyg base source version=$(version.original) $(name.original)
		package write cyg base binary version=$(version.original) $(name.original)
	$()
	using the $(org)-base package. To download and install the latest
	$(org)-base source package in /opt/$(org) run:
	$()
		PATH=/opt/$(org)/bin:$PATH
		cd /opt/$(org)
		package setup flat source $("\\")
			$(url) $("\\")
			$(org)-base
		package make
	$()
	and export /opt/$(org)/bin in PATH to use. If multiple architectures
	may be built under /opt/$(org) then drop "flat" and export
	/opt/$(org)/arch/`package`/bin in PATH to use. To update
	previously downloaded packages from the same url simply run:
	$()
		cd /opt/$(org)
		package setup
		package make
	$()
	To download and install the latest $(org)-base binary package in
	/opt/$(org) change "source" to "binary" and omit "package make".
	!
			echo ";;;$tmp/CYGWIN-README;CYGWIN-PATCHES/README"
			cat > $(source:/-src.$(suffix)//).setup.hint <<'!'
	category: $(category:/\(.\).*/\1/U)$(category:/.\(.*\)/\1/L)
	requires: cygwin
	sdesc: "$(index)"
	ldesc: "$($(name.original).txt)"
	!
			echo ";;;$(source:/-src.$(suffix)//).setup.hint;CYGWIN-PATCHES/setup.hint"
			echo ";;;$(BINPACKAGE);bin/package"
			cat > $tmp/Makefile <<'!'
	:MAKE:
	!
			echo ";;;$tmp/Makefile;src/Makefile"
			echo ";;;$tmp/Makefile;src/cmd/Makefile"
			echo ";;;$tmp/Makefile;src/lib/Makefile"
			cat > $tmp/Mamfile1 <<'!'
	info mam static
	note source level :MAKE: equivalent
	make install
	make all
	exec - ${MAMAKE} -r '*/*' ${MAMAKEARGS}
	done all virtual
	done install virtual
	!
			echo ";;;$tmp/Mamfile1;src/Mamfile"
			cat > $tmp/Mamfile2 <<'!'
	info mam static
	note component level :MAKE: equivalent
	make install
	make all
	exec - ${MAMAKE} -r '*' ${MAMAKEARGS}
	done all virtual
	done install virtual
	!
			echo ";;;$tmp/Mamfile2;src/cmd/Mamfile"
			echo ";;;$tmp/Mamfile2;src/lib/Mamfile"
			$(package.src:T=F:/.*/echo ";;;&"$("\n")/)
			echo ";;;$(PACKAGEGEN)/$(name.original).req"
			set -- $(package.closure)
			for i
			do	cd $(INSTALLROOT)/$i
				(( m++ ))
				s=$( $(MAKE) --noexec recurse=list 2>/dev/null )
				if	test "" != "$s"
				then	(( m++ ))
					cat > $tmp/$m.mam <<'!'
	info mam static
	note subcomponent level :MAKE: equivalent
	make install
	make all
	exec - ${MAMAKE} -r '*' ${MAMAKEARGS}
	done all virtual
	done install virtual
	!
					echo ";;;$tmp/$m.mam;$i/Mamfile"
					for j in $s
					do	if	test -d $j
						then	cd $j
							(( m++ ))
							$(MAKE) --never --force --mam=static --corrupt=accept CC=$(CC.DIALECT:N=C++:?CC?cc?) $(=) 'dontcare test' install test > $tmp/$m.mam
							echo ";;;$tmp/$m.mam;$i/$j/Mamfile"
							cd $(INSTALLROOT)/$i
						fi
					done
				else	(( m++ ))
					$(MAKE) --never --force --mam=static --corrupt=accept CC=$(CC.DIALECT:N=C++:?CC?cc?) $(=) 'dontcare test' install test > $tmp/$m.mam
					echo ";;;$tmp/$m.mam;$i/Mamfile"
				fi
				$(MAKE) --noexec $(-) $(=) recurse list.package.$(type)
			done
			set -- $(package.dir:P=G)
			for i
			do	tw -d $i -e "action:printf(';;;%s\n',path);"
			done
			: > $tmp/TAIL
			echo ";;;$tmp/TAIL;$(package.notice)"
		} |
		$(PAX)	--filter=- \
			--to=ascii \
			--format=$(format) \
			--local \
			-wvf $(source) $(base) \
			$(VROOT:T=F:P=L*:C%.*%-s",^&/,,"%) \
			$(vendor:?-s",^[^/],$(opt),"??)
		rm -rf $tmp
	fi

.source.lcl :
	if	test '' != '$(~$(name))'
	then	tmp=/tmp/pkg$(tmp)
		mkdir $tmp
		{
			integer m
			$(package.src:T=F:/.*/echo ";;;&"$("\n")/)
			set -- $(package.closure)
			for i
			do	cd $(INSTALLROOT)/$i
				$(MAKE) --noexec $(-) $(=) .FILES.+=Mamfile recurse list.package.local
			done
			set -- $(package.dir:P=G)
			for i
			do	tw -d $i -e "action:printf(';;;%s\n',path);"
			done
		} |
		$(PAX)	--filter=- \
			--to=ascii \
			$(op:N=delta:??--format=$(format)?) \
			--local \
			-wvf $(source) $(base) \
			$(VROOT:T=F:P=L*:C%.*%-s",^&/,,"%)
		rm -rf $tmp
	fi

.source.tgz :
	if	test '' != '$(~$(name))'
	then	tmp=/tmp/pkg$(tmp)
		mkdir $tmp
		{
			integer m=0
			: > $tmp/HEAD
			echo ";;;$tmp/HEAD;$(package.notice)"
			cat > $tmp/README <<'!'
	$(package.readme)
	!
			echo ";;;$tmp/README;README"
			if	test '$(init)' = '$(name)'
			then	
				cat > $tmp/Makefile <<'!'
	:MAKE:
	!
				echo ";;;$tmp/Makefile;src/Makefile"
				echo ";;;$tmp/Makefile;src/cmd/Makefile"
				echo ";;;$tmp/Makefile;src/lib/Makefile"
				cat > $tmp/Mamfile1 <<'!'
	info mam static
	note source level :MAKE: equivalent
	make install
	make all
	exec - ${MAMAKE} -r '*/*' ${MAMAKEARGS}
	done all virtual
	done install virtual
	!
				echo ";;;$tmp/Mamfile1;src/Mamfile"
				cat > $tmp/Mamfile2 <<'!'
	info mam static
	note component level :MAKE: equivalent
	make install
	make all
	exec - ${MAMAKE} -r '*' ${MAMAKEARGS}
	done all virtual
	done install virtual
	!
				echo ";;;$tmp/Mamfile2;src/cmd/Mamfile"
				echo ";;;$tmp/Mamfile2;src/lib/Mamfile"
			fi
			$(package.src:T=F:/.*/echo ";;;&"$("\n")/)
			echo $(name) $(version) $(release|version) 1 > $(PACKAGEGEN)/$(name).ver
			echo ";;;$(PACKAGEGEN)/$(name).ver"
			if	test '' != '$(~covers)'
			then	for i in $(~covers)
				do	for j in pkg lic
					do	if	test -f $(PACKAGESRC)/$i.$j
						then	echo ";;;$(PACKAGESRC)/$i.$j"
						fi
					done
					for j in ver req
					do	if	test -f $(PACKAGEGEN)/$i.$j
						then	echo ";;;$(PACKAGEGEN)/$i.$j"
						fi
					done
				done
			fi
			sed 's,1$,0,' $(~req) < /dev/null > $(PACKAGEGEN)/$(name).req
			echo ";;;$(PACKAGEGEN)/$(name).req"
			{
				echo "name='$(name)'"
				echo "index='$(index)'"
				echo "covers='$(~covers)'"
				echo "requires='$(~req)'"
			} > $(PACKAGEGEN)/$(name).inx
			{
				{
				echo '$($(name).txt)'
				if	test '' != '$(~covers)'
				then	echo "This package is a superset of the following package$(~covers:O=2:?s??): $(~covers); you won't need $(~covers:O=2:?these?this?) if you download $(name)."
				fi
				if	test '' != '$(~requires)'
				then	echo 'It requires the following package$(~requires:O=2:?s??): $(~requires).'
				fi
				} | fmt
				package help source
				package release $(name)
			} > $(PACKAGEGEN)/$(name).txt
			echo ";;;$(PACKAGEGEN)/$(name).txt"
			{
				echo '.xx title="$(name) package"'
				echo '.xx meta.description="$(name) package"'
				echo '.xx meta.keywords="software, package"'
				echo '.MT 4'
				echo '.TL'
				echo '$(name) package'
				echo '.H 1 "$(name) package"'
				echo '$($(name).txt)'
				set -- $(package.closure:C,.*,$(INSTALLROOT)/&/PROMO.mm,:T=F:D::B)
				hot=
				for i
				do	hot="$hot -e s/\\(\\<$i\\>\\)/\\\\h'0*1'\\1\\\\h'0'/"
				done
				set -- $(package.closure:B)
				if	test $# != 0
				then	echo 'Components in this package:'
					echo '.P'
					echo '.TS'
					echo 'center expand;'
					echo 'l l l l l l.'
					if	test '' != "$hot"
					then	hot="sed $hot"
					else	hot=cat
					fi
					for i
					do	echo $i
					done |
					pr -6 -t -s'	' |
					$hot
					echo '.TE'
				fi
				echo '.P'
				if	test '' != '$(~covers)'
				then	echo "This package is a superset of the following package$(~covers:O=2:?s??): $(~covers); you won't need $(~covers:O=2:?these?this?) if you download $(name)."
				fi
				if	test '' != '$(~requires)'
				then	echo 'It requires the following package$(~requires:O=2:?s??): $(~requires).'
				fi
				case $(name) in
				$(init))set -- $(licenses:B:S=.lic:U:T=F) ;;
				*)	set -- $(package.src:N=*.lic:U:T=F) ;;
				esac
				case $# in
				0)	;;
				*)	case $# in
					1)	echo 'The software is covered by this license:' ;;
					*)	echo 'The software is covered by these licenses:' ;;
					esac
					echo .BL
					for j
					do	i=$( $(PROTO) -l $j -p -h -o type=usage /dev/null | sed -e 's,.*\[-license?\([^]]*\).*,\1,' )
						echo .LI
						echo ".xx link=\"$i\""
					done
					echo .LE
					echo 'Individual components may be covered by separate licenses;'
					echo 'refer to the component source and/or binaries for more information.'
					echo .P
					;;
				esac
				echo 'A recent'
				echo '.xx link="release change log"'
				echo 'is also included.'
				cat $(package.closure:C,.*,$(INSTALLROOT)/&/PROMO.mm,:T=F) < /dev/null
				echo '.H 1 "release change log"'
				echo '.xx index'
				echo '.nf'
				package release $(name) |
				sed -e 's/:::::::: \(.*\) ::::::::/.fi\$("\n").H 1 "\1 changes"\$("\n").nf/'
				echo '.fi'
			} |
			$(MM2HTML) $(MM2HTMLFLAGS) -o nohtml.ident > $(PACKAGEGEN)/$(name).html
			$(STDED) $(STDEDFLAGS) $(PACKAGEGEN)/$(name).html <<'!'
	/^<!--LABELS-->$/,/^<!--\/LABELS-->$/s/ changes</</
	/^<!--LABELS-->$/,/^<!--\/LABELS-->$/m/<A name="release change log">/
	w
	q
	!
			echo ";;;$(PACKAGEGEN)/$(name).html"
			if	test '' != '$(deltasince)'
			then	{
				echo '.xx title="$(name) package"'
				echo '.xx meta.description="$(name) package $(version) delta $(release)"'
				echo '.xx meta.keywords="software, package, delta"'
				echo '.MT 4'
				echo '.TL'
				echo '$(name) package $(deltaversion) delta $(release)'
				echo '.H 1 "$(name) package $(deltaversion) delta $(release) changes"'
				echo '.nf'
				package release $(deltasince) $(name) |
				sed -e 's/:::::::: \(.*\) ::::::::/.H 2 \1/'
				echo '.fi'
				} |
				$(MM2HTML) $(MM2HTMLFLAGS) -o nohtml.ident > $(PACKAGEGEN)/$(name).$(release).html
				echo ";;;$(PACKAGEGEN)/$(name).$(release).html"
			fi
			set -- $(package.closure)
			for i
			do	cd $(INSTALLROOT)/$i
				s=$( $(MAKE) --noexec recurse=list 2>/dev/null )
				if	test "" != "$s"
				then	(( m++ ))
					cat > $tmp/$m.mam <<'!'
	info mam static
	note subcomponent level :MAKE: equivalent
	make install
	make all
	exec - ${MAMAKE} -r '*' ${MAMAKEARGS}
	done all virtual
	done install virtual
	!
					echo ";;;$tmp/$m.mam;$i/Mamfile"
					for j in $s
					do	if	test -d $j
						then	cd $j
							(( m++ ))
							$(MAKE) --never --force --mam=static --corrupt=accept CC=$(CC.DIALECT:N=C++:?CC?cc?) $(=) 'dontcare test' install test > $tmp/$m.mam
							echo ";;;$tmp/$m.mam;$i/$j/Mamfile"
							cd $(INSTALLROOT)/$i
						fi
					done
				else	(( m++ ))
					$(MAKE) --never --force --mam=static --corrupt=accept CC=$(CC.DIALECT:N=C++:?CC?cc?) $(=) 'dontcare test' install test > $tmp/$m.mam
					echo ";;;$tmp/$m.mam;$i/Mamfile"
				fi
				$(MAKE) --noexec $(-) $(=) recurse list.package.$(type)
			done
			set -- $(package.dir:P=G)
			for i
			do	tw -d $i -e "action:printf(';;;%s\n',path);"
			done
			: > $tmp/TAIL
			echo ";;;$tmp/TAIL;$(package.notice)"
		} |
		$(PAX)	--filter=- \
			--to=ascii \
			$(op:N=delta:??--format=$(format)?) \
			--local \
			-wvf $(source) $(base) \
			$(VROOT:T=F:P=L*:C%.*%-s",^&/,,"%)
		echo local > $(source:D:B=$(name):S=.tim)
		test '1' = '$(incremental)' -a '' != '$(old.source)' &&
		$(PAX) -rf $(source) -wvf $(old.new.source) -z $(old.source)
		rm -rf $tmp
	else	if	test '' != '$(old.source)' &&
			cmp -s $(source.$(name)) $(source)
		then	: $(name) is up to date
		else	echo $(name) $(version) $(release|version) 1 > $(PACKAGEGEN)/$(name).ver
			: > $(PACKAGEGEN)/$(name).req
			{
				echo "name='$(name)'"
				echo "index='$(index)'"
				echo "covers='$(~covers)'"
				echo "requires='$(~req)'"
			} > $(PACKAGEGEN)/$(name).inx
			{
				echo '.xx title="$(name) package"'
				echo '.xx meta.description="$(name) package"'
				echo '.xx meta.keywords="software, package"'
				echo '.MT 4'
				echo '.TL'
				echo '$(name) package'
				echo '.H 1'
				echo '$($(name).txt)'
			} |
			$(MM2HTML) $(MM2HTMLFLAGS) -o nohtml.ident > $(PACKAGEGEN)/$(name).html
			if	test '' != '$(source.$(name))'
			then	{
					echo '$($(name).txt)'
					package help source
				} > $(PACKAGEGEN)/$(name).txt
				cp $(source.$(name)) $(source)
			fi
			echo local > $(source:D:B=$(name):S=.tim)
		fi
	fi

binary : .binary.init .binary.gen .binary.$$(style)

.binary.init : .MAKE
	local A B D I P V
	type := binary
	if ! "$(incremental)"
		incremental = 0
	end
	if ! "$(~$(name))"
		if name == "ratz"
			suffix = exe
		else
			suffix = gz
		end
	end
	: $(.init.$(style)) :
	: $(details.$(style):V:R) :
	A := $(binary.list)
	B := $(A:N=*.$(stamp).$(CC.HOSTTYPE).$(suffix):N!=*.$(stamp).$(stamp).*:O=1:T=F)
	P := $(A:N=*.$(stamp).$(CC.HOSTTYPE).$(suffix):N!=*.$(stamp).$(stamp).*:O=2:T=F)
	D := $(A:N=*.$(stamp).$(stamp).$(CC.HOSTTYPE).$(suffix):O=1:T=F)
	if op == "delta"
		if ! B
			error 3 delta requires a base archive
		end
		base := -z $(B)
		if "$(release)" != "$(stamp)"
			release := $("":T=R%Y-%m-%d)
		end
		binary := $(B:/$(CC.HOSTTYPE).$(suffix)$/$(release).&/)
		version := $(binary:B:B:/$(name).//)
	elif B || op == "base"
		if op == "base"
			for I $(B) $(P)
				V := $(I:B:/$(name)\.\([^.]*\).*/\1/)
				if V == "[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]" && V != "$(version)"
					old.version := $(V)
					old.binary := $(I)
					if "$(old.version)" >= "$(version)"
						error 3 $(name): previous base $(old.version) is newer than $(version)
					end
					break
				end
			end
		else
			binary := $(B)
		end
		if B == "$(binary)"
			if "$(B:D:B)" == "$(D:D:B)" && "$(B:S)" != "$(D:S)"
				error 3 $(B:B:S): base overwrite would invalidate delta $(D:B:S)
			end
			error 1 $(B:B:S): replacing current base
		end
		version := $(binary:B:/$(name).//:/\..*//)
	end
	PACKAGEGEN := $(PACKAGEBIN)/gen

.binary.gen : $$(PACKAGEDIR) $$(PACKAGEGEN)

.binary.exp .binary.pkg .binary.rpm : .MAKE
	error 3 $(style): binary package style not supported yet

.binary.cyg :
	if	test '' != '$(~$(name))'
	then	tmp=/tmp/pkg$(tmp)
		mkdir $tmp
		{
			integer m
			: > $tmp/HEAD
			echo ";;;$tmp/HEAD;usr/doc/$(opt)$(package.notice)"
			{
				echo '$($(name.original).txt)' | fmt
				cat <<'!'
	$(readme.$(style):@?$$("\n")$$(readme.$$(style))??)
	!
			} > $tmp/README1
			echo ";;;$tmp/README1;usr/doc/Cygwin/$(opt:/.$//).README"
			{
				echo '$($(name.original).txt)' | fmt
				cat <<'!'
	$()
	The remainder of this file is the README from the source package
	that was used to generate this binary package. It describes
	the source build hierarchy, not the current directory.
	$()
	$(package.readme)
	!
			} > $tmp/README2
			echo ";;;$tmp/README2;usr/doc/$(opt)README"
			package release $(name.original) > $tmp/RELEASE
			echo ";;;$tmp/RELEASE;usr/doc/$(opt)RELEASE"
			cat > $(binary:/.$(suffix)//).setup.hint <<'!'
	category: $(category:/\(.\).*/\1/U)$(category:/.\(.*\)/\1/L)
	requires:
	sdesc: "$(index)"
	ldesc: "$($(name.original).txt)"
	!
			set -- $(.package.licenses. $(name.original):N!=*.lic)
			for i
			do	echo ";;;${i};usr/doc/$(opt)LICENSE-${i##*/}"
			done
			cat <<'!'
	$(filter.$(style))
	!
			if	test '' != '$(postinstall.$(style):V:O=1:?1??)'
			then	cat >$tmp/postinstall <<'!'
	$("#")!/bin/sh
	$(postinstall.$(style))
	!
				echo ";;;$tmp/postinstall;etc/postinstall/$(name).sh"
			fi
			: > $tmp/TAIL
			echo ";;;$tmp/TAIL;usr/doc/$(opt)$(package.notice)"
		} |
		$(PAX)	--filter=- \
			--to=ascii \
			--format=$(format) \
			--local \
			-wvf $(binary)
		rm -rf $tmp
	fi

.binary.lcl :
	if	test '' != '$(~$(name))'
	then	tmp=/tmp/pkg$(tmp)
		mkdir $tmp
		{
			$(package.src:T=F:/.*/echo ";;;&"$("\n")/)
			$(package.bin:T=F:/.*/echo ";;;&"$("\n")/)
			set -- $(package.closure)
			for i
			do	cd $(INSTALLROOT)/$i
				$(MAKE) --noexec $(-) $(=) recurse list.package.$(type) variants=$(variants:Q) cc-
			done
		} |
		sort -u |
		$(PAX)	--filter=- \
			--to=ascii \
			$(op:N=delta:??--format=$(format)?) \
			--local \
			--checksum=md5:$(PACKAGEGEN)/$(name).sum \
			--install=$(PACKAGEGEN)/$(name).ins \
			-wvf $(binary) $(base) \
			-s",^$tmp/,$(INSTALLOFFSET)/," \
			$(PACKAGEROOT:C%.*%-s",^&/,,"%)
		echo local > $(binary:D:B=$(name):S=.$(CC.HOSTTYPE).tim)
		rm -rf $tmp
	fi

.binary.tgz :
	if	test '' != '$(~$(name))'
	then	tmp=/tmp/pkg$(tmp)
		mkdir $tmp
		{
			if	test '$(init)' = '$(name)'
			then	for i in lib32 lib64
				do	if	test -d $(INSTALLROOT)/$i
					then	echo ";physical;;$(INSTALLROOT)/$i"
					fi
				done
			fi
			$(package.src:T=F:/.*/echo ";;;&"$("\n")/)
			$(package.src:T=F:N=*/LICENSES/*:B:C,.*,echo ";;;$(PACKAGESRC)/LICENSES/&;$(PACKAGEBIN)/LICENSES/&"$("\n"),)
			$(package.bin:T=F:/.*/echo ";;;&"$("\n")/)
			echo $(name) $(version) $(release|version) 1 > $(PACKAGEGEN)/$(name).ver
			echo ";;;$(PACKAGEGEN)/$(name).ver"
			if	test '' != '$(~covers)'
			then	for i in $(~covers)
				do	for j in pkg lic
					do	if	test -f $(PACKAGESRC)/$i.$j
						then	echo ";;;$(PACKAGESRC)/$i.$j"
						fi
					done
					for j in ver req
					do	if	test -f $(PACKAGEGEN)/$i.$j
						then	echo ";;;$(PACKAGEGEN)/$i.$j"
						fi
					done
				done
			fi
			sed 's,1$,0,' $(~req) < /dev/null > $(PACKAGEGEN)/$(name).req
			echo ";;;$(PACKAGEGEN)/$(name).req"
			{
				echo "name='$(name)'"
				echo "index='$(index)'"
				echo "covers='$(~covers)'"
				echo "requires='$(~req)'"
			} > $(PACKAGEGEN)/$(name).inx
			{
				{
				echo '$($(name).txt)'
				if	test '' != '$(~covers)'
				then	echo "This package is a superset of the following package$(~covers:O=2:?s??): $(~covers); you won't need $(~covers:O=2:?these?this?) if you download $(name)."
				fi
				if	test '' != '$(~requires)'
				then	echo 'It requires the following package$(~requires:O=2:?s??): $(~requires).'
				fi
				} | fmt
				package help binary
				package release $(name)
			} > $(PACKAGEGEN)/$(name).txt
			echo ";;;$(PACKAGEGEN)/$(name).txt"
			set -- $(package.closure)
			for i
			do	cd $(INSTALLROOT)/$i
				$(MAKE) --noexec $(-) $(=) package.strip=$(strip) recurse list.package.$(type) variants=$(variants:Q) cc-
			done
		} |
		sort -u | {
			: > $tmp/HEAD
			echo ";;;$tmp/HEAD;$(package.notice)"
			cat > $tmp/README <<'!'
	$(package.readme)
	!
			echo ";;;$tmp/README;README"
			cat
			: > $tmp/TAIL
			echo ";;;$tmp/TAIL;$(package.notice)"
		} |
		$(PAX)	--filter=- \
			--to=ascii \
			$(op:N=delta:??--format=$(format)?) \
			--local \
			--checksum=md5:$(PACKAGEGEN)/$(name).sum \
			--install=$(PACKAGEGEN)/$(name).ins \
			-wvf $(binary) $(base) \
			-s",^$tmp/,$(INSTALLOFFSET)/," \
			$(PACKAGEROOT:C%.*%-s",^&/,,"%)
		echo local > $(binary:D:B=$(name):S=.$(CC.HOSTTYPE).tim)
		test '1' = '$(incremental)' -a '' != '$(old.binary)' &&
		$(PAX) -rf $(binary) -wvf $(old.new.binary) -z $(old.binary)
		rm -rf $tmp
	else	if	test '' != '$(binary.$(name))'
		then	exe=$(binary.$(name))
		else	exe=$(INSTALLROOT)/bin/$(name)
		fi
		if	test '' != '$(old.binary)' &&
			cmp -s $exe $(binary)
		then	: $(name) is up to date
		else	echo $(name) $(version) $(release|version) 1 > $(PACKAGEGEN)/$(name).ver
			: > $(PACKAGEGEN)/$(name).req
			{
				echo "name='$(name)'"
				echo "index='$(index)'"
				echo "covers='$(~covers)'"
				echo "requires='$(~req)'"
			} > $(PACKAGEGEN)/$(name).inx
			{
				echo '$($(name).txt)'
				package help binary
			} > $(PACKAGEGEN)/$(name).txt
			case "$(binary)" in
			*.gz)	gzip < $exe > $(binary) ;;
			*)	cp $exe $(binary) ;;
			esac
			echo local > $(binary:D:B=$(name):S=.$(CC.HOSTTYPE).tim)
		fi
	fi

list.installed list.manifest :
	set -- $(package.closure)
	for i
	do	cd $(INSTALLROOT)/$i
		ignore $(MAKE) --noexec $(-) $(=) $(<)
	done
