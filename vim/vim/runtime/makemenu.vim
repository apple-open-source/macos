" Script to define the syntax menu in synmenu.vim
" Maintainer:	Bram Moolenaar <Bram@vim.org>
" Last Change:	2003 May 14

" This is used by "make menu" in the src directory.
edit <sfile>:p:h/synmenu.vim

/The Start Of The Syntax Menu/+1,/The End Of The Syntax Menu/-1d
let s:lnum = line(".") - 1
call append(s:lnum, "")
let s:lnum = s:lnum + 1

" Use the SynMenu command and function to define all menu entries
command! -nargs=* SynMenu call <SID>Syn(<q-args>)

let s:cur_menu_name = ""
let s:cur_menu_nr = 0
let s:cur_menu_item = 0
let s:cur_menu_char = ""

fun! <SID>Syn(arg)
  " isolate menu name: until the first dot
  let i = match(a:arg, '\.')
  let menu_name = strpart(a:arg, 0, i)
  let r = strpart(a:arg, i + 1, 999)
  " isolate submenu name: until the colon
  let i = match(r, ":")
  let submenu_name = strpart(r, 0, i)
  " after the colon is the syntax name
  let syntax_name = strpart(r, i + 1, 999)

  if s:cur_menu_name != menu_name
    let s:cur_menu_name = menu_name
    let s:cur_menu_nr = s:cur_menu_nr + 10
    let s:cur_menu_item = 100
    let s:cur_menu_char = submenu_name[0]
  else
    " When starting a new letter, insert a menu separator.
    let c = submenu_name[0]
    if c != s:cur_menu_char
      exe 'an 50.' . s:cur_menu_nr . '.' . s:cur_menu_item . ' &Syntax.' . menu_name . ".-" . c . '- <nul>'
      let s:cur_menu_item = s:cur_menu_item + 10
      let s:cur_menu_char = c
    endif
  endif
  call append(s:lnum, 'an 50.' . s:cur_menu_nr . '.' . s:cur_menu_item . ' &Syntax.' . menu_name . "." . submenu_name . ' :cal SetSyn("' . syntax_name . '")<CR>')
  let s:cur_menu_item = s:cur_menu_item + 10
  let s:lnum = s:lnum + 1
endfun

SynMenu AB.Abaqus:abaqus
SynMenu AB.ABC\ music\ notation:abc
SynMenu AB.ABEL:abel
SynMenu AB.AceDB\ model:acedb
SynMenu AB.Ada:ada
SynMenu AB.AfLex:aflex
SynMenu AB.Altera\ AHDL:ahdl
SynMenu AB.Amiga\ DOS:amiga
SynMenu AB.AMPL:ampl
SynMenu AB.Ant\ build\ file:ant
SynMenu AB.ANTLR:antlr
SynMenu AB.Apache\ config:apache
SynMenu AB.Apache-style\ config:apachestyle
SynMenu AB.Applix\ ELF:elf
SynMenu AB.Arc\ Macro\ Language:aml
SynMenu AB.ASP\ with\ VBScript:aspvbs
SynMenu AB.ASP\ with\ Perl:aspperl
SynMenu AB.Assembly.680x0:asm68k
SynMenu AB.Assembly.GNU:asm
SynMenu AB.Assembly.GNU\ H-8300:asmh8300
SynMenu AB.Assembly.Intel\ IA-64:ia64
SynMenu AB.Assembly.Microsoft:masm
SynMenu AB.Assembly.Netwide:nasm
SynMenu AB.Assembly.PIC:pic
SynMenu AB.Assembly.Turbo:tasm
SynMenu AB.Assembly.Z-80:z8a
SynMenu AB.ASN\.1:asn
SynMenu AB.Atlas:atlas
SynMenu AB.Automake:automake
SynMenu AB.Avenue:ave
SynMenu AB.Awk:awk
SynMenu AB.AYacc:ayacc
SynMenu AB.B:b
SynMenu AB.Baan:baan
SynMenu AB.BASIC:basic
SynMenu AB.BC\ calculator:bc
SynMenu AB.BDF\ font:bdf
SynMenu AB.BibTeX:bib
SynMenu AB.BIND.BIND\ config:named
SynMenu AB.BIND.BIND\ zone:bindzone
SynMenu AB.Blank:blank

