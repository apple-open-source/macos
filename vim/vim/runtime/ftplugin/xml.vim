" Vim filetype plugin file
" Language:	xml
" Maintainer:	Dan Sharp <dwsharp at hotmail dot com>
" Last Changed: 2003 Apr 16
" URL:		http://mywebpage.netscape.com/sharppeople/vim/ftplugin

if exists("b:did_ftplugin") | finish | endif
let b:did_ftplugin = 1

setlocal commentstring=<!--%s-->

" XML:  thanks to Johannes Zellner and Akbar Ibrahim
" - case sensitive
" - don't match empty tags <fred/>
" - match <!--, --> style comments (but not --, --)
" - match <!, > inlined dtd's. This is not perfect, as it
"   gets confused for example by
"       <!ENTITY gt ">">
if exists("loaded_matchit") && !exists("b:match_words")
    let b:match_ignorecase=0
    let b:match_words =
     \  '<:>,' .
     \  '<\@<=!\[CDATA\[:]]>,'.
     \  '<\@<=!--:-->,'.
     \  '<\@<=?\k\+:?>,'.
     \  '<\@<=\([^ \t>/]\+\)\%(\s\+[^>]*\%([^/]>\|$\)\|>\|$\):<\@<=/\1>,'.
     \  '<\@<=\%([^ \t>/]\+\)\%(\s\+[^/>]*\|$\):/>'
endif

" Change the :browse e filter to primarily show xml-related files.
if has("gui_win32") && !exists("b:browsefilter")
    let  b:browsefilter="XML Files (*.xml)\t*.xml\n" .
		\	"DTD Files (*.dtd)\t*.dtd\n" .
		\	"All Files (*.*)\t*.*\n"
endif
