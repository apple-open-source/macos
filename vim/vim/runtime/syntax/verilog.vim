" Vim syntax file
" Language:	Verilog
" Maintainer:	Mun Johl <mun_johl@sierralogic.com>
" Last Update:  Mon Jun 17 13:00:58 PDT 2002

" For version 5.x: Clear all syntax items
" For version 6.x: Quit when a syntax file was already loaded
if version < 600
   syntax clear
elseif exists("b:current_syntax")
   finish
endif

" Set the local value of the 'iskeyword' option
if version >= 600
   setlocal iskeyword=@,48-57,_,192-255
else
   set iskeyword=@,48-57,_,192-255
endif

" A bunch of useful Verilog keywords
syn keyword verilogStatement   disable assign deassign force release
syn keyword verilogStatement   parameter function endfunction
syn keyword verilogStatement   always initial module endmodule or
syn keyword verilogStatement   task endtask
syn keyword verilogStatement   input output inout reg wire
syn keyword verilogStatement   posedge negedge wait
syn keyword verilogStatement   buf pullup pull0 pull1 pulldown
syn keyword verilogStatement   tri0 tri1 tri trireg
syn keyword verilogStatement   wand wor triand trior
syn keyword verilogStatement   defparam
syn keyword verilogStatement   integer real
syn keyword verilogStatement   time
syn keyword verilogStatement   and bufif0 bufif1 cmos edge event endprimitive
syn keyword verilogStatement   endspecify endtable highz0 highz1 large
syn keyword verilogStatement   macromodule medium nand nmos nor not notif0
syn keyword verilogStatement   notif1 pmos primitive rcmos rnmos rpmos
syn keyword verilogStatement   rtran rtranif0 rtranif1 scalared small specify
syn keyword verilogStatement   specparam strong0 strong1 supply0 supply1
syn keyword verilogStatement   table tran tranif0 tranif1 weak0 weak1
syn keyword verilogStatement   xnor xor
syn keyword verilogLabel       begin end fork join
syn keyword verilogConditional if else case casex casez default endcase
syn keyword verilogRepeat      forever repeat while for

syn keyword verilogTodo contained TODO

syn match   verilogOperator "[&|~><!)(*#%@+/=?:;}{,.\^\-\[\]]"

syn region  verilogComment start="/\*" end="\*/" contains=verilogTodo
syn match   verilogComment "//.*" contains=verilogTodo

syn match   verilogGlobal "`[a-zA-Z0-9_]\+\>"
syn match   verilogGlobal "$[a-zA-Z0-9_]\+\>"

syn match   verilogConstant "\<[A-Z][A-Z0-9_]\+\>"

syn match   verilogNumber "\(\<\d\+\|\)'[bB]\s*[0-1_xXzZ?]\+\>"
syn match   verilogNumber "\(\<\d\+\|\)'[oO]\s*[0-7_xXzZ?]\+\>"
syn match   verilogNumber "\(\<\d\+\|\)'[dD]\s*[0-9_xXzZ?]\+\>"
syn match   verilogNumber "\(\<\d\+\|\)'[hH]\s*[0-9a-fA-F_xXzZ?]\+\>"
syn match   verilogNumber "\<[+-]\=[0-9_]\+\(\.[0-9_]*\|\)\(e[0-9_]*\|\)\>"

syn region  verilogString start=+"+ skip=+\\"+ end=+"+ contains=verilogEscape
syn match   verilogEscape +\\[nt"\\]+ contained
syn match   verilogEscape "\\\o\o\=\o\=" contained

" Directives
syn match   verilogDirective   "//\s*synopsys\>.*$"
syn region  verilogDirective   start="/\*\s*synopsys\>" end="\*/"
syn region  verilogDirective   start="//\s*synopsys dc_script_begin\>" end="//\s*synopsys dc_script_end\>"

syn match   verilogDirective   "//\s*\$s\>.*$"
syn region  verilogDirective   start="/\*\s*\$s\>" end="\*/"
syn region  verilogDirective   start="//\s*\$s dc_script_begin\>" end="//\s*\$s dc_script_end\>"

"Modify the following as needed.  The trade-off is performance versus
"functionality.
syn sync lines=50

" Define the default highlighting.
" For version 5.7 and earlier: only when not done already
" For version 5.8 and later: only when an item doesn't have highlighting yet
if version >= 508 || !exists("did_verilog_syn_inits")
   if version < 508
      let did_verilog_syn_inits = 1
      command -nargs=+ HiLink hi link <args>
   else
      command -nargs=+ HiLink hi def link <args>
   endif

   " The default highlighting.
   HiLink verilogCharacter	 Character
   HiLink verilogConditional	 Conditional
   HiLink verilogRepeat		 Repeat
   HiLink verilogString		 String
   HiLink verilogTodo		 Todo
   HiLink verilogComment	 Comment
   HiLink verilogConstant	 Constant
   HiLink verilogLabel		 Label
   HiLink verilogNumber		 Number
   HiLink verilogOperator	 Special
   HiLink verilogStatement	 Statement
   HiLink verilogGlobal		 Define
   HiLink verilogDirective	 SpecialComment
   HiLink verilogEscape		 Special

   delcommand HiLink
endif

let b:current_syntax = "verilog"

" vim: ts=8