SynMenu CD.C:c
SynMenu CD.C++:cpp
SynMenu CD.C#:cs
SynMenu CD.CDL:cdl
SynMenu CD.Crontab:crontab
SynMenu CD.Cyn++:cynpp
SynMenu CD.Cynlib:cynlib
SynMenu CD.Cascading\ Style\ Sheets:css
SynMenu CD.Century\ Term:cterm
SynMenu CD.CHILL:ch
SynMenu CD.ChangeLog:changelog
SynMenu CD.Cheetah\ template:cheetah
SynMenu CD.Clean:clean
SynMenu CD.Clever:cl
SynMenu CD.Clipper:clipper
SynMenu CD.Cold\ Fusion:cf
SynMenu CD.Config.Cfg\ Config\ file:cfg
SynMenu CD.Config.Generic\ Config\ file:conf
SynMenu CD.Config.Configure\.in:config
SynMenu CD.Ctrl-H:ctrlh
SynMenu CD.Cobol:cobol
SynMenu CD.CSP:csp
SynMenu CD.CUPL.CUPL:cupl
SynMenu CD.CUPL.Simulation:cuplsim
SynMenu CD.CVS.commit\ file:cvs
SynMenu CD.CVS.cvsrc:cvsrc
SynMenu CD.Debian.Debian\ ChangeLog:debchangelog
SynMenu CD.Debian.Debian\ Control:debcontrol
SynMenu CD.Diff:diff
SynMenu CD.Digital\ Command\ Lang:dcl
SynMenu CD.DNS/BIND\ zone:dns
SynMenu CD.DocBook:docbk
SynMenu CD.Dot:dot
SynMenu CD.Dracula:dracula
SynMenu CD.DSSSL:dsl
SynMenu CD.DTD:dtd
SynMenu CD.DTML\ (Zope):dtml
SynMenu CD.Dylan.Dylan:dylan
SynMenu CD.Dylan.Dylan\ interface:dylanintr
SynMenu CD.Dylan.Dylan\ lid:dylanlid

SynMenu EFG.EDIF:edif
SynMenu EFG.Eiffel:eiffel
SynMenu EFG.Elm\ filter\ rules:elmfilt
SynMenu EFG.Embedix\ Component\ Description:ecd
SynMenu EFG.ERicsson\ LANGuage:erlang
SynMenu EFG.ESQL-C:esqlc
SynMenu EFG.Essbase\ script:csc
SynMenu EFG.Eterm\ config:eterm
SynMenu EFG.Exim\ conf:exim
SynMenu EFG.Expect:expect
SynMenu EFG.Exports:exports
SynMenu EFG.Fetchmail:fetchmail
SynMenu EFG.Focus\ Executable:focexec
SynMenu EFG.Focus\ Master:master
SynMenu EFG.FORM:form
SynMenu EFG.Forth:forth
SynMenu EFG.Fortran:fortran
SynMenu EFG.FoxPro:foxpro
SynMenu EFG.Fstab:fstab
SynMenu EFG.Fvwm.Fvwm\ configuration:fvwm1
SynMenu EFG.Fvwm.Fvwm2\ configuration:fvwm2
SynMenu EFG.Fvwm.Fvwm2\ configuration with M4:fvwm2m4
SynMenu EFG.GDB\ command\ file:gdb
SynMenu EFG.GDMO:gdmo
SynMenu EFG.Gedcom:gedcom
SynMenu EFG.Gkrellmrc:gkrellmrc
SynMenu EFG.GP:gp
SynMenu EFG.GPG:gpg
SynMenu EFG.Grub:grub
SynMenu EFG.GNU\ Server\ Pages:gsp
SynMenu EFG.GNUplot:gnuplot
SynMenu EFG.Groff:groff
SynMenu EFG.GTKrc:gtkrc

SynMenu HIJK.Haskell.Haskell:haskell
SynMenu HIJK.Haskell.Haskell-c2hs:chaskell
SynMenu HIJK.Haskell.Haskell-literate:lhaskell
SynMenu HIJK.Hercules:hercules
SynMenu HIJK.HTML.HTML:html
SynMenu HIJK.HTML.HTML\ with\ M4:htmlm4
SynMenu HIJK.HTML.Cheetah\ HTML\ template:htmlcheetah
SynMenu HIJK.HTML.HTML/OS:htmlos
SynMenu HIJK.Hyper\ Builder:hb
SynMenu HIJK.Icon:icon
SynMenu HIJK.IDL\Generic\ IDL:idl
SynMenu HIJK.IDL\Microsoft\ IDL:msidl
SynMenu HIJK.Indent\ profile:indent
SynMenu HIJK.Inform:inform
SynMenu HIJK.Informix\ 4GL:fgl
SynMenu HIJK.Inittab:inittab
SynMenu HIJK.Inno\ setup:iss
SynMenu HIJK.InstallShield\ script:ishd
SynMenu HIJK.Interactive\ Data\ Lang:idlang
SynMenu HIJK.IPfilter:ipfilter
SynMenu HIJK.JAL:jal
SynMenu HIJK.JAM:jam
SynMenu HIJK.Jargon:jargon
SynMenu HIJK.Java.Java:java
SynMenu HIJK.Java.JavaCC:javacc
SynMenu HIJK.Java.Java\ Server\ Pages:jsp
SynMenu HIJK.Java.Java\ Properties:jproperties
SynMenu HIJK.JavaScript:javascript
SynMenu HIJK.Jess:jess
SynMenu HIJK.Jgraph:jgraph
SynMenu HIJK.KDE\ script:kscript
SynMenu HIJK.Kimwitu++:kwt
SynMenu HIJK.KixTart:kix

