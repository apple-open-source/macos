" Vim filetype plugin file
" Language:	html
" Maintainer:	Dan Sharp <dwsharp at hotmail dot com>
" Last Changed: 2003 Apr 16
" URL:		http://mywebpage.netscape.com/sharppeople/vim/ftplugin

if exists("b:did_ftplugin") | finish | endif
let b:did_ftplugin = 1

setlocal commentstring=<!--%s-->

" HTML:  thanks to Johannes Zellner.
if exists("loaded_matchit") && !exists("b:match_words")
    let b:match_ignorecase = 1
    let b:match_skip = 's:Comment'
    let b:match_words = '<:>,' .
    \ '<\@<=[ou]l[^>]*\%(>\|$\):<\@<=li>:<\@<=/[ou]l>,' .
    \ '<\@<=\([^/][^ \t>]*\)[^>]*\%(>\|$\):<\@<=/\1>'
endif

" Change the :browse e filter to primarily show HTML-related files.
if has("gui_win32") && !exists("b:browsefilter")
    let  b:browsefilter="HTML Files (*.html,*.htm)\t*.html\n" .
		\	"JavaScript Files (*.js)\t*.js\n" .
		\	"Cascading StyleSheets (*.css)\t*.css\n" .
		\	"All Files (*.*)\t*.*\n"
endif
