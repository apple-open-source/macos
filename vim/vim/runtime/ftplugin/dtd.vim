" Vim filetype plugin file
" Language:	dtd
" Maintainer:	Dan Sharp <dwsharp at hotmail dot com>
" Last Changed: 2003 Apr 16
" URL:		http://mywebpage.netscape.com/sharppeople/vim/ftplugin

if exists("b:did_ftplugin") | finish | endif
let b:did_ftplugin = 1

setlocal commentstring=<!--%s-->

if exists("loaded_matchit") && !exists("b:match_words")
    let b:match_words = '<!--:-->,<!:>'
endif

" Change the :browse e filter to primarily show Java-related files.
if has("gui_win32") && !exists("b:browsefilter")
    let  b:browsefilter="DTD Files (*.dtd)\t*.dtd\n" .
		\	"XML Files (*.xml)\t*.xml\n" .
		\	"All Files (*.*)\t*.*\n"
endif