SynMenu L-Ma.Lace:lace
SynMenu L-Ma.LamdaProlog:lprolog
SynMenu L-Ma.Latte:latte
SynMenu L-Ma.Lex:lex
SynMenu L-Ma.LFTP\ config:lftp
SynMenu L-Ma.LifeLines\ script:lifelines
SynMenu L-Ma.Lilo:lilo
SynMenu L-Ma.Lisp:lisp
SynMenu L-Ma.Lite:lite
SynMenu L-Ma.Logtalk:logtalk
SynMenu L-Ma.LOTOS:lotos
SynMenu L-Ma.LotusScript:lscript
SynMenu L-Ma.Lout:lout
SynMenu L-Ma.LPC:lpc
SynMenu L-Ma.Lua:lua
SynMenu L-Ma.Lynx\ Style:lss
SynMenu L-Ma.Lynx\ config:lynx
SynMenu L-Ma.M4:m4
SynMenu L-Ma.MaGic\ Point:mgp
SynMenu L-Ma.Mail:mail
SynMenu L-Ma.Mailcap:mailcap
SynMenu L-Ma.Makefile:make
SynMenu L-Ma.MakeIndex:ist
SynMenu L-Ma.Man\ page:man
SynMenu L-Ma.Maple\ V:maple
SynMenu L-Ma.Mason:mason
SynMenu L-Ma.Mathematica:mma
SynMenu L-Ma.Matlab:matlab

SynMenu Me-NO.MEL\ (for\ Maya):mel
SynMenu Me-NO.Metafont:mf
SynMenu Me-NO.MetaPost:mp
SynMenu Me-NO.MMIX:mmix
SynMenu Me-NO.Modconf:modconf
SynMenu Me-NO.Model:model
SynMenu Me-NO.Modsim\ III:modsim3
SynMenu Me-NO.Modula\ 2:modula2
SynMenu Me-NO.Modula\ 3:modula3
SynMenu Me-NO.Monk:monk
SynMenu Me-NO.MOO:moo
SynMenu Me-NO.MS-DOS/Windows.4DOS\ \.bat\ file:btm
SynMenu Me-NO.MS-DOS/Windows.\.bat\/\.cmd\ file:dosbatch
SynMenu Me-NO.MS-DOS/Windows.\.ini\ file:dosini
SynMenu Me-NO.MS-DOS/Windows.Module\ Definition:def
SynMenu Me-NO.MS-DOS/Windows.Registry:registry
SynMenu Me-NO.MS-DOS/Windows.Resource\ file:rc
SynMenu Me-NO.Msql:msql
SynMenu Me-NO.MUSHcode:mush
SynMenu Me-NO.Muttrc:muttrc
SynMenu Me-NO.Nastran\ input/DMAP:nastran
SynMenu Me-NO.Natural:natural
SynMenu Me-NO.Novell\ NCF\ batch:ncf
SynMenu Me-NO.Not\ Quite\ C\ (LEGO):nqc
SynMenu Me-NO.Nroff:nroff
SynMenu Me-NO.NSIS\ script:nsis
SynMenu Me-NO.Objective\ C:objc
SynMenu Me-NO.OCAML:ocaml
SynMenu Me-NO.Occam:occam
SynMenu Me-NO.Omnimark:omnimark
SynMenu Me-NO.OpenROAD:openroad
SynMenu Me-NO.Open\ Psion\ Lang:opl
SynMenu Me-NO.Oracle\ config:ora

