" Vim filetype plugin file
" Language:	aspvbs
" Maintainer:	Dan Sharp <dwsharp at hotmail dot com>
" Last Changed: 2002 Nov 26
" URL:		http://mywebpage.netscape.com/sharppeople/vim/ftplugin

if exists("b:did_ftplugin") | finish | endif

runtime! ftplugin/html.vim ftplugin/html_*.vim ftplugin/html/*.vim

" ASP:  Active Server Pages (with Visual Basic Script)
" thanks to Gontran BAERTS
if exists("loaded_matchit") && exists("b:match_words")
  let s:notend = '\%(\<end\s\+\)\@<!'
  let b:match_words =
  \ s:notend . '\<If\>:^\s\+\<Else\>:\<ElseIf\>:\<end\s\+\<if\>,' .
  \ s:notend . '\<Select\s\+\<Case\>:\<Case\>:\<Case\s\+\<Else\>:\<End\s\+\<Select\>,' .
  \ '^\s*\<Sub\>:\<End\s\+\<Sub\>,' .
  \ '^\s*\<Function\>:\<End\s\+\<Function\>,' .
  \ '\<Class\>:\<End\s\+\<Class\>,' .
  \ '^\s*\<Do\>:\<Loop\>,' .
  \ '^\s*\<For\>:\<Next\>,' .
  \ '\<While\>:\<Wend\>,' .
  \ b:match_words
endif

let b:did_ftplugin = 1

" Change the :browse e filter to primarily show ASP-related files.
if has("gui_win32") && exists("b:browsefilter")
    let  b:browsefilter="ASP Files (*.asp)\t*.asp\n" . b:browsefilter
endif
