" Vim syntax file
" Language:	ANT build file (xml)
" Maintainer:	Johannes Zellner <johannes@zellner.org>
" Last Change:	Tue, 20 May 2003 21:44:17 CEST
" Filenames:	build.xml
" $Id$

" Quit when a syntax file was already loaded
if exists("b:current_syntax")
    finish
endif

let s:ant_cpo_save = &cpo
set cpo&vim

runtime! syntax/xml.vim

syn case ignore

if !exists('*AntSyntaxScript')
    fun AntSyntaxScript(tagname, synfilename)
	unlet b:current_syntax
	let s:include = expand("<sfile>:p:h").'/'.a:synfilename
	if filereadable(s:include)
	    exe 'syn include @ant'.a:tagname.' '.s:include
	else
	    exe 'syn include @ant'.a:tagname." $VIMRUNTIME/syntax/".a:synfilename
	endif

	exe 'syn region ant'.a:tagname
		    \." start=#<script[^>]\\{-}language\\s*=\\s*['\"]".a:tagname."['\"]\\(>\\|[^>]*[^/>]>\\)#"
		    \.' end=#</script>#'
		    \.' fold'
		    \.' contains=@ant'.a:tagname.',xmlCdataStart,xmlCdataEnd,xmlTag,xmlEndTag'
		    \.' keepend'
	exe 'syn cluster xmlRegionHook add=ant'.a:tagname
    endfun
endif

" TODO: add more script languages here ?
call AntSyntaxScript('javascript', 'javascript.vim')
call AntSyntaxScript('jpython', 'python.vim')


syn cluster xmlTagHook add=antElement

syn keyword antElement display WsdlToDotnet addfiles and ant antcall antlr antstructure apply archives arg
syn keyword antElement display argument attribute available basename batchtest bcc blgenclient bootclasspath
syn keyword antElement display borland bottom buildnumber bunzip2 bzip2 cab cc cccheckin cccheckout ccmcheckin
syn keyword antElement display ccmcheckintask ccmcheckout ccmcreatetask ccmreconfigure ccuncheckout ccupdate
syn keyword antElement display checksum chmod class classconstants classes classfileset classpath commandline
syn keyword antElement display comment compilerarg compilerclasspath concat condition copy copydir copyfile
syn keyword antElement display coveragepath csc custom cvs cvschangelog cvspass cvstagdiff date delete deltree
syn keyword antElement display depend depends dependset depth description dirname dirset dname doclet doctitle
syn keyword antElement display dtd ear echo echoproperties ejbjar entity entry env equals exclude
syn keyword antElement display excludepackage excludesfile exec execon expandproperties extdirs extension
syn keyword antElement display extensionSet extensionset fail filelist filename filepath fileset filesmatch
syn keyword antElement display filter filterchain filterreader filters filterset filtersfile fixcrlf footer
syn keyword antElement display format formatter from generic genkey get group gunzip gzip header headfilter
syn keyword antElement display http ilasm include includesfile input iplanet iplanet-ejbc isfalse isset istrue
syn keyword antElement display jar jarlib-available jarlib-manifest jarlib-resolve java javac javacc javadoc
syn keyword antElement display javadoc2 javah jboss jjtree jlink jonas jpcoverage jpcovmerge jpcovreport jspc
syn keyword antElement display junit junitreport jvmarg lib libfileset link loadfile loadproperties location
syn keyword antElement display mail majority manifest map mapper marker maudit mergefiles message metainf
syn keyword antElement display method mimemail mkdir mmetrics move mparse native2ascii none not options or os
syn keyword antElement display outputproperty p4add p4change p4counter p4delete p4edit p4have p4label p4reopen
syn keyword antElement display p4revert p4submit p4sync package packageset parallel param patch path
syn keyword antElement display pathconvert pathelement patternset prefixlines present project property
syn keyword antElement display propertyfile pvcs pvcsproject record reference regexp rename renameext replace
syn keyword antElement display replacefilter replaceregexp replacetoken replacetokens replacevalue report rmic
syn keyword antElement display root rootfileset rpm rulespath script searchpath section selector sequential
syn keyword antElement display serverdeploy setproxy signjar size sleep socket soscheckin soscheckout sosget
syn keyword antElement display soslabel sound source sourcepath splash sql src srcfile srcfilelist srcfiles
syn keyword antElement display srcfileset stripjavacomments striplinebreaks striplinecomments style
syn keyword antElement display substitution success support sysproperty tabstospaces tag taglet tailfilter tar
syn keyword antElement display tarfileset target targetfile targetfilelist targetfileset taskdef tempfile test
syn keyword antElement display testlet title to token touch transaction translate triggers tstamp typedef unjar
syn keyword antElement display untar unwar unzip uptodate url user vssadd vsscheckin vsscheckout vsscp
syn keyword antElement display vsscreate vssget vsshistory vsslabel waitfor war wasclasspath webapp webinf
syn keyword antElement display weblogic weblogictoplink websphere wlclasspath wljspc wsdltodotnet xmlcatalog
syn keyword antElement display xmlproperty xmlvalidate xslt zip zipfileset zipgroupfileset

hi def link antElement Statement

let b:current_syntax = "ant"

let &cpo = s:ant_cpo_save
unlet s:ant_cpo_save

" vim: ts=8