SynMenu PQ.Palm\ resource\ compiler:pilrc
SynMenu PQ.PApp:papp
SynMenu PQ.Pascal:pascal
SynMenu PQ.PCCTS:pccts
SynMenu PQ.PPWizard:ppwiz
SynMenu PQ.Perl.Perl:perl
SynMenu PQ.Perl.Perl\ POD:pod
SynMenu PQ.Perl.Perl\ XS:xs
SynMenu PQ.PHP\ 3-4:php
SynMenu PQ.Phtml\ (PHP\ 2):phtml
SynMenu PQ.Pike:pike
SynMenu PQ.Pine\ RC:pine
SynMenu PQ.PL/M:plm
SynMenu PQ.PL/SQL:plsql
SynMenu PQ.PLP:plp
SynMenu PQ.PO\ (GNU\ gettext):po
SynMenu PQ.Postfix\ main\ config:pfmain
SynMenu PQ.PostScript:postscr
SynMenu PQ.PostScript\ Printer\ Description:ppd
SynMenu PQ.Povray\ scene\ descr:pov
SynMenu PQ.Povray\ configuration:povini
SynMenu PQ.Printcap:pcap
SynMenu PQ.Procmail:procmail
SynMenu PQ.Product\ Spec\ File:psf
SynMenu PQ.Progress:progress
SynMenu PQ.Prolog:prolog
SynMenu PQ.Purify\ log:purifylog
SynMenu PQ.Python:python
SynMenu PQ.Quake:quake

SynMenu R-Sg.R:r
SynMenu R-Sg.Radiance:radiance
SynMenu R-Sg.Ratpoison:ratpoison
SynMenu R-Sg.Readline\ config:readline
SynMenu R-Sg.RCS.RCS\ log\ output:rcslog
SynMenu R-Sg.RCS.RCS\ file:rcs
SynMenu R-Sg.Rebol:rebol
SynMenu R-Sg.Remind:remind
SynMenu R-Sg.Renderman.Renderman\ Shader\ Lang:sl
SynMenu R-Sg.Renderman.Renderman\ Interface\ Bytestream:rib
SynMenu R-Sg.Resolv\.conf:resolv
SynMenu R-Sg.Rexx:rexx
SynMenu R-Sg.Robots\.txt:robots
SynMenu R-Sg.RockLinux\ package\ desc\.:desc
SynMenu R-Sg.Rpcgen:rpcgen
SynMenu R-Sg.RPL/2:rpl
SynMenu R-Sg.RTF:rtf
SynMenu R-Sg.Ruby:ruby
SynMenu R-Sg.S-Lang:slang
SynMenu R-Sg.Samba\ config:samba
SynMenu R-Sg.SAS:sas
SynMenu R-Sg.Sather:sather
SynMenu R-Sg.Scheme:scheme
SynMenu R-Sg.Scilab:scilab
SynMenu R-Sg.Screen\ RC:screen
SynMenu R-Sg.SDL:sdl
SynMenu R-Sg.Sed:sed
SynMenu R-Sg.Sendmail\.cf:sm
SynMenu R-Sg.Send-pr:sendpr
SynMenu R-Sg.SGML.SGML\ catalog:catalog
SynMenu R-Sg.SGML.SGML\ DTD:sgml
SynMenu R-Sg.SGML.SGML\ Declaration:sgmldecl
SynMenu R-Sg.SGML.SGML-linuxdoc:sgmllnx

SynMenu Sh-S.Shell\ script.sh\ and\ ksh:sh
SynMenu Sh-S.Shell\ script.csh:csh
SynMenu Sh-S.Shell\ script.zsh:zsh
SynMenu Sh-S.SiCAD:sicad
SynMenu Sh-S.Simula:simula
SynMenu Sh-S.Sinda.Sinda\ compare:sindacmp
SynMenu Sh-S.Sinda.Sinda\ input:sinda
SynMenu Sh-S.Sinda.Sinda\ output:sindaout
SynMenu Sh-S.SKILL.SKILL:skill
SynMenu Sh-S.SKILL.SKILL\ for\ Diva:diva
SynMenu Sh-S.SLRN.Slrn\ rc:slrnrc
SynMenu Sh-S.SLRN.Slrn\ score:slrnsc
SynMenu Sh-S.SmallTalk:st
SynMenu Sh-S.Smarty\ Templates:smarty
SynMenu Sh-S.SMIL:smil
SynMenu Sh-S.SMITH:smith
SynMenu Sh-S.SNMP\ MIB:mib
SynMenu Sh-S.SNNS.SNNS\ network:snnsnet
SynMenu Sh-S.SNNS.SNNS\ pattern:snnspat
SynMenu Sh-S.SNNS.SNNS\ result:snnsres
SynMenu Sh-S.Snobol4:snobol4
SynMenu Sh-S.Snort\ Configuration:hog
SynMenu Sh-S.SPEC\ (Linux\ RPM):spec
SynMenu Sh-S.Specman:specman
SynMenu Sh-S.Spice:spice
SynMenu Sh-S.Spyce:spyce
SynMenu Sh-S.Speedup:spup
SynMenu Sh-S.Splint:splint
SynMenu Sh-S.Squid\ config:squid
SynMenu Sh-S.SQL.MySQL:mysql
SynMenu Sh-S.SQL.SQL:sql
SynMenu Sh-S.SQL.SQL\ Forms:sqlforms
SynMenu Sh-S.SQL.SQLJ:sqlj
SynMenu Sh-S.SQR:sqr
SynMenu Sh-S.Ssh.ssh_config:sshconfig
SynMenu Sh-S.Ssh.sshd_config:sshdconfig
SynMenu Sh-S.Standard\ ML:sml
SynMenu Sh-S.Stored\ Procedures:stp
SynMenu Sh-S.Strace:strace
SynMenu Sh-S.Subversion\ commit:svn

