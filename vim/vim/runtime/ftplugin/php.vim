" Vim filetype plugin file
" Language:	php
" Maintainer:	Dan Sharp <dwsharp at hotmail dot com>
" Last Changed: 2002 Nov 26
" URL:		http://mywebpage.netscape.com/sharppeople/vim/ftplugin

if exists("b:did_ftplugin") | finish | endif

runtime! ftplugin/html.vim ftplugin/html_*.vim ftplugin/html/*.vim

let b:did_ftplugin = 1

" Change the :browse e filter to primarily show PHP-related files.
if has("gui_win32") && exists("b:browsefilter")
    let  b:browsefilter="PHP Files (*.php)\t*.php\n" . b:browsefilter
endif
