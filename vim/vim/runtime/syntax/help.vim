" Vim syntax file
" Language:	Vim help file
" Maintainer:	Bram Moolenaar (Bram@vim.org)
" Last Change:	2003 Jan 20

" For version 5.x: Clear all syntax items
" For version 6.x: Quit when a syntax file was already loaded
if version < 600
  syntax clear
elseif exists("b:current_syntax")
  finish
endif

syn match helpHeadline		"^[A-Z ]\+[ ]\+\*"me=e-1
syn match helpSectionDelim	"^=\{3,}.*===$"
syn match helpSectionDelim	"^-\{3,}.*--$"
syn region helpExample		matchgroup=helpIgnore start=" >$" start="^>$" end="^[^ \t]"me=e-1 end="^<"
if has("ebcdic")
  syn match helpHyperTextJump	"\\\@<!|[^"*|]\+|"
  syn match helpHyperTextEntry	"\*[^"*|]\+\*\s"he=e-1
  syn match helpHyperTextEntry	"\*[^"*|]\+\*$"
else
  syn match helpHyperTextJump	"\\\@<!|[#-)!+-~]\+|"
  syn match helpHyperTextEntry	"\*[#-)!+-~]\+\*\s"he=e-1
  syn match helpHyperTextEntry	"\*[#-)!+-~]\+\*$"
endif
syn match helpNormal		"|.*====*|"
syn match helpVim		"Vim version [0-9.a-z]\+"
syn match helpVim		"VIM REFERENCE.*"
syn match helpOption		"'[a-z]\{2,\}'"
syn match helpOption		"'t_..'"
syn match helpHeader		".*\~$"me=e-1 nextgroup=helpIgnore
syn match helpIgnore		"." contained
syn keyword helpNote		note Note NOTE note: Note: NOTE:
syn match helpSpecial		"\<N\>"
syn match helpSpecial		"(N\>"ms=s+1
syn match helpSpecial		"\[N]"
" avoid highlighting N  N in help.txt
syn match helpSpecial		"N  N"he=s+1
syn match helpSpecial		"Nth"me=e-2
syn match helpSpecial		"N-1"me=e-2
syn match helpSpecial		"{[-a-zA-Z0-9'":%#=[\]<>.,]\+}"
syn match helpSpecial		"\s\[[-a-z^A-Z0-9_]\{2,}]"ms=s+1
syn match helpSpecial		"<[-a-zA-Z0-9_]\+>"
syn match helpSpecial		"<[SCM]-.>"
syn match helpNormal		"<---*>"
syn match helpSpecial		"\[range]"
syn match helpSpecial		"\[line]"
syn match helpSpecial		"\[count]"
syn match helpSpecial		"\[offset]"
syn match helpSpecial		"\[cmd]"
syn match helpSpecial		"\[num]"
syn match helpSpecial		"\[+num]"
syn match helpSpecial		"\[-num]"
syn match helpSpecial		"CTRL-."
syn match helpSpecial		"CTRL-Break"
syn match helpSpecial		"CTRL-PageUp"
syn match helpSpecial		"CTRL-PageDown"
syn match helpSpecial		"CTRL-Insert"
syn match helpSpecial		"CTRL-Del"
syn match helpSpecial		"CTRL-{char}"
syn region helpNotVi		start="{Vi[: ]" start="{not" start="{only" end="}" contains=helpLeadBlank,helpHyperTextJump
syn match helpLeadBlank		"^\s\+" contained

syn sync minlines=40


" Define the default highlighting.
" For version 5.7 and earlier: only when not done already
" For version 5.8 and later: only when an item doesn't have highlighting yet
if version >= 508 || !exists("did_help_syntax_inits")
  if version < 508
    let did_help_syntax_inits = 1
    command -nargs=+ HiLink hi link <args>
  else
    command -nargs=+ HiLink hi def link <args>
  endif

  HiLink helpExampleStart	helpIgnore
  HiLink helpIgnore		Ignore
  HiLink helpHyperTextJump	Subtitle
  HiLink helpHyperTextEntry	String
  HiLink helpHeadline		Statement
  HiLink helpHeader		PreProc
  HiLink helpSectionDelim	PreProc
  HiLink helpVim		Identifier
  HiLink helpExample		Comment
  HiLink helpOption		Type
  HiLink helpNotVi		Special
  HiLink helpSpecial		Special
  HiLink helpNote		Todo
  HiLink Subtitle		Identifier

  delcommand HiLink
endif

let b:current_syntax = "help"

" vim: ts=8 sw=2