SynMenu TUV.TADS:tads
SynMenu TUV.Tags:tags
SynMenu TUV.TAK.TAK\ compare:takcmp
SynMenu TUV.TAK.TAK\ input:tak
SynMenu TUV.TAK.TAK\ output:takout
SynMenu TUV.Tcl/Tk:tcl
SynMenu TUV.TealInfo:tli
SynMenu TUV.Telix\ Salt:tsalt
SynMenu TUV.Termcap/Printcap:ptcap
SynMenu TUV.Terminfo:terminfo
SynMenu TUV.TeX.TeX:tex
SynMenu TUV.TeX.TeX\ configuration:texmf
SynMenu TUV.TeX.Texinfo:texinfo
SynMenu TUV.TF\ mud\ client:tf
SynMenu TUV.Tidy\ configuration:tidy
SynMenu TUV.Tilde:tilde
SynMenu TUV.Trasys\ input:trasys
SynMenu TUV.TSS.Command\ Line:tsscl
SynMenu TUV.TSS.Geometry:tssgm
SynMenu TUV.TSS.Optics:tssop
SynMenu TUV.UIT/UIL:uil
SynMenu TUV.UnrealScript:uc
SynMenu TUV.Valgrind:valgrind
SynMenu TUV.Verilog\ HDL:verilog
SynMenu TUV.Vgrindefs:vgrindefs
SynMenu TUV.VHDL:vhdl
SynMenu TUV.Vim.Vim\ help\ file:help
SynMenu TUV.Vim.Vim\ script:vim
SynMenu TUV.Vim.Viminfo\ file:viminfo
SynMenu TUV.Virata\ config:virata
SynMenu TUV.Visual\ Basic:vb
SynMenu TUV.VRML:vrml
SynMenu TUV.VSE\ JCL:vsejcl

SynMenu WXYZ.WEB.CWEB:cweb
SynMenu WXYZ.WEB.WEB:web
SynMenu WXYZ.WEB.WEB\ Changes:change
SynMenu WXYZ.Webmacro:webmacro
SynMenu WXYZ.Website\ MetaLanguage:wml
SynMenu WXYZ.wDiff:wdiff
SynMenu WXYZ.Wget\ config:wget
SynMenu WXYZ.Whitespace\ (add):whitespace
SynMenu WXYZ.WildPackets\ EtherPeek\ Decoder:dcd
SynMenu WXYZ.WinBatch/Webbatch:winbatch
SynMenu WXYZ.Windows\ Scripting\ Host:wsh
SynMenu WXYZ.WvDial:wvdial
SynMenu WXYZ.X\ Keyboard\ Extension:xkb
SynMenu WXYZ.X\ Pixmap:xpm
SynMenu WXYZ.X\ Pixmap\ (2):xpm2
SynMenu WXYZ.X\ resources:xdefaults
SynMenu WXYZ.Xmodmap:xmodmap
SynMenu WXYZ.Xmath:xmath
SynMenu WXYZ.XML:xml
SynMenu WXYZ.XML\ Schema\ (XSD):xsd
SynMenu WXYZ.Xslt:xslt
SynMenu WXYZ.XXD\ hex\ dump:xxd
SynMenu WXYZ.XFree86\ Config:xf86conf
SynMenu WXYZ.Yacc:yacc

call append(s:lnum, "")

wq
