" Vim filetype plugin file
" Language:	config
" Maintainer:	Dan Sharp <dwsharp at hotmail dot com>
" Last Changed: 2002 Nov 26
" URL:		http://mywebpage.netscape.com/sharppeople/vim/ftplugin

if exists("b:did_ftplugin") | finish | endif

runtime! ftplugin/sh.vim ftplugin/sh_*.vim ftplugin/sh/*.vim

let b:did_ftplugin = 1

" Change the :browse e filter to primarily show configure-related files.
if has("gui_win32") && !exists("b:browsefilter")
    let  b:browsefilter="Configure Scripts (configure.*)\tconfigure.*\n" .
		\	"All Files (*.*)\t*.*\n"
endif
